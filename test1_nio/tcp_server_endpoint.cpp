#include "tcp_server_endpoint.h"
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <fcntl.h>
#include <arpa/inet.h>
#include <vector>
#include <algorithm>

TcpServerEndpoint::TcpServerEndpoint(uint16_t port) 
    : port_(port) {}

TcpServerEndpoint::~TcpServerEndpoint() {
    close();
}

void TcpServerEndpoint::send(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    // Add data to send buffer
    sendBuffer_.insert(sendBuffer_.end(), data, data + len);
}

std::string TcpServerEndpoint::getInfo() const {
    return "TCPServer:" + std::to_string(port_);
}

bool TcpServerEndpoint::connect() {
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        log("Socket creation failed: " + std::string(strerror(errno)));
        return false;
    }
    
    int opt = 1;
    if (setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        log("Setsockopt failed: " + std::string(strerror(errno)));
        ::close(serverFd_);
        serverFd_ = -1;
        return false;
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(serverFd_, (sockaddr*)&addr, sizeof(addr))) {
        log("Bind failed: " + std::string(strerror(errno)));
        ::close(serverFd_);
        serverFd_ = -1;
        return false;
    }
    
    if (listen(serverFd_, 5)) {
        log("Listen failed: " + std::string(strerror(errno)));
        ::close(serverFd_);
        serverFd_ = -1;
        return false;
    }
    
    // Set non-blocking
    int flags = fcntl(serverFd_, F_GETFL, 0);
    fcntl(serverFd_, F_SETFL, flags | O_NONBLOCK);
    
    log("Server started on port " + std::to_string(port_));
    return true;
}

void TcpServerEndpoint::disconnect() {
    // Close all client connections
    for (int fd : clientFds_) {
        ::close(fd);
    }
    clientFds_.clear();
    
    // Close server socket
    if (serverFd_ != -1) {
        ::close(serverFd_);
        serverFd_ = -1;
    }
    
    // Clear any pending close requests
    toCloseFds_.clear();
}

void TcpServerEndpoint::run() {
    // Process pending close requests
    for (int fd : toCloseFds_) {
        closeClient(fd);
    }
    toCloseFds_.clear();
    
    // Setup fd_set for select
    FD_ZERO(&readFds_);
    FD_SET(serverFd_, &readFds_);
    maxFd_ = serverFd_;
    
    // Add all client sockets
    for (int fd : clientFds_) {
        FD_SET(fd, &readFds_);
        if (fd > maxFd_) maxFd_ = fd;
    }
    
    // Set timeout for select
    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    int activity = select(maxFd_ + 1, &readFds_, nullptr, nullptr, &timeout);
    
    if (activity < 0) {
        if (errno == EINTR) {
            return; // Interrupted by signal, try again
        }
        log("Select error: " + std::string(strerror(errno)));
        return;
    }
    
    if (activity == 0) {
        // Timeout, check for send data
        std::lock_guard<std::mutex> lock(sendMutex_);
        if (!sendBuffer_.empty()) {
            // Send to all connected clients
            for (int fd : clientFds_) {
                ssize_t sent = ::send(fd, sendBuffer_.data(), sendBuffer_.size(), MSG_NOSIGNAL);
                if (sent < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        log("Send error to client " + std::to_string(fd) + ": " + 
                            std::string(strerror(errno)));
                        // Queue for close
                        toCloseFds_.push_back(fd);
                    }
                }
            }
            sendBuffer_.clear();
        }
        return;
    }
    
    // Check for new connection
    if (FD_ISSET(serverFd_, &readFds_)) {
        handleNewConnection();
    }
    
    // Create a copy of client list to avoid iterator invalidation
    std::vector<int> currentClients = clientFds_;
    
    // Check for data from clients
    for (int fd : currentClients) {
        // Skip if this fd was closed in this iteration
        if (std::find(toCloseFds_.begin(), toCloseFds_.end(), fd) != toCloseFds_.end()) {
            continue;
        }
        
        if (FD_ISSET(fd, &readFds_)) {
            handleClientData(fd);
        }
    }
}

void TcpServerEndpoint::handleNewConnection() {
    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    int clientFd = accept(serverFd_, (sockaddr*)&clientAddr, &addrLen);
    
    if (clientFd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            log("Accept failed: " + std::string(strerror(errno)));
        }
        return;
    }
    
    char clientIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
    log("New client connected: " + std::string(clientIp) + ":" + 
        std::to_string(ntohs(clientAddr.sin_port)));
    
    // Set non-blocking
    int flags = fcntl(clientFd, F_GETFL, 0);
    fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
    
    clientFds_.push_back(clientFd);
}

void TcpServerEndpoint::handleClientData(int clientFd) {
    uint8_t buffer[4096];
    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer), 0);
    
    if (bytesRead <= 0) {
        if (bytesRead == 0) {
            log("Client disconnected");
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            log("Receive error: " + std::string(strerror(errno)));
        }
        
            
        // Queue this client for closing
        toCloseFds_.push_back(clientFd);
        return;
    }
 // 记录接收到的数据（带时间戳和线程ID）
    LOG_BINARY("TCP Server Received ", buffer, bytesRead);
    if (dataCallback_) {
        dataCallback_(buffer, bytesRead);
    }
}

void TcpServerEndpoint::closeClient(int clientFd) {
    // Close the socket
    ::close(clientFd);
    
    // Remove from client list
    auto it = std::find(clientFds_.begin(), clientFds_.end(), clientFd);
    if (it != clientFds_.end()) {
        clientFds_.erase(it);
        log("Closed client connection: " + std::to_string(clientFd));
    }
}