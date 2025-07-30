// UdpClientEndpoint.hpp
#pragma once
#include "BaseEndpoint.hpp"
#include "NetworkClient.hpp"
#include <thread>

class UdpClientEndpoint : public BaseEndpoint {
public:
    UdpClientEndpoint(const std::string& server_ip, int port)
        : server_ip_(server_ip), port_(port) {}
    
    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool write(const std::string& data) override;
    void start() override;
    void stop() override;
    
    Endpoint::Type getType() const override { return Endpoint::UDP_CLIENT; }
    std::string getConfigString() const override;
    
private:
    void receiveThreadFunc();
    
    std::string server_ip_;
    int port_;
    std::unique_ptr<NetworkClient> client_;
    std::thread receive_thread_;
};