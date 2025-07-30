#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

class Endpoint {
public:
    // 回调函数类型定义
    using DataCallback = std::function<void(const uint8_t* data, size_t len)>;
    using LogCallback = std::function<void(const std::string& msg)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    Endpoint();
    virtual ~Endpoint();
    
    // 核心接口
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual void write(const uint8_t* data, size_t len) = 0;
    
    // 回调设置
    void setDataCallback(DataCallback cb);
    void setLogCallback(LogCallback cb);
    void setErrorCallback(ErrorCallback cb);

    bool isRunning() const;
    bool isConnected() const;

protected:
    enum class State { DISCONNECTED, CONNECTING, CONNECTED, ERROR };

    // 线程控制
    virtual void run() = 0;
    void startThread();
    void stopThread();
    void notifyThread();

    // 状态管理
    void setState(State newState);
    State getState() const;

    // 回调函数
    void logMessage(const std::string& msg);
    void logError(const std::string& error);
    void processData(const uint8_t* data, size_t len);

    // 同步工具
    std::atomic<State> _state{State::DISCONNECTED};
    std::atomic<bool> _running{false};
    std::thread _worker;
    std::mutex _mutex;
    std::condition_variable _cv;

    // 回调函数对象
    DataCallback _dataCallback;
    LogCallback _logCallback;
    ErrorCallback _errorCallback;
};