// include/ChannelBridge.hpp
#pragma once
#include "ChannelBase.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <string>

struct ChannelConfig {
    std::string type;
    std::string host;
    uint16_t port;
    std::string device;
    unsigned int baudRate;
};

struct BridgeConfig {
    int id;
    ChannelConfig channel1;
    ChannelConfig channel2;
};

class ChannelBridge {
public:
    ChannelBridge(boost::asio::io_context& io, const BridgeConfig& config);
    ~ChannelBridge();
    
    void start();
    void stop();
    int getId() const { return m_config.id; }
    bool isRunning() const { return m_isRunning; }

private:
    std::unique_ptr<ChannelBase> createChannel(const ChannelConfig& config);
    
    boost::asio::io_context& m_io;
    BridgeConfig m_config;
    std::unique_ptr<ChannelBase> m_channel1;
    std::unique_ptr<ChannelBase> m_channel2;
    bool m_isRunning = false;
};