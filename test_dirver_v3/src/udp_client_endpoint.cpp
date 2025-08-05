#include "udp_client_endpoint.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>

UdpClientEndpoint::UdpClientEndpoint(const std::string& host, uint16_t port)
    : _host(host), _port(port) {
    memset(&_serverAddr, 0, sizeof(_serverAddr));
    _serverAddr.sin_family = AF_INET;
    _serverAddr.sin_port = htons(_port);
    
    if (inet_pton(AF_INET, _host.c_str(), &_serverAddr.sin_addr) <= 0) {
        throw std::runtime_error("Invalid address: " + _host);
    }
}

UdpClientEndpoint::~UdpClientEndpoint() {
    close();
}

bool UdpClientEndpoint::open() {
    if (isRunning()) return true;
    
    // 创建UDP socket
    _socketFd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (_socketFd < 0) {
        logError("Socket creation failed: " + std::string(strerror(errno)));
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

    setState(State::CONNECTED);
    startThread();
    logMessage("UDP client connected to " + _host + ":" + std::to_string(_port));
    return true;
}

void UdpClientEndpoint::close() {
    stopThread();
    
    if (_epollFd >= 0) {
        ::close(_epollFd);
        _epollFd = -1;
    }
    
    if (_socketFd >= 0) {
        ::close(_socketFd);
        _socketFd = -1;
    }
    
    setState(State::DISCONNECTED);
    logMessage("UDP client disconnected");
}

void UdpClientEndpoint::write(const uint8_t* data, size_t len) {
    if (!isConnected()) return;
    
    std::lock_guard<std::mutex> lock(_mutex);
    if (sendto(_socketFd, data, len, 0, 
              (sockaddr*)&_serverAddr, sizeof(_serverAddr)) < 0) {
        logError("Sendto failed: " + std::string(strerror(errno)));
    }
}

void UdpClientEndpoint::run() {
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

void UdpClientEndpoint::handleData() {
    uint8_t buffer[4096];
    ssize_t bytesRead = recv(_socketFd, buffer, sizeof(buffer), 0);
    
    if (bytesRead > 0) {
        processData(buffer, bytesRead);
    } else if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        logError("Recv error: " + std::string(strerror(errno)));
    }
}