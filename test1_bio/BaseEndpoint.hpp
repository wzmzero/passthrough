// BaseEndpoint.hpp
#pragma once
#include "Endpoint.hpp"
#include <mutex>
#include <atomic> // 添加这行

class BaseEndpoint : public Endpoint {
public:
    BaseEndpoint() = default;
    virtual ~BaseEndpoint() { stop(); }
    
    void setDataCallback(DataCallback callback) override {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        data_callback_ = callback;
    }
    
    void setErrorCallback(ErrorCallback callback) override {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        error_callback_ = callback;
    }
    
    void stop() override {
        running_ = false;
    }
    
protected:
    void callDataCallback(const std::string& data) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (data_callback_) data_callback_(data);
    }
    
    void callErrorCallback(const std::string& error) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (error_callback_) error_callback_(error);
    }
    
    DataCallback data_callback_;
    ErrorCallback error_callback_;
    std::atomic<bool> running_{false};
    
private:
    mutable std::mutex callback_mutex_;
};