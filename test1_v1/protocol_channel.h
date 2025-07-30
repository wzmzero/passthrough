// protocol_channel.h
#pragma once
#include "endpoint.h"
#include "ring_buffer.h"
#include "shared_structs.h"
#include <memory>
#include <string>

class ProtocolChannel {
public:
    ProtocolChannel(const std::string& name,
                   const EndpointConfig& node1_config,
                   const EndpointConfig& node2_config);
    
    ~ProtocolChannel();
    void start();
    void stop();
    const std::string& getName() const { return name_; }

private:
    std::unique_ptr<Endpoint> createEndpoint(const EndpointConfig& config);
    void logHandler(const std::string& msg);
    void setupForwarding();
    
    std::string name_;
    std::unique_ptr<Endpoint> node1_;
    std::unique_ptr<Endpoint> node2_;
    RingBuffer node1_to_node2_buffer_{1024 * 1024}; // 1MB buffer
    RingBuffer node2_to_node1_buffer_{1024 * 1024}; // 1MB buffer
};