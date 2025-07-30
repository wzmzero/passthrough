#include "tcp_client_endpoint.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>
#include <system_error>

TcpClientEndpoint::TcpClientEndpoint(const std::string& host, uint16_t port, int reconnect_interval)
    : _host(host), _port(port), _reconnect_interval(reconnect_interval),
      _last_reconnect_time(std::chrono::steady_clock::now()) {}

TcpClientEndpoint::~TcpClientEndpoint() {
    close();
}

bool TcpClientEndpoint::open() {
    if (isRunning()) return true;
    
    setState(State::CONNECTING);
    startThread();
    return true;
}

void TcpClientEndpoint::close() {
    stopThread();
    resetConnection();
    setState(State::DISCONNECTED);
}

void TcpClientEndpoint::resetConnection() {
    // 从 epoll 中移除 socket 监控
    if (_epollFd >= 0 && _socketFd >= 0) {
        epoll_ctl(_epollFd, EPOLL_CTL_DEL, _socketFd, nullptr);
    }
    
    if (_socketFd >= 0) {
        ::close(_socketFd);
        _socketFd = -1;
    }
    
    _connecting = false;
}

void TcpClientEndpoint::write(const uint8_t* data, size_t len) {
    if (!isConnected()) return;
    
    std::lock_guard<std::mutex> lock(_mutex);
    if (send(_socketFd, data, len, MSG_NOSIGNAL) < 0) {
        logError("Send failed: " + std::string(strerror(errno)));
        setState(State::ERROR);
        handleDisconnectEvent();
    }
}

void TcpClientEndpoint::run() {
    while (isRunning()) {
        // 创建或重新创建 epoll 实例
        if (_epollFd < 0) {
            _epollFd = epoll_create1(0);
            if (_epollFd < 0) {
                logError("Epoll creation failed: " + std::string(strerror(errno)));
                setState(State::ERROR);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
        }

        // 处理重连逻辑
        if (!isConnected() && !_connecting && !_reconnect_pending) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - _last_reconnect_time).count();
            
            if (elapsed >= _reconnect_interval) {
                logMessage("Attempting to connect...");
                setState(State::CONNECTING);
                if (tryConnect()) {
                    _connecting = true;
                } else {
                    // 连接失败，等待下一次重连
                    _last_reconnect_time = now;
                    _reconnect_pending = true;
                }
            }
        }

        // 事件循环 - 仅在有有效 epoll 描述符时执行
        if (_epollFd >= 0) {
            epoll_event events[2];
            int numEvents = epoll_wait(_epollFd, events, 2, 100); // 100ms超时
            
            if (numEvents < 0) {
                if (errno == EBADF) {
                    // epoll 描述符无效，重置连接
                    logError("Epoll_wait encountered bad file descriptor. Resetting connection.");
                    resetConnection();
                    continue;
                } else if (errno != EINTR) {
                    logError("Epoll_wait error: " + std::string(strerror(errno)));
                }
                continue;
            }

            for (int i = 0; i < numEvents; ++i) {
                // 处理连接事件
                if (events[i].events & EPOLLOUT && _connecting) {
                    handleConnectEvent();
                }
                // 处理断开事件
                else if (events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                    handleDisconnectEvent();
                }
                // 处理数据事件
                else if (events[i].events & EPOLLIN) {
                    handleSocketData();
                }
            }
        }

        // 处理重连等待状态
        if (_reconnect_pending) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - _last_reconnect_time).count();
            
            if (elapsed >= _reconnect_interval) {
                _reconnect_pending = false;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    
    // 线程退出前清理资源
    resetConnection();
    if (_epollFd >= 0) {
        ::close(_epollFd);
        _epollFd = -1;
    }
}

bool TcpClientEndpoint::tryConnect() {
    if (_socketFd >= 0) {
        resetConnection(); // 确保之前的连接已关闭
    }

    _socketFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (_socketFd < 0) {
        logError("Socket creation failed: " + std::string(strerror(errno)));
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(_port);
    
    if (inet_pton(AF_INET, _host.c_str(), &serverAddr.sin_addr) <= 0) {
        logError("Invalid address: " + _host);
        return false;
    }

    // 非阻塞连接
    int result = connect(_socketFd, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result < 0 && errno != EINPROGRESS) {
        logError("Connect failed: " + std::string(strerror(errno)));
        return false;
    }

    // 监控连接状态
    epoll_event event{};
    event.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
    event.data.fd = _socketFd;
    
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, _socketFd, &event) < 0) {
        logError("Epoll_ctl failed: " + std::string(strerror(errno)));
        return false;
    }

    return true;
}

void TcpClientEndpoint::handleConnectEvent() {
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(_socketFd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        logError("Getsockopt failed: " + std::string(strerror(errno)));
        handleDisconnectEvent();
        return;
    }
    
    if (error != 0) {
        logError("Connection failed: " + std::string(strerror(error)));
        handleDisconnectEvent();
        return;
    }

    // 连接成功，更新epoll监控事件
    epoll_event event{};
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
    event.data.fd = _socketFd;
    
    if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, _socketFd, &event) < 0) {
        logError("Epoll_ctl modify failed: " + std::string(strerror(errno)));
        handleDisconnectEvent();
        return;
    }

    _connecting = false;
    setState(State::CONNECTED);
    logMessage("Connected to " + _host + ":" + std::to_string(_port));
}

void TcpClientEndpoint::handleDisconnectEvent() {
    logMessage("Connection closed");
    resetConnection();
    setState(State::DISCONNECTED);
    _last_reconnect_time = std::chrono::steady_clock::now();
    _reconnect_pending = true; // 进入重连等待状态
}

void TcpClientEndpoint::handleSocketData() {
    uint8_t buffer[4096];
    ssize_t bytesRead = recv(_socketFd, buffer, sizeof(buffer), 0);
    
    if (bytesRead > 0) {
        processData(buffer, bytesRead);
    } else if (bytesRead == 0) {
        // 对端关闭连接
        handleDisconnectEvent();
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        logError("Receive error: " + std::string(strerror(errno)));
        handleDisconnectEvent();
    }
}