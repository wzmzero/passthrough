#pragma once
#include "ChannelBase.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <string>

class ChannelTcpClient : public ChannelBase {
public:
    ChannelTcpClient(const std::string& ip, int port);
    virtual ~ChannelTcpClient();
    
    bool start() override;
    void stop() override;
    bool send(const std::vector<uint8_t>& data) override;

private:
    void connectionThreadFunc();
    bool connectToServer();
    
    std::string serverIp;
    int serverPort;
    int clientFd = -1;
    std::atomic<bool> running{false};
    std::thread workerThread;
    std::mutex writeMutex;
};