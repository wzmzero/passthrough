// ring_buffer.h
#pragma once
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : buffer_(capacity), capacity_(capacity), 
          read_pos_(0), write_pos_(0), count_(0),
          shutdown_(false) {}

    // 非阻塞写入
    bool push(const uint8_t* data, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (shutdown_ || size > capacity_ - count_) {
            return false; // 已关闭或空间不足
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
            if (write_pos_ >= capacity_) write_pos_ -= capacity_;
        }

        count_ += size;
        return true;
    }

    // 非阻塞读取
    size_t pop(uint8_t* data, size_t max_size) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (shutdown_ || count_ == 0) {
            return 0; // 已关闭或无数据
        }

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
            if (read_pos_ >= capacity_) read_pos_ -= capacity_;
        }

        count_ -= to_read;
        return to_read;
    }

    // 检查是否为空
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == 0;
    }

    // 关闭缓冲区
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }

private:
    std::vector<uint8_t> buffer_;
    size_t capacity_;
    size_t read_pos_;
    size_t write_pos_;
    size_t count_;
    mutable std::mutex mutex_;
    bool shutdown_ = false;
};