#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 12345
#define BUFFER_SIZE 2048

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

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

    fd_set readfds;
    std::vector<int> client_sockets;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (int sd : client_sockets) {
            FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Wait for an activity on one of the sockets
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            std::cout << "select error" << std::endl;
        }

        // Incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
                perror("accept");
            } else {
                std::cout << "New connection: " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << std::endl;
                
                // Broadcast new user joined
                std::string joinMsg = "[SYSTEM] New user joined<EOM>\n";
                for(int existing_sd : client_sockets) {
                    send(existing_sd, joinMsg.c_str(), joinMsg.length(), 0);
                }

                client_sockets.push_back(client_fd);
                
                // Optional: Send welcome message
                std::string welcome = "[SYSTEM] Welcome to the chat server!<EOM>\n";
                send(client_fd, welcome.c_str(), welcome.length(), 0);
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
                    std::string leaveMsg = "[SYSTEM]  User left<EOM>\n";
                    for(int other_sd : client_sockets) {
                        send(other_sd, leaveMsg.c_str(), leaveMsg.length(), 0);
                    }
                    continue;
                } else {
                    buffer[valread] = '\0';
                    std::string message(buffer);

                    std::cout << "client[" << sd << "] : " << message << '\n';
                    // Broadcast to other clients
                    for (int other_sd : client_sockets) {
                        if (other_sd != sd) {
                            send(other_sd, message.c_str(), message.length(), 0);
                        }
                    }
                }
            }
            ++it;
        }
    }

    return 0;
}
