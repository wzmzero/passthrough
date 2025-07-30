// protocol_channel.cpp
#include "protocol_channel.h"
#include "tcp_server_endpoint.h"  // 确保正确包含
#include "tcp_client_endpoint.h"
#include "udp_server_endpoint.h"
#include "udp_client_endpoint.h"
#include "serial_endpoint.h"
#include "logrecord.h"  // 日志记录功能
#include <iostream>
#include <iomanip>
#include <ctime>
#include <memory>  // 确保包含

std::unique_ptr<Endpoint> ProtocolChannel::createEndpoint(const EndpointConfig& config) {
    if (config.type == "tcp_server") {
        return std::make_unique<TcpServerEndpoint>(config.port);
    }
    else if (config.type == "tcp_client") {
        return std::make_unique<TcpClientEndpoint>(config.ip, config.port);
    }
    else if (config.type == "udp_server") {
        return std::make_unique<UdpServerEndpoint>(config.port);
    }
    else if (config.type == "udp_client") {
        return std::make_unique<UdpClientEndpoint>(config.ip, config.port);
    }
    else if (config.type == "serial") {
        return std::make_unique<SerialEndpoint>(config.serial_port, config.baud_rate);
    }
    
    throw std::runtime_error("Unknown endpoint type: " + config.type);
}

 

ProtocolChannel::ProtocolChannel(const std::string& name,
                               const EndpointConfig& node1_config,
                               const EndpointConfig& node2_config)
    : name_(name)
{
    // 创建端点
    node1_ = createEndpoint(node1_config);
    node2_ = createEndpoint(node2_config);

    // 设置日志回调
    node1_->setLogCallback([this](const std::string& msg) {
        logHandler("[NODE1 LOG] " + msg);
    });
    node2_->setLogCallback([this](const std::string& msg) {
        logHandler("[NODE2 LOG] " + msg);
    });
    
    // 设置错误回调
    node1_->setErrorCallback([this](const std::string& msg) {
        logHandler("[NODE1 ERROR] " + msg);
    });
    node2_->setErrorCallback([this](const std::string& msg) {
        logHandler("[NODE2 ERROR] " + msg);
    });
    
    // 设置数据转发
    setupForwarding();
}

void ProtocolChannel::setupForwarding() {
    // Node1 -> Node2 转发
    node1_->setDataCallback([this](const uint8_t* data, size_t len) {
        std::string prefix = "[" + name_ + "] (NODE1->NODE2):";
        LOG_BINARY(prefix, data, len);
        node2_->write(data, len);
    });
    
    // Node2 -> Node1 转发
    node2_->setDataCallback([this](const uint8_t* data, size_t len) {
        std::string prefix = "[" + name_ + "] (NODE2->NODE1):";
        LOG_BINARY(prefix, data, len);
        node1_->write(data, len);
    });
}

ProtocolChannel::~ProtocolChannel() {
    stop();
}

void ProtocolChannel::start() {
    node1_->open();
    node2_->open();
    logHandler("Channel started");
}

void ProtocolChannel::stop() {
    node1_->close();
    node2_->close();
    logHandler("Channel stopped");
}

void ProtocolChannel::logHandler(const std::string& msg) {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::cout << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] "
              << "[" << name_ << "] " << msg << std::endl;
}