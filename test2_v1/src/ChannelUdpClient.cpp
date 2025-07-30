// src/ChannelUdpClient.cpp
#include "ChannelUdpClient.hpp"
#include <iostream>

ChannelUdpClient::ChannelUdpClient(boost::asio::io_context& io,
        const std::string& host, uint16_t port)
    : ChannelBase(io),
      m_host(host),
      m_port(port),
      m_socket(io, boost::asio::ip::udp::v4()) {}

ChannelUdpClient::~ChannelUdpClient() {
    stop();
}

void ChannelUdpClient::start() {
    if (m_isRunning) return;
    m_isRunning = true;
    resolveEndpoint();
    startReceive();
}

void ChannelUdpClient::stop() {
    if (!m_isRunning) return;
    m_isRunning = false;
    
    boost::system::error_code ec;
    m_socket.close(ec);
    if (ec) {
        log(LogLevel::ERROR, "UDP socket close error: " + ec.message());
    }
    m_serverEndpoint.reset();
}

void ChannelUdpClient::send(const std::string& data) {
    if (!m_isRunning) return;
    
    if (!m_serverEndpoint) {
        log(LogLevel::WARNING, "UDP Send failed: Endpoint not resolved");
        resolveEndpoint();
        return;
    }
    
    m_socket.async_send_to(boost::asio::buffer(data), *m_serverEndpoint,
        [this](boost::system::error_code ec, std::size_t) {
            if (ec) {
                log(LogLevel::ERROR, "UDP Send error: " + ec.message());
                m_serverEndpoint.reset();
            }
        });
}

void ChannelUdpClient::startReceive() {
    if (!m_isRunning) return;
    
    m_socket.async_receive_from(boost::asio::buffer(m_recvBuffer), m_remoteEndpoint,
        [this](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                if (receiveCallback) {
                    receiveCallback(std::string(m_recvBuffer.data(), length));
                }
                startReceive();
            } else {
                log(LogLevel::ERROR, "UDP Receive error: " + ec.message());
                if (m_isRunning) {
                    startReceive();
                }
            }
        });
}

void ChannelUdpClient::resolveEndpoint() {
    if (!m_isRunning) return;
    
    boost::asio::ip::udp::resolver resolver(ioContext);
    boost::system::error_code ec;
    auto endpoints = resolver.resolve(m_host, std::to_string(m_port), ec);
    if (!ec && !endpoints.empty()) {
        m_serverEndpoint = *endpoints.begin();
        log(LogLevel::INFO, "UDP endpoint resolved: " + m_host + ":" + std::to_string(m_port));
    } else {
        log(LogLevel::ERROR, "UDP resolve failed: " + ec.message());
    }
}