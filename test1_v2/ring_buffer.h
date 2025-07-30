#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : buffer_(capacity), capacity_(capacity), 
          read_pos_(0), write_pos_(0), count_(0) {}
    
    bool push(const uint8_t* data, size_t size) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (size > capacity_ - count_) {
            return false; // 空间不足
        }
        
        // 分两部分写入（处理回绕）
        size_t first_part = std::min(size, capacity_ - write_pos_);
        std::copy(data, data + first_part, buffer_.begin() + write_pos_);
        
        if (size > first_part) {
            size_t second_part = size - first_part;
            std::copy(data + first_part, data + size, buffer_.begin());
            write_pos_ = second_part;
        } else {
            write_pos_ += first_part;
            if (write_pos_ == capacity_) write_pos_ = 0;
        }
        
        count_ += size;
        cv_.notify_one();
        return true;
    }
    
    size_t pop(uint8_t* data, size_t max_size) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待数据可用
        cv_.wait(lock, [this] { return count_ > 0 || !running_; });
        
        if (!running_) return 0;
        
        size_t to_read = std::min(count_, max_size);
        
        // 分两部分读取（处理回绕）
        size_t first_part = std::min(to_read, capacity_ - read_pos_);
        std::copy(buffer_.begin() + read_pos_, 
                 buffer_.begin() + read_pos_ + first_part, 
                 data);
        
        if (to_read > first_part) {
            size_t second_part = to_read - first_part;
            std::copy(buffer_.begin(), buffer_.begin() + second_part, data + first_part);
            read_pos_ = second_part;
        } else {
            read_pos_ += first_part;
            if (read_pos_ == capacity_) read_pos_ = 0;
        }
        
        count_ -= to_read;
        return to_read;
    }
    
    void shutdown() {
        running_ = false;
        cv_.notify_all();
    }

private:
    std::vector<uint8_t> buffer_;
    size_t capacity_;
    size_t read_pos_;
    size_t write_pos_;
    size_t count_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{true};
};