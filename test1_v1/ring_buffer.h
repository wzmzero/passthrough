#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <cstring>

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) 
        : buffer_(capacity), capacity_(capacity) {}
    
    bool push(const uint8_t* data, size_t len) {
        if (len == 0) return true;
        if (len > capacity_) return false;
        
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this, len] { 
            return (capacity_ - size_) >= len; 
        });
        
        size_t space_to_end = capacity_ - tail_;
        if (len <= space_to_end) {
            std::memcpy(buffer_.data() + tail_, data, len);
        } else {
            std::memcpy(buffer_.data() + tail_, data, space_to_end);
            std::memcpy(buffer_.data(), data + space_to_end, len - space_to_end);
        }
        
        tail_ = (tail_ + len) % capacity_;
        size_ += len;
        lock.unlock();
        cv_.notify_one();
        return true;
    }
    
    std::optional<std::vector<uint8_t>> pop(size_t max_len, bool block = true) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (size_ == 0) {
            if (!block) return std::nullopt;
            cv_.wait(lock, [this] { return size_ > 0 || !running_; });
            if (size_ == 0) return std::nullopt;
        }
        
        size_t to_read = std::min(size_, max_len);
        std::vector<uint8_t> data(to_read);
        
        size_t first_chunk = std::min(to_read, capacity_ - head_);
        std::memcpy(data.data(), buffer_.data() + head_, first_chunk);
        
        if (to_read > first_chunk) {
            std::memcpy(data.data() + first_chunk, buffer_.data(), 
                        to_read - first_chunk);
        }
        
        head_ = (head_ + to_read) % capacity_;
        size_ -= to_read;
        lock.unlock();
        cv_.notify_one();
        return data;
    }
    
    void stop() {
        running_ = false;
        cv_.notify_all();
    }
    
    size_t size() const { return size_; }

private:
    std::vector<uint8_t> buffer_;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t size_ = 0;
    const size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{true};
};