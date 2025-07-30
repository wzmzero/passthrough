#include "tcp_client_endpoint.h"
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <fcntl.h>

TcpClientEndpoint::TcpClientEndpoint(const std::string& ip, uint16_t port) 
    : serverIp_(ip), serverPort_(port) {
    serverAddr_.sin_family = AF_INET;
    serverAddr_.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr_.sin_addr);
}

TcpClientEndpoint::~TcpClientEndpoint() {
    close();
}

void TcpClientEndpoint::send(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    // Add data to send buffer
    sendBuffer_.insert(sendBuffer_.end(), data, data + len);
}

std::string TcpClientEndpoint::getInfo() const {
    return "TCPClient:" + serverIp_ + ":" + std::to_string(serverPort_);
}

bool TcpClientEndpoint::connect() {
    clientFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (clientFd_ < 0) {
        log("Socket creation failed: " + std::string(strerror(errno)));
        return false;
    }
    
    if (::connect(clientFd_, (sockaddr*)&serverAddr_, sizeof(serverAddr_))) {
        log("Connection failed: " + std::string(strerror(errno)));
        ::close(clientFd_);
        clientFd_ = -1;
        return false;
    }
    
    // Set non-blocking
    int flags = fcntl(clientFd_, F_GETFL, 0);
    fcntl(clientFd_, F_SETFL, flags | O_NONBLOCK);
    
    log("Connected to server");
    return true;
}

void TcpClientEndpoint::disconnect() {
    if (clientFd_ != -1) {
        ::close(clientFd_);
        clientFd_ = -1;
    }
}

void TcpClientEndpoint::run() {
    FD_ZERO(&readFds_);
    FD_SET(clientFd_, &readFds_);
    
    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    int activity = select(clientFd_ + 1, &readFds_, nullptr, nullptr, &timeout);
    
    if (activity < 0 && errno != EINTR) {
        log("Select error: " + std::string(strerror(errno)));
        status_ = DISCONNECTED;
        return;
    }
    
    if (activity == 0) {
        // Timeout, check for send data
        std::lock_guard<std::mutex> lock(sendMutex_);
        if (!sendBuffer_.empty()) {
            ssize_t sent = ::send(clientFd_, sendBuffer_.data(), sendBuffer_.size(), 0);
            if (sent < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    log("Send error: " + std::string(strerror(errno)));
                    status_ = DISCONNECTED;
                }
            } else if (static_cast<size_t>(sent) < sendBuffer_.size()) {
                // Partial send, remove sent part
                sendBuffer_.erase(sendBuffer_.begin(), sendBuffer_.begin() + sent);
            } else {
                sendBuffer_.clear();
            }
        }
        return;
    }
    
    // Handle incoming data
    if (FD_ISSET(clientFd_, &readFds_)) {
        uint8_t buffer[4096];
        ssize_t bytesRead = recv(clientFd_, buffer, sizeof(buffer), 0);
        
        if (bytesRead <= 0) {
            if (bytesRead == 0) {
                log("Server disconnected");
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                log("Receive error: " + std::string(strerror(errno)));
            }
            status_ = DISCONNECTED;
            return;
        }
 // 记录接收到的数据（带时间戳和线程ID）
    LOG_BINARY("TCP Client Received ", buffer, bytesRead);
        if (dataCallback_) {
            dataCallback_(buffer, bytesRead);
        }
    }
}