#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "chatPacket.h"

// Tell MSVC to link with ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

constexpr const int PORT = 12345;
constexpr const int BUFFER_SIZE = 65535;

int main() {
    WSADATA wsaData;
    SOCKET server_fd, udp_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    auto joinMsg = ChatPacket(-1, ChatType::SYSTEM, "New user joined").toJsonString();
    auto welcomeMsg = ChatPacket(-1, ChatType::SYSTEM, "Welcome to the Simple Chat!").toJsonString();
    auto leaveMsg = ChatPacket(-1, ChatType::SYSTEM, "User left").toJsonString();

    // 1. Create TCP Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cerr << "TCP socket failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Set TCP Options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        std::cerr << "TCP setsockopt failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind TCP
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "TCP bind failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    // Listen TCP
    if (listen(server_fd, 5) == SOCKET_ERROR) {
        std::cerr << "TCP listen failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    // 2. Create UDP Socket
    if ((udp_fd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        std::cerr << "UDP socket failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    // Set UDP Options
    if (setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        std::cerr << "UDP setsockopt failed: " << WSAGetLastError() << std::endl;
    }

    // Bind UDP
    if (bind(udp_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "UDP bind failed: " << WSAGetLastError() << std::endl;
        closesocket(udp_fd);
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    std::cout << "Chat Server (Winsock/MSVC) started on port " << PORT << "..." << std::endl;

    fd_set readfds;
    std::vector<SOCKET> client_sockets;
    std::vector<struct sockaddr_in> udp_clients;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(udp_fd, &readfds);
        
        for (SOCKET sd : client_sockets) {
            FD_SET(sd, &readfds);
        }

        // Wait for sockets
        int activity = select(0, &readfds, NULL, NULL, NULL);

        if (activity == SOCKET_ERROR) {
            std::cerr << "select error: " << WSAGetLastError() << std::endl;
            break;
        }

        // --- UDP Handling ---
        if (FD_ISSET(udp_fd, &readfds)) {
            struct sockaddr_in client_addr;
            int client_addr_len = sizeof(client_addr);
            int len = recvfrom(udp_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_addr_len);

            if (len > 0) {
                // Register / Update client
                bool is_new = true;
                for (const auto& existing_addr : udp_clients) {
                    if (existing_addr.sin_addr.s_addr == client_addr.sin_addr.s_addr &&
                        existing_addr.sin_port == client_addr.sin_port) {
                        is_new = false;
                        break;
                    }
                }
                if (is_new) {
                    udp_clients.push_back(client_addr);
                    std::cout << "[UDP] New client: " << inet_ntoa(client_addr.sin_addr) 
                              << ":" << ntohs(client_addr.sin_port) << std::endl;
                }

                // Relay
                for (const auto& target_addr : udp_clients) {
                    if (target_addr.sin_addr.s_addr == client_addr.sin_addr.s_addr &&
                        target_addr.sin_port == client_addr.sin_port) {
                        continue;
                    }
                    sendto(udp_fd, buffer, len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
                }
                // Log (optional)
                 if (udp_clients.size() > 1) {
                     std::cout << "Relayed " << len << " bytes." << std::endl;
                 }
            }
        }

        // --- TCP Handling (New Connection) ---
        if (FD_ISSET(server_fd, &readfds)) {
            SOCKET new_socket;
            if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) == INVALID_SOCKET) {
                std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            } else {
                std::cout << "[TCP] New connection: " << inet_ntoa(address.sin_addr) 
                          << ":" << ntohs(address.sin_port) << std::endl;

                for(SOCKET existing_sd : client_sockets) {
                    send(existing_sd, joinMsg.c_str(), joinMsg.length(), 0);
                }

                client_sockets.push_back(new_socket);
                send(new_socket, welcomeMsg.c_str(), welcomeMsg.length(), 0);
            }
        }

        // --- TCP Handling (Client Messages) ---
        for (auto it = client_sockets.begin(); it != client_sockets.end(); ) {
            SOCKET sd = *it;
            if (FD_ISSET(sd, &readfds)) {
                int valread = recv(sd, buffer, BUFFER_SIZE, 0);
                if (valread <= 0) {
                    getpeername(sd, (struct sockaddr*)&address, &addrlen);
                    std::cout << "[TCP] Disconnected: " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << std::endl;
                    closesocket(sd);
                    it = client_sockets.erase(it);

                    for(SOCKET other_sd : client_sockets) {
                        send(other_sd, leaveMsg.c_str(), leaveMsg.length(), 0);
                    }
                    continue;
                } else {
                    buffer[valread] = '\0';
                    std::string message(buffer);
                    auto chatPacket = ChatPacket(sd, ChatType::CLIENT, message).toJsonString();
                    std::cout << chatPacket;

                    for (SOCKET other_sd : client_sockets) {
                        if (other_sd != sd) {
                            send(other_sd, chatPacket.c_str(), chatPacket.length(), 0);
                        }
                    }
                }
            }
            ++it;
        }
    }

    closesocket(server_fd);
    closesocket(udp_fd);
    WSACleanup();
    return 0;
}
