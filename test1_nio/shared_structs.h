#pragma once
#include <string>
#include <cstdint>
// 端点配置结构体
struct EndpointConfig {
    std::string type; // "tcp_server", "tcp_client", "udp_server", "udp_client", "serial"
    
    // 通用字段
    uint16_t port = 0;
    std::string ip;
    
    // 串口专用字段
    std::string serial_port;
    uint32_t baud_rate = 0;
};

// 通道配置结构体
struct ChannelConfig {
    std::string name;
    EndpointConfig input;
    EndpointConfig output;
};