#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "protocol_channel.h"
#include "thread_pool.h"
#include "logrecord.h"
class ChannelManager {
public:
    ChannelManager(size_t thread_pool_size = 12) 
        : thread_pool_(thread_pool_size) {}
    
    void addChannel(std::unique_ptr<ProtocolChannel> channel);
    void stopAll();
    void removeChannel(const std::string& name);
 
    ThreadPool& getThreadPool() { return thread_pool_; }

private:
    std::mutex mutex_;
    std::vector<std::unique_ptr<ProtocolChannel>> channels_;
    ThreadPool thread_pool_;
};