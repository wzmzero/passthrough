#pragma once
#include "Channel.hpp"
#include <vector>
#include <mutex>
#include <memory>
#include <unordered_map>

class ChannelManager {
public:
    static ChannelManager& instance();
    
    int addChannel(std::unique_ptr<Endpoint> input, 
                  std::unique_ptr<Endpoint> output);
    bool removeChannel(int channel_id);
    
    bool startChannel(int channel_id);
    bool stopChannel(int channel_id);
    bool isChannelRunning(int channel_id) const;
    
    std::vector<int> getAllChannelIds() const;
    std::string getChannelStatus(int channel_id) const;
    
    void startAllChannels();
    void stopAllChannels();
    void removeAllChannels();
    
private:
    ChannelManager() = default;
    ~ChannelManager();
    
    mutable std::mutex mutex_;
    std::unordered_map<int, std::unique_ptr<Channel>> channels_;
    int next_channel_id_{1};
};