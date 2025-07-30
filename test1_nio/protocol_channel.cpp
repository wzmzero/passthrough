// protocol_channel.cpp
#include "protocol_channel.h"
#include "tcp_server_endpoint.h"
#include "tcp_client_endpoint.h"
#include "udp_server_endpoint.h"
#include "udp_client_endpoint.h"
#include "serial_endpoint.h"
#include <iostream>
#include <iomanip>
#include <ctime>

// 端点工厂实现
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

// 修改后的构造函数
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
        logHandler("[NODE1] " + msg);
    });
    node2_->setLogCallback([this](const std::string& msg) {
        logHandler("[NODE2] " + msg);
    });
    
    // 设置接收回调
    node1_->setReceiveCallback([this](const uint8_t* data, size_t len) {
        std::vector<uint8_t> packet(data, data + len); // 保存完整消息
        if (!node1_to_node2_buffer_.push(std::move(packet))) {
            logHandler("Node1->Node2 buffer overflow, dropped message");
        }
    });

    node2_->setReceiveCallback([this](const uint8_t* data, size_t len) {
        std::vector<uint8_t> packet(data, data + len);
        if (!node2_to_node1_buffer_.push(std::move(packet))) {
            logHandler("Node2->Node1 buffer overflow, dropped message");
        }
    });
}

ProtocolChannel::~ProtocolChannel() {
    stop();
}

void ProtocolChannel::start() {
    if (running_) return;
    
    running_ = true;
    logHandler("Starting channel");
    
    // 打开端点
    if (!node1_->open()) {
        logHandler("Failed to open node1 endpoint");
        return;
    }
    
    if (!node2_->open()) {
        logHandler("Failed to open node2 endpoint");
        node1_->close();
        return;
    }
    
    // 启动工作线程
    node1_to_node2_thread_ = std::thread(&ProtocolChannel::node1ToNode2Worker, this);
    node2_to_node1_thread_ = std::thread(&ProtocolChannel::node2ToNode1Worker, this);
    
    logHandler("Channel started");
}

void ProtocolChannel::stop() {
    if (!running_) return;
    
    running_ = false;
    logHandler("Stopping channel");
    
    // 停止缓冲区
    node1_to_node2_buffer_.stop();
    node2_to_node1_buffer_.stop();
    
    // 停止线程
    if (node1_to_node2_thread_.joinable()) node1_to_node2_thread_.join();
    if (node2_to_node1_thread_.joinable()) node2_to_node1_thread_.join();
    
    // 关闭端点
    node1_->close();
    node2_->close();
    
    logHandler("Channel stopped");
}

void ProtocolChannel::node1ToNode2Worker() {
    while (running_) {
        auto data = node1_to_node2_buffer_.pop();
        if (data) {
            node2_->send(data->data(), data->size());
        }
    }
}

void ProtocolChannel::node2ToNode1Worker() {
    while (running_) {
        auto data = node2_to_node1_buffer_.pop();
        if (data) {
            node1_->send(data->data(), data->size());
        }
    }
}

void ProtocolChannel::logHandler(const std::string& msg) {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::cout << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] "
              << "[" << name_ << "] " << msg << std::endl;
}