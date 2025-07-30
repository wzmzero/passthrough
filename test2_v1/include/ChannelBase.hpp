// include/ChannelBase.hpp
#pragma once
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <iostream>

enum class LogLevel
{
    ERROR,
    WARNING,
    INFO,
    DEBUG
};

class ChannelBase {
public:
    using ReceiveCallback = std::function<void(const std::string&)>;
    using LogCallback = std::function<void(LogLevel, const std::string&)>;
    ChannelBase(boost::asio::io_context& io) : ioContext(io),m_recvBuffer(1024){}

    virtual ~ChannelBase() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void send(const std::string& data) = 0;
    virtual bool isRunning() const = 0;
    
    void setReceiveCallback(ReceiveCallback cb) { receiveCallback = std::move(cb); }
    void setLogCallback(LogCallback cb) {logCallback = std::move(cb);}

protected:
    void log(LogLevel level, const std::string &message) {
        if (logCallback) {
            logCallback(level, message);
        } else {
            // 默认日志输出
            if (level == LogLevel::ERROR) {
                std::cerr << "\033[1;31m[ERROR] " << message << "\033[0m\n";
            } else if (level == LogLevel::WARNING) {
                std::cerr << "\033[1;33m[WARNING] " << message << "\033[0m\n";
            } else if (level == LogLevel::INFO) {
                std::cout << "\033[1;32m[INFO] " << message << "\033[0m\n";
            } else {
                std::cout << "[LOG] " << message << "\n";
            }
        }
    }
    std::vector<char> m_recvBuffer; // 默认接收缓冲区大小
    boost::asio::io_context& ioContext;
    ReceiveCallback receiveCallback;
    LogCallback logCallback;
};