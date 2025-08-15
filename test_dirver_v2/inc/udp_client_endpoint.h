// udp_client_endpoint.h
#pragma once
#include "endpoint.h"
#include <sys/epoll.h>
#include <netinet/in.h>  // 添加此头文件

class UdpClientEndpoint : public Endpoint {
public:
    UdpClientEndpoint(const std::string& host, uint16_t port);
    ~UdpClientEndpoint() override;
    
    bool open() override;
    void close() override;
    void write(const uint8_t* data, size_t len) override;

private:
    void run() override;
    void handleData();

    const std::string _host;
    const uint16_t _port;
    int _socketFd = -1;
    int _epollFd = -1;
    struct sockaddr_in _serverAddr;  // 现在类型完整
};