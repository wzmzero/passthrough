// UdpServerEndpoint.hpp
#pragma once
#include "BaseEndpoint.hpp"
#include "NetworkServer.hpp"
#include <thread>
#include <atomic>
#include <mutex>
#include <set>

class UdpServerEndpoint : public BaseEndpoint {
public:
    UdpServerEndpoint(int port);
    ~UdpServerEndpoint();
    
    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool write(const std::string& data) override;
    void start() override;
    void stop() override;
    
    Endpoint::Type getType() const override { return Endpoint::UDP_SERVER; }
    std::string getConfigString() const override;
    
    void broadcast(const std::string& data);
    
private:
    void serverThreadFunc();
    
    int port_;
    std::unique_ptr<NetworkServer> server_;
    std::thread server_thread_;
};