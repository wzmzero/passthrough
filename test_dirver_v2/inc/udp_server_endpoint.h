// udp_server_endpoint.h
#pragma once
#include "endpoint.h"
#include <sys/epoll.h>
#include <netinet/in.h>
#include <map>
#include <mutex>
#include <string>

class UdpServerEndpoint : public Endpoint {
public:
    explicit UdpServerEndpoint(uint16_t port);
    ~UdpServerEndpoint() override;
    
    bool open() override;
    void close() override;
    void write(const uint8_t* data, size_t len) override;

private:
    void run() override;
    void handleData();
    std::string getClientId(const sockaddr_in& addr) const; // 生成客户端唯一ID

    const uint16_t _port;
    int _socketFd = -1;
    int _epollFd = -1;
    
    // 客户端地址管理
    std::mutex _clientsMutex;
    std::map<std::string, sockaddr_in> _clients; // 客户端ID->地址映射
};