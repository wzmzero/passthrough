#include "ChannelTcpClient.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

ChannelTcpClient::ChannelTcpClient(const std::string& ip, int port)
    : serverIp(ip), serverPort(port) {}

ChannelTcpClient::~ChannelTcpClient() {
    stop();
}

bool ChannelTcpClient::start() {
    if (running) return true;
    
    if (!connectToServer()) {
        return false;
    }
    
    running = true;
    workerThread = std::thread(&ChannelTcpClient::connectionThreadFunc, this);
    return true;
}

bool ChannelTcpClient::connectToServer() {
    clientFd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientFd < 0) {
        perror("Socket creation failed");
        return false;
    }
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    
    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        close(clientFd);
        return false;
    }
    
    if (connect(clientFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr))) {
        perror("Connection failed");
        close(clientFd);
        return false;
    }
    
    // 设置非阻塞模式
    int flags = fcntl(clientFd, F_GETFL, 0);
    fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
    
    return true;
}

void ChannelTcpClient::stop() {
    if (!running) return;
    
    running = false;
    if (workerThread.joinable()) {
        workerThread.join();
    }
    
    if (clientFd != -1) {
        close(clientFd);
        clientFd = -1;
    }
}

bool ChannelTcpClient::send(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(writeMutex);
    if (clientFd < 0) return false;
    
    ssize_t sent = ::send(clientFd, data.data(), data.size(), 0);
    return sent == static_cast<ssize_t>(data.size());
}

void ChannelTcpClient::connectionThreadFunc() {
    std::vector<uint8_t> buffer(1024);
    
    while (running) {
        ssize_t bytesRead = recv(clientFd, buffer.data(), buffer.size(), 0);
        if (bytesRead > 0) {
            if (receiveCallback) {
                receiveCallback(std::vector<uint8_t>(buffer.begin(), buffer.begin() + bytesRead));
            }
        } else if (bytesRead == 0) {
            // 服务器断开连接
            break;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Recv error");
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 尝试重新连接？
    stop();
}