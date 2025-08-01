#pragma once
#include "endpoint.h"
#include <sys/epoll.h>
#include <chrono>
#include <thread>
#include <atomic>

class TcpClientEndpoint : public Endpoint {
public:
    TcpClientEndpoint(const std::string& host, uint16_t port, int reconnect_interval = 1);
    ~TcpClientEndpoint() override;
    
    bool open() override;
    void close() override;
    void write(const uint8_t* data, size_t len) override;

private:
    void run() override;
    bool tryConnect();
    void handleSocketData();
    void resetConnection();
    void handleConnectEvent();
    void handleDisconnectEvent();

    const std::string _host;
    const uint16_t _port;
    const int _reconnect_interval; // 重连间隔（秒）
    int _socketFd = -1;
    int _epollFd = -1;
    std::chrono::steady_clock::time_point _last_reconnect_time;
    std::atomic<bool> _connecting{false}; // 使用原子操作确保线程安全
    std::atomic<bool> _reconnect_pending{false}; // 标记重连等待状态
};