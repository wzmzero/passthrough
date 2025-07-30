// src/ChannelBridge.cpp
#include "ChannelBridge.hpp"
#include "ChannelTcpServer.hpp"
#include "ChannelTcpClient.hpp"
#include "ChannelUdpServer.hpp"
#include "ChannelUdpClient.hpp"
#include "ChannelSerial.hpp"
 
 
#include <iostream>
#include <stdexcept>

ChannelBridge::ChannelBridge(boost::asio::io_context& io, const BridgeConfig& config)
    : m_io(io), m_config(config) {
    m_channel1 = createChannel(config.channel1);
    m_channel2 = createChannel(config.channel2);
    
    if (!m_channel1 || !m_channel2) {
        throw std::runtime_error("Failed to create channels for bridge");
    }
}

ChannelBridge::~ChannelBridge() {
    stop();
}

void ChannelBridge::start() {
    if (m_isRunning) return;
    
    m_channel1->start();
    m_channel2->start();
    
    // 设置双向转发回调
    m_channel1->setReceiveCallback([this](const std::string& data) {
        if (m_channel2 && m_channel2->isRunning()) {
            m_channel2->send(data);
        }
    });
    
    m_channel2->setReceiveCallback([this](const std::string& data) {
        if (m_channel1 && m_channel1->isRunning()) {
            m_channel1->send(data);
        }
    });
    
    m_isRunning = true;
}

void ChannelBridge::stop() {
    if (!m_isRunning) return;
    m_isRunning = false;
    
    if (m_channel1) m_channel1->stop();
    if (m_channel2) m_channel2->stop();
}

std::unique_ptr<ChannelBase> ChannelBridge::createChannel(const ChannelConfig& config) {
    try {
        if (config.type == "tcp_server") {
            return std::make_unique<ChannelTcpServer>(
                m_io, config.host, config.port);
        }
        else if (config.type == "tcp_client") {
            return std::make_unique<ChannelTcpClient>(
                m_io, config.host, config.port);
        }
        else if (config.type == "udp_server") {
            return std::make_unique<ChannelUdpServer>(m_io, config.port);
        }
        else if (config.type == "udp_client") {
            return std::make_unique<ChannelUdpClient>(
                m_io, config.host, config.port);
        }
        else if (config.type == "serial") {
            return std::make_unique<ChannelSerial>(
                m_io, config.device, config.baudRate);
        }
        else {
            std::cerr << "Unknown channel type: " << config.type << std::endl;
            return nullptr;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error creating channel: " << e.what() << std::endl;
        return nullptr;
    }
}