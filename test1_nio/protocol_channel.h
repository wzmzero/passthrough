// protocol_channel.h
#pragma once
#include "endpoint.h"
#include "ring_buffer.h"
#include "shared_structs.h"  // 包含 EndpointConfig
#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <stdexcept>  // 用于异常处理

class ProtocolChannel {
public:
    // 修改构造函数以接收两个端点配置
    ProtocolChannel(const std::string& name,
                   const EndpointConfig& node1_config,
                   const EndpointConfig& node2_config);
    
    ~ProtocolChannel();
    void start();
    void stop();
    const std::string& getName() const { return name_; }

private:
    // 创建端点的工厂方法
    std::unique_ptr<Endpoint> createEndpoint(const EndpointConfig& config);
    
    void node1ToNode2Worker();
    void node2ToNode1Worker();
    void logHandler(const std::string& msg);
    
    std::string name_;
    std::unique_ptr<Endpoint> node1_;
    std::unique_ptr<Endpoint> node2_;
    MessageBuffer node1_to_node2_buffer_{1000}; // 1000条消息容量
    MessageBuffer node2_to_node1_buffer_{1000};
    std::thread node1_to_node2_thread_;
    std::thread node2_to_node1_thread_;
    std::atomic<bool> running_{false};
};