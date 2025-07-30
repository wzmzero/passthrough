#pragma once
#include "endpoint.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

class UdpServerEndpoint : public Endpoint {
public:
    UdpServerEndpoint(uint16_t port);
    virtual ~UdpServerEndpoint();
    
    void send(const uint8_t* data, size_t len) override;
    std::string getInfo() const override;

protected:
    bool connect() override;
    void disconnect() override;
    void run() override;

private:
    uint16_t port_;
    int serverFd_ = -1;
    sockaddr_in lastClient_{};
    bool clientKnown_ = false;
    fd_set readFds_;
};