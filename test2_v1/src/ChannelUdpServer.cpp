// src/ChannelUdpServer.cpp
#include "ChannelUdpServer.hpp"
#include <iostream>

ChannelUdpServer::ChannelUdpServer(boost::asio::io_context& io, uint16_t port)
    : ChannelBase(io),
      m_socket(io, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)) {}

ChannelUdpServer::~ChannelUdpServer() {
    stop();
}

void ChannelUdpServer::start() {
    if (m_isRunning) return;
    m_isRunning = true;
    startReceive();
}

void ChannelUdpServer::stop() {
    if (!m_isRunning) return;
    m_isRunning = false;
    
    boost::system::error_code ec;
    m_socket.close(ec);
    if (ec) {
        log(LogLevel::ERROR, "UDP socket close error: " + ec.message());
    }
    
    {
        std::lock_guard<std::mutex> lock(m_endpointsMutex);
        m_clientEndpoints.clear();
    }
}

void ChannelUdpServer::addClientEndpoint(const boost::asio::ip::udp::endpoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_endpointsMutex);
    m_clientEndpoints.insert(endpoint);
}

void ChannelUdpServer::send(const std::string& data) {
    if (!m_isRunning) return;
    
    std::lock_guard<std::mutex> lock(m_endpointsMutex);
    for (const auto& endpoint : m_clientEndpoints) {
        m_socket.async_send_to(boost::asio::buffer(data), endpoint,
            [this, endpoint](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    log(LogLevel::ERROR, "UDP Send error to " + 
                        endpoint.address().to_string() + ":" + 
                        std::to_string(endpoint.port()) + ": " + ec.message());
                }
            });
    }
}

void ChannelUdpServer::startReceive() {
    if (!m_isRunning) return;
    
    m_socket.async_receive_from(boost::asio::buffer(m_recvBuffer), m_remoteEndpoint,
        [this](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                // 添加新客户端端点
                addClientEndpoint(m_remoteEndpoint);
                
                if (receiveCallback) {
                    receiveCallback(std::string(m_recvBuffer.data(), length));
                }
                
                // 继续接收
                startReceive();
            } else {
                if (ec != boost::asio::error::operation_aborted) {
                    log(LogLevel::ERROR, "UDP Receive error: " + ec.message());
                }
                if (m_isRunning) {
                    startReceive();
                }
            }
        });
}