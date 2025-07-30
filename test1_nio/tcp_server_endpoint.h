#pragma once
#include "endpoint.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <sys/select.h>

class TcpServerEndpoint : public Endpoint {
public:
    TcpServerEndpoint(uint16_t port);
    virtual ~TcpServerEndpoint();
    
    void send(const uint8_t* data, size_t len) override;
    std::string getInfo() const override;

protected:
    bool connect() override;
    void disconnect() override;
    void run() override;
    
    void handleNewConnection();
    void handleClientData(int clientFd);
    void closeClient(int clientFd);

private:
    uint16_t port_;
    int serverFd_ = -1;
    std::vector<int> clientFds_;
    std::vector<int> toCloseFds_;  // Clients to close at the start of next run cycle
    fd_set readFds_;
    int maxFd_ = 0;
};