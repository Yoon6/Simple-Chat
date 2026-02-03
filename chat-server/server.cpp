#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <chrono> // For timing
#include <fstream> // For file I/O
#include <iomanip> // For setprecision
#include "chatPacket.h"

constexpr const int PORT = 12345;
constexpr const int BUFFER_SIZE = 65535; // Increased to max UDP packet size for video

int main() {
    int server_fd, client_fd, udp_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    auto joinMsg = ChatPacket(-1, ChatType::SYSTEM, "New user joined").toJsonString();
    auto welcomeMsg = ChatPacket(-1, ChatType::SYSTEM, "Welcome to the Simple Chat!").toJsonString();
    auto leaveMsg = ChatPacket(-1, ChatType::SYSTEM, "User left").toJsonString();

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return 1;
    }

    // Forcefully attach socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }

    // Listen
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Chat Server started on port " << PORT << "..." << std::endl;

    // Create UDP socket
    if ((udp_fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0) {
        perror("socket failed");
        return 1;
    }

    // Bind UDP socket
    if (bind(udp_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }

    fd_set readfds;
    std::vector<int> client_sockets;
    std::vector<struct sockaddr_in> udp_clients; // List of known UDP clients

    // Performance Monitoring Variables
    long long packetCount = 0;
    long long byteCount = 0;
    auto lastCheckTime = std::chrono::steady_clock::now();
    std::ofstream statsFile("server_stats.csv", std::ios::app);
    if (statsFile.is_open()) {
        statsFile << "Timestamp,PPS,Mbps" << std::endl; // Header
    } 

    while (true) {
        // --- Periodic Performance Check ---
        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = currentTime - lastCheckTime;
        if (elapsed.count() >= 1.0) {
            double pps = packetCount / elapsed.count();
            double mbps = (byteCount * 8.0) / (1000000.0 * elapsed.count());
            
            // Console Output
            std::cout << "[Stats] PPS: " << std::fixed << std::setprecision(1) << pps 
                      << ", Bandwidth: " << std::setprecision(2) << mbps << " Mbps" << std::endl;
            
            // CSV Output
            if (statsFile.is_open()) {
                auto now = std::chrono::system_clock::now();
                std::time_t now_c = std::chrono::system_clock::to_time_t(now);
                statsFile << now_c << "," << pps << "," << mbps << std::endl;
                statsFile.flush(); // Ensure write immediately
            }

            // Reset
            packetCount = 0;
            byteCount = 0;
            lastCheckTime = currentTime;
        }
        // ----------------------------------

        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(udp_fd, &readfds); // Add UDP socket to set
        
        int max_sd = std::max(server_fd, udp_fd); // Initialize max_sd

        for (int sd : client_sockets) {
            FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Wait for an activity on one of the sockets (100ms timeout for stats update)
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms
        int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if ((activity < 0) && (errno != EINTR)) {
            std::cout << "select error" << std::endl;
        }

        // Handle UDP Activity (Video Data)
        if (FD_ISSET(udp_fd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int len = recvfrom(udp_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_addr_len);

            if (len > 0) {
                // Update Stats
                packetCount++;
                byteCount += len;

                // Check if this is a new UDP client
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
                    std::cout << "New UDP Client registered: " << inet_ntoa(client_addr.sin_addr) 
                              << ":" << ntohs(client_addr.sin_port) << std::endl;
                }

                // Relay (Broadcast) to all OTHER clients
                for (const auto& target_addr : udp_clients) {
                    // Skip sender
                    if (target_addr.sin_addr.s_addr == client_addr.sin_addr.s_addr &&
                        target_addr.sin_port == client_addr.sin_port) {
                        continue;
                    }
                    sendto(udp_fd, buffer, len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
                }
            }
        }

        // Incoming connection (TCP)
        if (FD_ISSET(server_fd, &readfds)) {
            if ((client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
                perror("accept");
            } else {
                std::cout << "New connection: " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << std::endl;
                
                // Broadcast new user joined
                for(int existing_sd : client_sockets) {
                    send(existing_sd, joinMsg.c_str(), joinMsg.length(), 0);
                }

                client_sockets.push_back(client_fd);
                
                // Optional: Send welcome message
                send(client_fd, welcomeMsg.c_str(), welcomeMsg.length(), 0);
            }
        }

        // IO operation on some other socket
        for (auto it = client_sockets.begin(); it != client_sockets.end(); ) {
            int sd = *it;
            if (FD_ISSET(sd, &readfds)) {
                // Check if it was for closing, and also read the incoming message
                int valread = read(sd, buffer, BUFFER_SIZE);
                if (valread == 0) {
                    // Somebody disconnected
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    std::cout << "Host disconnected: " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << std::endl;
                    close(sd);
                    it = client_sockets.erase(it);
                    
                    // Broadcast disconnection
                    for(int other_sd : client_sockets) {
                        send(other_sd, leaveMsg.c_str(), leaveMsg.length(), 0);
                    }
                    continue;
                } else {
                    buffer[valread] = '\0';
                    std::string message(buffer);
                    auto chatPacket = ChatPacket(sd, ChatType::CLIENT, message).toJsonString();

                    std::cout << chatPacket << '\n';
                    // Broadcast to other clients
                    for (int other_sd : client_sockets) {
                        if (other_sd != sd) {
                            send(other_sd, chatPacket.c_str(), chatPacket.length(), 0);
                        }
                    }
                }
            }
            ++it;
        }
    }

    return 0;
}
