#include "tcp_server_endpoint.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>

TcpServerEndpoint::TcpServerEndpoint(uint16_t port) : _port(port) {}

TcpServerEndpoint::~TcpServerEndpoint() {
    close();
}

bool TcpServerEndpoint::open() {
    if (isRunning()) return true;

    // 创建服务器socket
    _serverFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (_serverFd < 0) {
        logError("Socket creation failed: " + std::string(strerror(errno)));
        return false;
    }

    // 设置端口复用
    int opt = 1;
    setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定端口
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(_port);

    if (bind(_serverFd, (sockaddr*)&address, sizeof(address)) < 0) {
        logError("Bind failed: " + std::string(strerror(errno)));
        ::close(_serverFd);
        _serverFd = -1;
        return false;
    }

    // 开始监听
    if (listen(_serverFd, 5) < 0) {
        logError("Listen failed: " + std::string(strerror(errno)));
        ::close(_serverFd);
        _serverFd = -1;
        return false;
    }

    // 创建epoll实例
    _epollFd = epoll_create1(0);
    if (_epollFd < 0) {
        logError("Epoll creation failed: " + std::string(strerror(errno)));
        ::close(_serverFd);
        _serverFd = -1;
        return false;
    }

    // 添加服务器socket到epoll
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = _serverFd;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, _serverFd, &event) < 0) {
        logError("Epoll_ctl failed: " + std::string(strerror(errno)));
        ::close(_epollFd);
        ::close(_serverFd);
        _epollFd = -1;
        _serverFd = -1;
        return false;
    }

    setState(State::CONNECTED);
    startThread();
    return true;
}

void TcpServerEndpoint::close() {
    stopThread();
    
    if (_epollFd >= 0) {
        ::close(_epollFd);
        _epollFd = -1;
    }
    
    if (_serverFd >= 0) {
        ::close(_serverFd);
        _serverFd = -1;
    }
    
    for (auto& client : _clients) {
        ::close(client.first);
    }
    _clients.clear();
    
    setState(State::DISCONNECTED);
}

void TcpServerEndpoint::write(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& client : _clients) {
        if (send(client.first, data, len, MSG_NOSIGNAL) < 0) {
            logError("Send failed to client: " + std::to_string(client.first));
        }
    }
}
void TcpServerEndpoint::run() {
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
            if (events[i].data.fd == _serverFd) {
                handleNewConnection();
            } else {
                // 检查连接是否断开
                if (events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                    closeClient(events[i].data.fd);
                } else if (events[i].events & EPOLLIN) {
                    handleClientData(events[i].data.fd);
                }
            }
        }
    }
}

void TcpServerEndpoint::handleNewConnection() {
    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    int clientFd = accept4(_serverFd, (sockaddr*)&clientAddr, &addrLen, SOCK_NONBLOCK);
    
    if (clientFd < 0) {
        logError("Accept failed: " + std::string(strerror(errno)));
        return;
    }

    // 添加到epoll监控
    epoll_event event{};
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    event.data.fd = clientFd;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &event) < 0) {
        logError("Epoll_ctl add client failed: " + std::string(strerror(errno)));
        ::close(clientFd);
        return;
    }

    _clients[clientFd] = clientAddr;
    logMessage("New client connected: " + std::string(inet_ntoa(clientAddr.sin_addr)) + 
               ":" + std::to_string(ntohs(clientAddr.sin_port)));
}


void TcpServerEndpoint::handleClientData(int clientFd) {
    uint8_t buffer[4096];
    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer), 0);
    
    if (bytesRead > 0) {
        processData(buffer, bytesRead);
    } else {
        // 处理断开连接
        closeClient(clientFd);
    }
}

void TcpServerEndpoint::closeClient(int clientFd) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _clients.find(clientFd);
    if (it != _clients.end()) {
        logMessage("Client disconnected: " + 
                   std::string(inet_ntoa(it->second.sin_addr)) + 
                   ":" + std::to_string(ntohs(it->second.sin_port)));
        
        if (_epollFd >= 0) {
            epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
        }
        ::close(clientFd);
        _clients.erase(it);
    }
}