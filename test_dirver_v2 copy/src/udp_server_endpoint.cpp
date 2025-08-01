#include "udp_server_endpoint.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>
#include <algorithm>

// 生成客户端唯一ID (IP:Port)
std::string UdpServerEndpoint::getClientId(const sockaddr_in& addr) const {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}

UdpServerEndpoint::UdpServerEndpoint(uint16_t port) : _port(port) {}

UdpServerEndpoint::~UdpServerEndpoint() {
    close();
}

bool UdpServerEndpoint::open() {
    if (isRunning()) return true;

    // 创建UDP socket
    _socketFd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (_socketFd < 0) {
        logError("Socket creation failed: " + std::string(strerror(errno)));
        return false;
    }

    // 绑定端口
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(_port);

    if (bind(_socketFd, (sockaddr*)&address, sizeof(address)) < 0) {
        logError("Bind failed: " + std::string(strerror(errno)));
        ::close(_socketFd);
        _socketFd = -1;
        return false;
    }

    // 创建epoll实例
    _epollFd = epoll_create1(0);
    if (_epollFd < 0) {
        logError("Epoll creation failed: " + std::string(strerror(errno)));
        ::close(_socketFd);
        _socketFd = -1;
        return false;
    }

    // 添加socket到epoll
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = _socketFd;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, _socketFd, &event) < 0) {
        logError("Epoll_ctl failed: " + std::string(strerror(errno)));
        ::close(_epollFd);
        ::close(_socketFd);
        _epollFd = -1;
        _socketFd = -1;
        return false;
    }

    // 清空客户端列表
    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        _clients.clear();
    }

    setState(State::CONNECTED);
    startThread();
    logMessage("UDP server started on port " + std::to_string(_port));
    return true;
}

void UdpServerEndpoint::close() {
    stopThread();
    
    if (_epollFd >= 0) {
        ::close(_epollFd);
        _epollFd = -1;
    }
    
    if (_socketFd >= 0) {
        ::close(_socketFd);
        _socketFd = -1;
    }
    
    // 清空客户端列表
    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        _clients.clear();
    }
    
    setState(State::DISCONNECTED);
    logMessage("UDP server closed");
}

// 广播到所有客户端
void UdpServerEndpoint::write(const uint8_t* data, size_t len) {
    // if (!isConnected()) return;
    
    std::lock_guard<std::mutex> lock(_clientsMutex);
    if (_clients.empty()) {
        logError("No clients connected, skip sending");
        return;
    }
    
    for (const auto& client : _clients) {
        if (sendto(_socketFd, data, len, 0, 
                  (sockaddr*)&client.second, sizeof(client.second)) < 0) {
            logError("Sendto failed to " + client.first + ": " + 
                     std::string(strerror(errno)));
        }
    }
}


void UdpServerEndpoint::run() {
    constexpr int MAX_EVENTS = 10;
    epoll_event events[MAX_EVENTS];
    
    while (isRunning()) {
        int numEvents = epoll_wait(_epollFd, events, MAX_EVENTS, 100);
        if (numEvents < 0) {
            if (errno != EINTR) {
                logError("Epoll_wait error: " + std::string(strerror(errno)));
            }
            continue;
        }

        for (int i = 0; i < numEvents; ++i) {
            if (events[i].data.fd == _socketFd) {
                handleData();
            }
        }
    }
}

void UdpServerEndpoint::handleData() {
    uint8_t buffer[4096];
    sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    
    ssize_t bytesRead = recvfrom(_socketFd, buffer, sizeof(buffer), 0,
                                (sockaddr*)&clientAddr, &addrLen);
    
    if (bytesRead > 0) {
        // 注册/更新客户端
        std::string clientId = getClientId(clientAddr);
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            _clients[clientId] = clientAddr;
        }
        
        // logMessage("Received data from " + clientId);
        processData(buffer, bytesRead);
    } else if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        logError("Recvfrom error: " + std::string(strerror(errno)));
    }
}