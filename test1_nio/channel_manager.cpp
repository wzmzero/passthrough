#include "channel_manager.h"
#include <thread>
#include <vector>
#include <algorithm>

void ChannelManager::addChannel(std::unique_ptr<ProtocolChannel> channel) {
    channels_.push_back(std::move(channel));
}

void ChannelManager::startAll() {
    // 创建线程容器
    std::vector<std::thread> workers;
    workers.reserve(channels_.size());
    
    // 为每个通道创建启动线程
    for (auto& channel : channels_) {
        workers.emplace_back([&] {
            channel->start();
        });
    }
    
    // 转移线程所有权到成员变量
    worker_threads_ = std::move(workers);
}

void ChannelManager::stopAll() {
    // 停止所有通道
    for (auto& channel : channels_) {
        channel->stop();
    }
    
    // 等待所有线程完成
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    // 清空线程容器
    worker_threads_.clear();
}