// Channel.cpp
#include "Channel.hpp"
#include <chrono>

void Channel::SafeQueue::push(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(data);
    cond_.notify_one();
}

bool Channel::SafeQueue::pop(std::string& data, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (cond_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
        data = queue_.front();
        queue_.pop();
        return true;
    }
    return false;
}

Channel::Channel(std::unique_ptr<Endpoint> input, 
                 std::unique_ptr<Endpoint> output)
    : input_endpoint_(std::move(input)), 
      output_endpoint_(std::move(output)) 
{
    input_endpoint_->setDataCallback([this](const std::string& data) {
        data_queue_.push(data);
        bytes_received_ += data.size();
    });
}

Channel::~Channel() {
    stop();
}

bool Channel::start() {
    if (running_) return false;
    
    if (!input_endpoint_->open()) {
        return false;
    }
    
    if (!output_endpoint_->open()) {
        input_endpoint_->close();
        return false;
    }
    
    running_ = true;
    input_endpoint_->start();
    output_endpoint_->start();
    
    input_thread_ = std::thread(&Channel::inputThreadFunc, this);
    output_thread_ = std::thread(&Channel::outputThreadFunc, this);
    
    return true;
}

void Channel::stop() {
    if (!running_) return;
    
    running_ = false;
    data_queue_.notifyAll();
    
    input_endpoint_->stop();
    output_endpoint_->stop();
    
    if (input_thread_.joinable()) input_thread_.join();
    if (output_thread_.joinable()) output_thread_.join();
}

void Channel::inputThreadFunc() {
    // 实际工作由端点的回调完成
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Channel::outputThreadFunc() {
    while (running_) {
        std::string data;
        if (data_queue_.pop(data, std::chrono::milliseconds(100))) {
            if (!output_endpoint_->write(data)) {
                // 错误处理
            } else {
                bytes_sent_ += data.size();
            }
        }
    }
}


bool Channel::SafeQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

void Channel::SafeQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        queue_.pop();
    }
}

void Channel::SafeQueue::notifyAll() {
    cond_.notify_all();
}

// Channel 方法实现
bool Channel::isRunning() const {
    return running_;
}