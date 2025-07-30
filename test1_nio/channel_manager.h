#pragma once
#include <vector>
#include <memory>
#include "protocol_channel.h"

class ProtocolChannel; // 前置声明

class ChannelManager {
public:
    void addChannel(std::unique_ptr<ProtocolChannel> channel);
    void startAll();
    void stopAll();
    
private:
    std::vector<std::unique_ptr<ProtocolChannel>> channels_;
    std::vector<std::thread> worker_threads_;  // 线程容器
};