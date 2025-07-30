#include "udp_server_endpoint.h"
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <fcntl.h>

UdpServerEndpoint::UdpServerEndpoint(uint16_t port) 
    : port_(port) {}

UdpServerEndpoint::~UdpServerEndpoint() {
    close();
}

void UdpServerEndpoint::send(const uint8_t* data, size_t len) {
    if (!clientKnown_) {
        log("No client known, data dropped");
        return;
    }
    
    ssize_t sent = sendto(serverFd_, data, len, 0, 
                         (sockaddr*)&lastClient_, sizeof(lastClient_));
    if (sent < 0) {
        log("Sendto failed: " + std::string(strerror(errno)));
    } else if (static_cast<size_t>(sent) != len) {
        log("Incomplete send: " + std::to_string(sent) + "/" + 
            std::to_string(len) + " bytes");
    }
}

std::string UdpServerEndpoint::getInfo() const {
    return "UDPServer:" + std::to_string(port_);
}

bool UdpServerEndpoint::connect() {
    serverFd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverFd_ < 0) {
        log("Socket creation failed: " + std::string(strerror(errno)));
        return false;
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(serverFd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        log("Bind failed: " + std::string(strerror(errno)));
        ::close(serverFd_);
        serverFd_ = -1;
        return false;
    }
    
    // Set non-blocking
    int flags = fcntl(serverFd_, F_GETFL, 0);
    fcntl(serverFd_, F_SETFL, flags | O_NONBLOCK);
    
    log("UDP server started on port " + std::to_string(port_));
    return true;
}

void UdpServerEndpoint::disconnect() {
    if (serverFd_ != -1) {
        ::close(serverFd_);
        serverFd_ = -1;
    }
    clientKnown_ = false;
}

void UdpServerEndpoint::run() {
    FD_ZERO(&readFds_);
    FD_SET(serverFd_, &readFds_);
    
    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    int activity = select(serverFd_ + 1, &readFds_, nullptr, nullptr, &timeout);
    
    if (activity < 0 && errno != EINTR) {
        log("Select error: " + std::string(strerror(errno)));
        return;
    }
    
    if (activity == 0) {
        // Timeout, nothing to do
        return;
    }
    
    if (FD_ISSET(serverFd_, &readFds_)) {
        uint8_t buffer[65535];
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);
        
        ssize_t bytesRead = recvfrom(serverFd_, buffer, sizeof(buffer), 0,
                                    (sockaddr*)&clientAddr, &addrLen);
        
        if (bytesRead < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                log("Recvfrom error: " + std::string(strerror(errno)));
            }
            return;
        }
        
        if (bytesRead > 0) {
            // char clientIp[INET_ADDRSTRLEN];
            // inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
            // log("Received " + std::to_string(bytesRead) + " bytes from " + 
            //     std::string(clientIp) + ":" + std::to_string(ntohs(clientAddr.sin_port)));
    
    // 记录接收到的数据（带时间戳和线程ID）
        LOG_BINARY("UDP Server Received ", buffer, bytesRead);
            // Save client address for future sends
            lastClient_ = clientAddr;
            clientKnown_ = true;
            
            if (dataCallback_) {
                
                dataCallback_(buffer, bytesRead);
            }
        }
    }
}