#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include "logrecord.h"
class Endpoint {
public:
    using DataCallback = std::function<void(const uint8_t*, size_t)>;
    using LogCallback = std::function<void(const std::string&)>;
    
    enum ConnectionStatus {
        DISCONNECTED = 0,
        CONNECTED = 1,
        RECONNECTING = 2
    };
    
    Endpoint() : running_(false), status_(DISCONNECTED) {}
    virtual ~Endpoint() { close(); }
    
    virtual bool open();
    virtual void close();
    virtual void restart();
    virtual void setReceiveCallback(DataCallback callback);
    virtual void setLogCallback(LogCallback callback);
    virtual void send(const uint8_t* data, size_t len) = 0;
    virtual std::string getInfo() const = 0;
    
    ConnectionStatus getStatus() const { return status_; }

protected:
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual void run() = 0;
    void log(const std::string& msg);
    
    static void* workThread(void* arg);
    
    std::thread workThread_;
    std::atomic<bool> running_;
    std::atomic<ConnectionStatus> status_;
    std::atomic<bool> restartFlag_{false};
    
    DataCallback dataCallback_;
    LogCallback logCallback_;
    
    std::mutex sendMutex_;
    std::vector<uint8_t> sendBuffer_;
};
