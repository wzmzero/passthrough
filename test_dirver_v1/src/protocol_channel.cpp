// protocol_channel.cpp
#include "protocol_channel.h"
#include "tcp_server_endpoint.h" 
#include "tcp_client_endpoint.h"
#include "udp_server_endpoint.h"
#include "udp_client_endpoint.h"
#include "serial_endpoint.h"
#include "logrecord.h"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <memory>
#include <atomic>
#include <array>

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
                               ThreadPool& thread_pool): name_(name), thread_pool_(thread_pool),
    forwarding_task_active_{{ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT}} {
    
    CH_LOG_INFO(name_, "Creating channel %s", name_.c_str());
    CH_LOG_INFO(name_, "Input: %s, Output: %s", node1_config.type.c_str(), node2_config.type.c_str());
    
    try {
        node1_ = createEndpoint(node1_config);
        node2_ = createEndpoint(node2_config);
    } catch (const std::exception& e) {
        CH_LOG_ERROR(name_, "Endpoint creation failed: %s", e.what());
        throw;
    }

    // 设置日志回调
    auto make_log_callback = [this](const std::string& prefix) {
        return [this, prefix](const std::string& msg) {
            CH_LOG_INFO(name_, "[%s] %s", prefix.c_str(), msg.c_str());
        };
    };
    
    node1_->setLogCallback(make_log_callback("NODE1"));
    node2_->setLogCallback(make_log_callback("NODE2"));
    
    // 设置错误回调
    auto make_error_callback = [this](const std::string& prefix) {
        return [this, prefix](const std::string& msg) {
            CH_LOG_ERROR(name_, "[%s] %s", prefix.c_str(), msg.c_str());
        };
    };
    
    node1_->setErrorCallback(make_error_callback("NODE1"));
    node2_->setErrorCallback(make_error_callback("NODE2"));
    
    // 设置数据转发
    setupForwarding();
}

void ProtocolChannel::setupForwarding() {
    // Node1 -> Node2 数据回调
    node1_->setDataCallback([this](const uint8_t* data, size_t len) {
        LOG_BINARY(name_, "[NODE1 RECV]", data, len);
        
        if (!node1_to_node2_buffer_.push(data, len)) {
            CH_LOG_WARNING(name_, "NODE1_TO_NODE2 buffer full, dropped %zu bytes", len);
            return;
        }
        
        // 提交转发任务（如果尚未提交）
        if (!forwarding_task_active_[0].test_and_set(std::memory_order_acq_rel)) {
            thread_pool_.enqueue([this] {
                forwardDataTask(node1_to_node2_buffer_, *node2_, "[NODE1->NODE2]", 0);
            });
        }
    });
    
    // Node2 -> Node1 数据回调
    node2_->setDataCallback([this](const uint8_t* data, size_t len) {
        LOG_BINARY(name_, "[NODE2 RECV]", data, len);
        
        if (!node2_to_node1_buffer_.push(data, len)) {
            CH_LOG_WARNING(name_, "NODE2_TO_NODE1 buffer full, dropped %zu bytes", len);
            return;
        }
        
        if (!forwarding_task_active_[1].test_and_set(std::memory_order_acq_rel)) {
            thread_pool_.enqueue([this] {
                forwardDataTask(node2_to_node1_buffer_, *node1_, "[NODE2->NODE1]", 1);
            });
        }
    });
}

void ProtocolChannel::forwardDataTask(RingBuffer& source, Endpoint& target, 
                                    const std::string& direction, int index) {
    try {
        uint8_t buffer[4096];
        size_t total_forwarded = 0;
        size_t len;
        
        // 处理当前所有可用数据
        while ((len = source.pop(buffer, sizeof(buffer)))) {
            LOG_BINARY_TEXT(name_, direction, buffer, len);
            target.write(buffer, len);
            total_forwarded += len;
        }
        
        // if (total_forwarded > 0) {
        //     CH_LOG_DEBUG(name_, "%s forwarded %zu bytes", direction.c_str(), total_forwarded);
        // }
    } 
    catch (const std::runtime_error& e) {
        CH_LOG_ERROR(name_, "%s forwarding error: %s", direction.c_str(), e.what());
    } 
    catch (const std::exception& e) {
        CH_LOG_ERROR(name_, "%s unexpected error: %s", direction.c_str(), e.what());
    }
    
    // 标记任务完成
    forwarding_task_active_[index].clear(std::memory_order_release);
    
    // 检查是否有新数据到达，需要重新提交任务
    if (!source.empty()) {
        if (forwarding_task_active_[index].test_and_set(std::memory_order_acq_rel)) {
            // 已经有任务在运行（可能是新数据触发的）
            return;
        }
        thread_pool_.enqueue([this, index, &source, &target, direction] {
            forwardDataTask(source, target, direction, index);
        });
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
    if (!running_) return;
    running_ = false;
    
    // 关闭缓冲区
    node1_to_node2_buffer_.shutdown();
    node2_to_node1_buffer_.shutdown();
    
    // 关闭端点
    node1_->close();
    node2_->close();
    
    // 等待活动任务完成（最多50ms）
    constexpr int max_wait = 5;
    int wait_count = 0;
    while (true) {
        bool active0 = forwarding_task_active_[0].test_and_set(std::memory_order_acquire);
        if (!active0) forwarding_task_active_[0].clear(std::memory_order_release);
        bool active1 = forwarding_task_active_[1].test_and_set(std::memory_order_acquire);
        if (!active1) forwarding_task_active_[1].clear(std::memory_order_release);
        if (!(active0 || active1)) break;
        if (++wait_count >= max_wait) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    CH_LOG_INFO(name_, "Channel stopped");
}