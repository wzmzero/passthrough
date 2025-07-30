// protocol_channel.cpp
#include "protocol_channel.h"
#include "tcp_server_endpoint.h" 
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
                               const EndpointConfig& node2_config,
                               ThreadPool& thread_pool)
    : name_(name), thread_pool_(thread_pool) {
    // 记录通道初始化
    CH_LOG_INFO(name_, "Creating channel %s", name_.c_str());
    CH_LOG_INFO(name_, "Input: %s, Output: %s", node1_config.type.c_str(), node2_config.type.c_str());
     // 创建端点
    try {
        node1_ = createEndpoint(node1_config);
        node2_ = createEndpoint(node2_config);
    } catch (const std::exception& e) {
        CH_LOG_ERROR(name_, "Endpoint creation failed: %s", e.what());
        throw;
    }

    // 设置日志回调
    node1_->setLogCallback([this](const std::string& msg) {
        CH_LOG_INFO(name_, "[NODE1] %s", msg.c_str());
    });
    node2_->setLogCallback([this](const std::string& msg) {
        CH_LOG_INFO(name_, "[NODE2] %s", msg.c_str());
    });
    // 设置错误回调
    node1_->setErrorCallback([this](const std::string& msg) {
        CH_LOG_ERROR(name_, "[NODE1] %s", msg.c_str());
    });
    node2_->setErrorCallback([this](const std::string& msg) {
        CH_LOG_ERROR(name_, "[NODE2] %s", msg.c_str());
    });
    
    // 设置数据转发
    setupForwarding();
}

void ProtocolChannel::setupForwarding() {
    // Node1 -> Node2 数据回调
    node1_->setDataCallback([this](const uint8_t* data, size_t len) {
         LOG_BINARY(name_, "[NODE1 RECV]", data, len);
        // 非阻塞写入缓冲区
        if (!node1_to_node2_buffer_.push(data, len)) {
            CH_LOG_WARNING(name_, "NODE1_TO_NODE2 buffer full, dropped %d bytes", len);
        }
    });
    // Node2 -> Node1 数据回调
    node2_->setDataCallback([this](const uint8_t* data, size_t len) {
        LOG_BINARY(name_, "[NODE2 RECV]", data, len);
        if (!node2_to_node1_buffer_.push(data, len)) {
            CH_LOG_WARNING(name_, "NODE2_TO_NODE1 buffer full, dropped %d bytes", len);
        }
    });
    
    // 启动转发任务
    thread_pool_.enqueue([this] {
        forwardLoop(node1_to_node2_buffer_, *node2_, "[NODE1->NODE2]");
    });
    
    thread_pool_.enqueue([this] {
        forwardLoop(node2_to_node1_buffer_, *node1_, "[NODE2->NODE1]");
    });
}

void ProtocolChannel::forwardLoop(RingBuffer& source, Endpoint& target, const std::string& direction) {
    uint8_t buffer[4096]; // 10KB 缓冲区
    while (running_) {
        size_t len = source.pop(buffer, sizeof(buffer));
        if (len > 0) {
            LOG_BINARY_TEXT(name_, direction, buffer, len);
            try {
                target.write(buffer, len);
            } catch (const std::exception& e) {
                CH_LOG_ERROR(name_, "Forwarding failed: %s", e.what());
            }
        }
    }
}

ProtocolChannel::~ProtocolChannel() {
    stop();
}

void ProtocolChannel::start() {
    running_ = true;

    node1_->open();
    node2_->open();
    CH_LOG_INFO(name_, "---------------Channel started---------------");
}

void ProtocolChannel::stop() {
    running_ = false;
    node1_to_node2_buffer_.shutdown();
    node2_to_node1_buffer_.shutdown();
    node1_->close();
    node2_->close();
    CH_LOG_INFO(name_, "Channel stopped");
}



 