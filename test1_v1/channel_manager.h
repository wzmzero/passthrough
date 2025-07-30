#pragma once
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <unordered_map>
#include "protocol_channel.h"

 

class ChannelManager {
public:
    void addChannel(std::unique_ptr<ProtocolChannel> channel);
 
    void stopAll();
    void removeChannel(const std::string& name);
    
private:
    struct ChannelThread {
        std::unique_ptr<ProtocolChannel> channel;
        std::thread thread;
    };
    
    std::mutex mutex_;
    std::vector<ChannelThread> channels_;
};