#include "channel_manager.h"
#include <algorithm>
#include <iostream>

void ChannelManager::addChannel(std::unique_ptr<ProtocolChannel> channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 获取通道名称和指针（避免lambda捕获引用）
    std::string name = channel->getName();
    ProtocolChannel* channelPtr = channel.get();
    
    // 创建通道并立即启动线程
    channels_.push_back({std::move(channel), std::thread()});
    auto& ct = channels_.back();
    
    // 安全地启动线程
    ct.thread = std::thread([channelPtr] {
        std::cout << "Starting channel: " << channelPtr->getName() << std::endl;
        channelPtr->start();
    });
    
    std::cout << "Created and started channel: " << name << std::endl;
}

// 移除 startAll 方法实现

void ChannelManager::stopAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& ct : channels_) {
        ct.channel->stop();
    }
    for (auto& ct : channels_) {
        if (ct.thread.joinable()) {
            ct.thread.join();
        }
    }
    channels_.clear();
}

void ChannelManager::removeChannel(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(channels_.begin(), channels_.end(),
        [&](const ChannelThread& ct) { return ct.channel->getName() == name; });
    
    if (it != channels_.end()) {
        it->channel->stop();
        if (it->thread.joinable()) {
            it->thread.join();
        }
        channels_.erase(it);
        std::cout << "Removed channel: " << name << std::endl;
    }
}