// tcp_server_endpoint.h
#pragma once
#include "endpoint.h"
#include <sys/epoll.h>
#include <unordered_map>
#include <netinet/in.h>  // 添加此头文件

class TcpServerEndpoint : public Endpoint {  // 修正类名
public:
    explicit TcpServerEndpoint(uint16_t port);
    ~TcpServerEndpoint() override;
    
    bool open() override;
    void close() override;
    void write(const uint8_t* data, size_t len) override;

private:
    void run() override;
    void handleNewConnection();
    void handleClientData(int clientFd);
    void closeClient(int clientFd);

    const uint16_t _port;
    int _serverFd = -1;
    int _epollFd = -1;
    std::unordered_map<int, struct sockaddr_in> _clients;
};