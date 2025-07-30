// include/ChannelSerial.hpp
#pragma once
#include "ChannelBase.hpp"
#include <boost/asio.hpp>
#include <boost/asio/serial_port.hpp>

class ChannelSerial : public ChannelBase {
public:
    ChannelSerial(boost::asio::io_context& io,
                 const std::string& device, unsigned int baudRate);
    ~ChannelSerial() override;
    
    void start() override;
    void stop() override;
    void send(const std::string& data) override;
    bool isRunning() const override { return m_isRunning; }
    bool isConnected() const;
    
private:
    void tryOpen();
    void startReceive();
    
    boost::asio::serial_port m_port;
    std::string m_device;
    unsigned int m_baudRate;
    bool m_isRunning = false;
    std::array<char, 1024> m_recvBuffer;
};