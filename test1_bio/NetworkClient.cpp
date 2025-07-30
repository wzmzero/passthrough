#include "NetworkClient.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

NetworkClient::NetworkClient(Protocol protocol, const std::string& server_ip, int port)
    : protocol(protocol), serverIP(server_ip), serverPort(port) {
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
}

NetworkClient::~NetworkClient() {
    closeConnection();
}

bool NetworkClient::connect() {
    if (protocol == TCP) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("TCP socket creation failed");
            return false;
        }
        
         // 设置超时（5秒）
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0; 
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        
        std::cout << "Connected to TCP server at " 
                  << serverIP << ":" << serverPort << std::endl;
    } else {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            perror("UDP socket creation failed");
            return false;
        }
        
        std::cout << "UDP client ready for " 
                  << serverIP << ":" << serverPort << std::endl;
    }
    return true;
}

bool NetworkClient::sendMessage(const std::string& message) {
    if (sock < 0) return false;
    
    if (protocol == TCP) {
        return send(sock, message.c_str(), message.length(), 0) >= 0;
    } else {
        socklen_t addrLen = sizeof(serverAddr);
        return sendto(sock, message.c_str(), message.length(), 0,
                     (struct sockaddr*)&serverAddr, addrLen) >= 0;
    }
}
 
std::string NetworkClient::receiveMessage() {
    if (sock < 0) return "";
    
    char buffer[BUFFER_SIZE] = {0};
    
    if (protocol == TCP) {
        int bytes = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0) {
            return "";
        }
        return std::string(buffer, bytes);
    } else {
        struct sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);
        int bytes = recvfrom(sock, buffer, BUFFER_SIZE, 0, 
                           (struct sockaddr*)&fromAddr, &fromLen);
        if (bytes <= 0) {
            return "";
        }
        return std::string(buffer, bytes);
    }
}

void NetworkClient::closeConnection() {
    if (sock != -1) {
        close(sock);
        sock = -1;
    }
}