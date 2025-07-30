#pragma once
#include "ChannelBase.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <netinet/in.h>

class ChannelTcpServer : public ChannelBase {
public:
    ChannelTcpServer(int port, int maxConnections = 5);
    virtual ~ChannelTcpServer();
    
    bool start() override;
    void stop() override;
    bool send(const std::vector<uint8_t>& data) override;

private:
    void acceptThreadFunc();
    void clientHandler(int client_fd);
    
    int port;
    int maxConnections;
    int serverFd = -1;
    std::atomic<bool> running{false};
    std::thread acceptThread;
    std::mutex clientsMutex;
    std::unordered_map<int, std::thread> clientThreads;
};