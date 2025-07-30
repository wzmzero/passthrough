#include "ChannelTcpServer.h"
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

ChannelTcpServer::ChannelTcpServer(int port, int maxConnections)
    : port(port), maxConnections(maxConnections) {}

ChannelTcpServer::~ChannelTcpServer() {
    stop();
}

bool ChannelTcpServer::start() {
    if (running) return true;
    
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        perror("Socket creation failed");
        return false;
    }
    
    int opt = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        close(serverFd);
        return false;
    }
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(serverFd);
        return false;
    }
    
    if (listen(serverFd, maxConnections) < 0) {
        perror("Listen failed");
        close(serverFd);
        return false;
    }
    
    running = true;
    acceptThread = std::thread(&ChannelTcpServer::acceptThreadFunc, this);
    return true;
}

void ChannelTcpServer::stop() {
    if (!running) return;
    
    running = false;
    
    // 关闭服务器套接字
    if (serverFd != -1) {
        close(serverFd);
        serverFd = -1;
    }
    
    // 关闭所有客户端连接
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto& [fd, thread] : clientThreads) {
            shutdown(fd, SHUT_RDWR);
            close(fd);
        }
    }
    
    if (acceptThread.joinable()) {
        acceptThread.join();
    }
    
    // 等待所有客户端线程结束
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto& [fd, thread] : clientThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        clientThreads.clear();
    }
}

bool ChannelTcpServer::send(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    bool allSuccess = true;
    
    for (auto& [fd, thread] : clientThreads) {
        ssize_t sent = ::send(fd, data.data(), data.size(), 0);
        if (sent != static_cast<ssize_t>(data.size())) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

void ChannelTcpServer::acceptThreadFunc() {
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    
    while (running) {
        int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &addrLen);
        if (clientFd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Accept failed");
            }
            continue;
        }
        
        // 设置非阻塞模式
        int flags = fcntl(clientFd, F_GETFL, 0);
        fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
        
        // 启动客户端处理线程
        std::lock_guard<std::mutex> lock(clientsMutex);
        clientThreads.emplace(clientFd, std::thread(&ChannelTcpServer::clientHandler, this, clientFd));
    }
}

void ChannelTcpServer::clientHandler(int clientFd) {
    std::vector<uint8_t> buffer(1024);
    
    while (running) {
        ssize_t bytesRead = recv(clientFd, buffer.data(), buffer.size(), 0);
        if (bytesRead > 0) {
            if (receiveCallback) {
                receiveCallback(std::vector<uint8_t>(buffer.begin(), buffer.begin() + bytesRead));
            }
        } else if (bytesRead == 0) {
            // 客户端断开连接
            break;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Recv error");
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 清理客户端资源
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
        
        auto it = clientThreads.find(clientFd);
        if (it != clientThreads.end()) {
            if (it->second.joinable()) {
                it->second.detach();
            }
            clientThreads.erase(it);
        }
    }
}