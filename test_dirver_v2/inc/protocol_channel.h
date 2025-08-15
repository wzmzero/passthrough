// protocol_channel.h
#pragma once
#include "endpoint.h"
#include "ring_buffer.h"
#include "thread_pool.h"
#include <memory>
#include <string>
#include <atomic>
#include "models.h"
class ProtocolChannel {
public:
    ProtocolChannel(const std::string& name,
                   const EndpointConfig& node1_config,
                   const EndpointConfig& node2_config,
                   ThreadPool& thread_pool);
    
    ~ProtocolChannel();
    void start();
    void stop();
    const std::string& getName() const { return name_; }

private:
    std::unique_ptr<Endpoint> createEndpoint(const EndpointConfig& config);
    void setupForwarding();
    // 数据转发任务实现
    void forwardDataTask(RingBuffer& source, Endpoint& target, 
                         const std::string& direction, int index);

    std::string name_;
    std::unique_ptr<Endpoint> node1_;
    std::unique_ptr<Endpoint> node2_;
    RingBuffer node1_to_node2_buffer_{1024 * 1024}; // 1MB buffer
    RingBuffer node2_to_node1_buffer_{1024 * 1024}; // 1MB buffer
    ThreadPool& thread_pool_;
    std::atomic<bool> running_{false};
     // 使用原子标志跟踪转发任务状态
    std::array<std::atomic_flag, 2> forwarding_task_active_;
};