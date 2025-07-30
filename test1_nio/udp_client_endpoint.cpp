#include "udp_client_endpoint.h"
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <fcntl.h>

UdpClientEndpoint::UdpClientEndpoint(const std::string& ip, uint16_t port) 
    : serverIp_(ip), serverPort_(port) {
    serverAddr_.sin_family = AF_INET;
    serverAddr_.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr_.sin_addr);
}

UdpClientEndpoint::~UdpClientEndpoint() {
    close();
}

void UdpClientEndpoint::send(const uint8_t* data, size_t len) {
    ssize_t sent = sendto(clientFd_, data, len, 0, 
                         (sockaddr*)&serverAddr_, sizeof(serverAddr_));
    if (sent < 0) {
        log("Sendto failed: " + std::string(strerror(errno)));
    } else if (static_cast<size_t>(sent) != len) {
        log("Incomplete send: " + std::to_string(sent) + "/" + 
            std::to_string(len) + " bytes");
    }
}

std::string UdpClientEndpoint::getInfo() const {
    return "UDPClient:" + serverIp_ + ":" + std::to_string(serverPort_);
}

bool UdpClientEndpoint::connect() {
    clientFd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientFd_ < 0) {
        log("Socket creation failed: " + std::string(strerror(errno)));
        return false;
    }
    
    // Set non-blocking
    int flags = fcntl(clientFd_, F_GETFL, 0);
    fcntl(clientFd_, F_SETFL, flags | O_NONBLOCK);
    
    log("UDP client ready to send to " + serverIp_ + ":" + 
        std::to_string(serverPort_));
    return true;
}

void UdpClientEndpoint::disconnect() {
    if (clientFd_ != -1) {
        ::close(clientFd_);
        clientFd_ = -1;
    }
}

void UdpClientEndpoint::run() {
    FD_ZERO(&readFds_);
    FD_SET(clientFd_, &readFds_);
    
    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    int activity = select(clientFd_ + 1, &readFds_, nullptr, nullptr, &timeout);
    
    if (activity < 0 && errno != EINTR) {
        log("Select error: " + std::string(strerror(errno)));
        return;
    }
    
    if (activity == 0) {
        // Timeout, nothing to do
        return;
    }
    
    if (FD_ISSET(clientFd_, &readFds_)) {
        uint8_t buffer[65535];
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);
        
        ssize_t bytesRead = recvfrom(clientFd_, buffer, sizeof(buffer), 0,
                                    (sockaddr*)&from, &fromLen);
        
        if (bytesRead < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                log("Recvfrom error: " + std::string(strerror(errno)));
            }
            return;
        }
        
        if (bytesRead > 0 && dataCallback_) {
 // 记录接收到的数据（带时间戳和线程ID）
    LOG_BINARY("UDP Client Received ", buffer, bytesRead);
            dataCallback_(buffer, bytesRead);
        }
    }
}