// src/ChannelSerial.cpp
#include "ChannelSerial.hpp"
#include <iostream>

ChannelSerial::ChannelSerial(boost::asio::io_context& io,
    const std::string& device, unsigned int baudRate)
    : ChannelBase(io),
      m_port(io),
      m_device(device),
      m_baudRate(baudRate) {}

ChannelSerial::~ChannelSerial() {
    stop();
}

void ChannelSerial::start() {
    if (m_isRunning) return;
    m_isRunning = true;
    tryOpen();
}

void ChannelSerial::stop() {
    if (!m_isRunning) return;
    m_isRunning = false;
    
    boost::system::error_code ec;
    m_port.close(ec);
    if (ec) {
        log(LogLevel::ERROR, "Serial port close error: " + ec.message());
    }
}

bool ChannelSerial::isConnected() const { 
    return m_port.is_open(); 
}

void ChannelSerial::send(const std::string& data) {
    if (!m_isRunning) return;
    
    if (!isConnected()) {
        log(LogLevel::WARNING, "Serial port not open");
        tryOpen();
        return;
    }
    
    boost::asio::async_write(m_port, boost::asio::buffer(data),
        [this](boost::system::error_code ec, std::size_t) {
            if (ec) {
                log(LogLevel::ERROR, "Serial Send error: " + ec.message());
                if (ec == boost::asio::error::broken_pipe) {
                    stop();
                    if (m_isRunning) { // 如果还在运行，则重新尝试打开
                        tryOpen();
                    }
                }
            }
        });
}

void ChannelSerial::tryOpen() {
    if (!m_isRunning) return;
    
    boost::system::error_code ec;
    m_port.open(m_device, ec);
    if (ec) {
        log(LogLevel::ERROR, "Serial open error: " + ec.message());
        // 重试打开
        auto timer = std::make_shared<boost::asio::steady_timer>(ioContext);
        timer->expires_after(std::chrono::seconds(5));
        timer->async_wait([this, timer](const boost::system::error_code& ec) {
            if (!ec) {
                tryOpen();
            }
        });
        return;
    }
    
    m_port.set_option(boost::asio::serial_port_base::baud_rate(m_baudRate), ec);
    m_port.set_option(boost::asio::serial_port_base::character_size(8), ec);
    m_port.set_option(boost::asio::serial_port_base::stop_bits(
        boost::asio::serial_port_base::stop_bits::one), ec);
    m_port.set_option(boost::asio::serial_port_base::parity(
        boost::asio::serial_port_base::parity::none), ec);
    m_port.set_option(boost::asio::serial_port_base::flow_control(
        boost::asio::serial_port_base::flow_control::none), ec);
    
    if (ec) {
        log(LogLevel::ERROR, "Serial set option error: " + ec.message());
        stop();
        tryOpen();
        return;
    }
    
    log(LogLevel::INFO, "Serial port opened: " + m_device);
    startReceive();
}

void ChannelSerial::startReceive() {
    if (!m_isRunning || !isConnected()) return;
    
    m_port.async_read_some(boost::asio::buffer(m_recvBuffer),
        [this](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                if (receiveCallback) {
                    receiveCallback(std::string(m_recvBuffer.data(), length));
                }
                startReceive();
            } else {
                log(LogLevel::ERROR, "Serial Receive error: " + ec.message());
                if (ec != boost::asio::error::operation_aborted) {
                    stop();
                    if (m_isRunning) {
                        tryOpen();
                    }
                }
            }
        });
}