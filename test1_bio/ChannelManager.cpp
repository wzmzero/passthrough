// ChannelManager.cpp
#include "ChannelManager.hpp"

ChannelManager& ChannelManager::instance() {
    static ChannelManager instance;
    return instance;
}

ChannelManager::~ChannelManager() {
    stopAllChannels();
}

void ChannelManager::stopAllChannels() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, channel] : channels_) {
        channel->stop();
    }
}

std::vector<int> ChannelManager::getAllChannelIds() const {
    std::vector<int> ids;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, channel] : channels_) {
        ids.push_back(id);
    }
    return ids;
}
int ChannelManager::addChannel(std::unique_ptr<Endpoint> input, 
                              std::unique_ptr<Endpoint> output) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = next_channel_id_++;
    channels_[id] = std::make_unique<Channel>(std::move(input), std::move(output));
    return id;
}

bool ChannelManager::removeChannel(int channel_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(channel_id);
    if (it == channels_.end()) return false;
    
    it->second->stop();
    channels_.erase(it);
    return true;
}

bool ChannelManager::startChannel(int channel_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(channel_id);
    if (it == channels_.end()) return false;
    return it->second->start();
}

bool ChannelManager::stopChannel(int channel_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(channel_id);
    if (it == channels_.end()) return false;
    it->second->stop();
    return true;
}

bool ChannelManager::isChannelRunning(int channel_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(channel_id);
    if (it == channels_.end()) return false;
    return it->second->isRunning();
}

// 其他方法实现类似...