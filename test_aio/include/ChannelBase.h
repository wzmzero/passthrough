#pragma once

#include "ThreadSafeQueue.h"
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

class ChannelBase {
public:
    using ReceiveCallback = std::function<void(const std::vector<uint8_t>&)>;
    using ReceiveQueue = ThreadSafeQueue<uint8_t>;

    ChannelBase(boost::asio::io_context& io_context) 
        : io_context_(io_context), running_(false) {}
    virtual ~ChannelBase() { stop(); }

    virtual bool start() = 0;
    virtual void stop();
    virtual bool send(const std::vector<uint8_t>& data) = 0;

    void setReceiveCallback(ReceiveCallback cb) { receive_callback_ = std::move(cb); }
    ReceiveQueue& getReceiveQueue() { return receive_queue_; }

protected:
    virtual void run_receive() = 0;
    
    boost::asio::io_context& io_context_;
    std::atomic<bool> running_;
    std::thread receive_thread_;
    ReceiveQueue receive_queue_;
    ReceiveCallback receive_callback_;
};

inline void ChannelBase::stop() {
    running_ = false;
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
}