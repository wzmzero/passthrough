#pragma once
#include "Endpoint.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <memory>

class Channel {
public:
    Channel(std::unique_ptr<Endpoint> input, 
            std::unique_ptr<Endpoint> output);
    ~Channel();
    
    bool start();
    void stop();
    bool isRunning() const;
    
    const Endpoint& getInputEndpoint() const;
    const Endpoint& getOutputEndpoint() const;
    
    std::string getConfigString() const;
    
private:
    void inputThreadFunc();
    void outputThreadFunc();
    
    // 线程安全的队列
    class SafeQueue {
    public:
        void push(const std::string& data);
        bool pop(std::string& data, std::chrono::milliseconds timeout);
        bool empty() const;
        void clear();
        void notifyAll();
        
    private:
        std::queue<std::string> queue_;
        mutable std::mutex mutex_;
        std::condition_variable cond_;
    };
    
    std::unique_ptr<Endpoint> input_endpoint_;
    std::unique_ptr<Endpoint> output_endpoint_;
    SafeQueue data_queue_;
    
    std::atomic<bool> running_{false};
    std::thread input_thread_;
    std::thread output_thread_;
    
    // 统计信息
    std::atomic<size_t> bytes_received_{0};
    std::atomic<size_t> bytes_sent_{0};
};