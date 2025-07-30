// src/ChannelTcpClient.cpp
#include "ChannelTcpClient.hpp"
#include <iostream>

ChannelTcpClient::ChannelTcpClient(boost::asio::io_context& io,
        const std::string& host, uint16_t port)
    : ChannelBase(io),
      m_host(host),
      m_port(port),
      m_socket(io),
      m_resolver(io),
      m_reconnectTimer(io) {}

ChannelTcpClient::~ChannelTcpClient() {
    stop();
}

void ChannelTcpClient::start() {
    if (m_isRunning) return;
    m_isRunning = true;
    startConnect();
}

void ChannelTcpClient::stop() {
    if (!m_isRunning) return;
    m_isRunning = false;
    m_isConnected = false;
    m_reconnectTimer.cancel();
    resetConnection();
}

void ChannelTcpClient::resetConnection() {
    boost::system::error_code ec;
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
}

void ChannelTcpClient::send(const std::string& data) {
    if (!m_isConnected) {
        log(LogLevel::WARNING, "TCP Send failed: Not connected");
        if (m_isRunning) {
            reconnect();
        }
        return;
    }
    
    boost::asio::async_write(m_socket, boost::asio::buffer(data),
        [this](boost::system::error_code ec, std::size_t) {
            if (ec) {
                log(LogLevel::ERROR, "TCP Send error: " + ec.message());
                if (m_isRunning && (ec == boost::asio::error::broken_pipe || 
                                    ec == boost::asio::error::connection_reset)) {
                    m_isConnected = false;
                    resetConnection();
                    reconnect();
                }
            }
        });
}

void ChannelTcpClient::startConnect() {
    if (!m_isRunning) return;
    
    m_resolver.async_resolve(m_host, std::to_string(m_port),
        [this](const boost::system::error_code& ec, 
               boost::asio::ip::tcp::resolver::results_type endpoints) {
            if (!ec) {
                boost::asio::async_connect(m_socket, endpoints,
                    [this](const boost::system::error_code& ec, const auto&) {
                        if (!ec) {
                            m_isConnected = true;
                            m_reconnectAttempts = 0;
                            m_reconnectInterval = std::chrono::seconds(1);
                            log(LogLevel::INFO, "TCP connected to " + m_host + ":" + std::to_string(m_port));
                            startReceive();
                        } else {
                            log(LogLevel::ERROR, "TCP Connect error: " + ec.message());
                            if (m_isRunning) {
                                reconnect();
                            }
                        }
                    });
            } else {
                log(LogLevel::ERROR, "TCP Resolve error: " + ec.message());
                if (m_isRunning) {
                    reconnect();
                }
            }
        });
}

void ChannelTcpClient::startReceive() {
    if (!m_isRunning || !m_isConnected) return;
    
    m_socket.async_read_some(boost::asio::buffer(m_recvBuffer),
        [this](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                if (receiveCallback) {
                    receiveCallback(std::string(m_recvBuffer.data(), length));
                }
                startReceive();
            } else if (ec != boost::asio::error::operation_aborted) {
                log(LogLevel::ERROR, "TCP Receive error: " + ec.message());
                if (m_isRunning) {
                    m_isConnected = false;
                    resetConnection();
                    reconnect();
                }
            }
        });
}

void ChannelTcpClient::reconnect() {
    if (!m_isRunning || m_isReconnecting) return;
    
    m_isReconnecting = true;
    m_reconnectAttempts++;
    
    // 指数退避策略
    m_reconnectInterval = std::chrono::seconds(
        std::min((1 << std::min(m_reconnectAttempts, 5)), 30)); // 最大30秒
    
    m_reconnectTimer.expires_after(m_reconnectInterval);
    m_reconnectTimer.async_wait([this](const boost::system::error_code& ec) {
        m_isReconnecting = false;
        if (!ec && m_isRunning) {
            log(LogLevel::INFO, "Reconnecting to " + m_host + ":" + 
                std::to_string(m_port) + " (attempt " + 
                std::to_string(m_reconnectAttempts) + ")");
            startConnect();
        }
    });
}