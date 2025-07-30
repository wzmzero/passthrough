#pragma once
#include "endpoint.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

class UdpClientEndpoint : public Endpoint {
public:
    UdpClientEndpoint(const std::string& ip, uint16_t port);
    virtual ~UdpClientEndpoint();
    
    void send(const uint8_t* data, size_t len) override;
    std::string getInfo() const override;

protected:
    bool connect() override;
    void disconnect() override;
    void run() override;

private:
    std::string serverIp_;
    uint16_t serverPort_;
    int clientFd_ = -1;
    sockaddr_in serverAddr_{};
    fd_set readFds_;
};