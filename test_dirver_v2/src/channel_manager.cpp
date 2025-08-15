// channel_manager.cpp
#include "channel_manager.h"
#include <iostream>

void ChannelManager::addChannel(std::unique_ptr<ProtocolChannel> channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    channel->start();
    channels_.push_back(std::move(channel));
    LOG_INFO("Added channel: %s", channels_.back()->getName().c_str());
}

void ChannelManager::stopAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO("Stopping all channels...");
    for (auto& channel : channels_) {
        channel->stop();
    }
    channels_.clear();
    LOG_INFO("All channels stopped");
}

void ChannelManager::removeChannel(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(channels_.begin(), channels_.end(),
        [&](const auto& channel) { 
            return channel->getName() == name; 
        });
    
    if (it != channels_.end()) {
        LOG_INFO("Removing channel: %s", name.c_str());
        (*it)->stop();
        channels_.erase(it);
        LOG_INFO("Removed channel: %s", name.c_str());
    }else {
        LOG_WARNING("Attempted to remove non-existent channel: %s", name.c_str());
    }
}