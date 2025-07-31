// src/ChannelBridge.cpp
#include "ChannelBridge.hpp"
#include "ChannelTcpServer.hpp"
#include "ChannelTcpClient.hpp"
#include "ChannelUdpServer.hpp"
#include "ChannelUdpClient.hpp"
#include "ChannelSerial.hpp"
#include "Logger.h" // 添加头文件
 
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
      // 创建桥接器前缀 (例如: "bridge1")
    std::string bridgePrefix = "bridge" + std::to_string(m_config.id);
    
    m_channel1->start();
    m_channel2->start();
    // 设置通道日志回调
    m_channel1->setLogCallback([this, bridgePrefix](LogLevel level, const std::string& msg) {
        Logger::instance().log(bridgePrefix , level, "[CH1] "+msg);
    });
    m_channel2->setLogCallback([this, bridgePrefix](LogLevel level, const std::string& msg) {
        Logger::instance().log(bridgePrefix, level,"[CH2] "+msg);
    });
    // 设置双向转发回调
    m_channel1->setReceiveCallback([this,bridgePrefix](const std::string& data) {
        if (m_channel2 && m_channel2->isRunning()) {
            m_channel2->send(data);
        }
        std::cout << "Received data on channel 1: " << data << std::endl;
        // Logger::instance().logData(bridgePrefix, data);
    });
    
    m_channel2->setReceiveCallback([this,bridgePrefix](const std::string& data) {
        if (m_channel1 && m_channel1->isRunning()) {
            m_channel1->send(data);
        }
        std::cout << "Received data on channel 2: " << data << std::endl;
        //  Logger::instance().logData(bridgePrefix, data);
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