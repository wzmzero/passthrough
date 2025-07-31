//  
#pragma once
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include "Common.h"

 
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
    // 日志记录函数
    void log(LogLevel level, const std::string &message) {
        if (logCallback) {
            logCallback(level, message);} 
        else {
           std::cout <<"log:" << message << "\n";
            }
        }
    std::vector<char> m_recvBuffer;  
    boost::asio::io_context& ioContext;
    ReceiveCallback receiveCallback;
    LogCallback logCallback;
};