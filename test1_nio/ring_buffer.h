#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <cstring>
#include <queue>

class MessageBuffer {
public:
    explicit MessageBuffer(size_t max_messages) 
        : max_messages_(max_messages) {}
    
    bool push(std::vector<uint8_t> data) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (message_queue_.size() >= max_messages_) {
            return false; // 立即返回不阻塞
        }
        message_queue_.push(std::move(data));
        lock.unlock();
        cv_.notify_one();
        return true;
    }
    
    std::optional<std::vector<uint8_t>> pop(bool block = true) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (message_queue_.empty()) {
            if (!block) return std::nullopt;
            cv_.wait(lock, [this] { 
                return !message_queue_.empty() || !running_; 
            });
            if (message_queue_.empty()) return std::nullopt;
        }
        auto data = std::move(message_queue_.front());
        message_queue_.pop();
        return data;
    }
    
    void stop() {
        running_ = false;
        cv_.notify_all();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return message_queue_.size();
    }

private:
    std::queue<std::vector<uint8_t>> message_queue_;
    const size_t max_messages_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{true};
};