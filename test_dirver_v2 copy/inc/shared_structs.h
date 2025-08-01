#pragma once
#include <string>
#include <vector>
#include <cstdint>
struct EndpointConfig {
    std::string type; // "tcp_server", "tcp_client", "udp_server", "udp_client", "serial"
    
    // 通用字段
    uint16_t port = 0;
    std::string ip;
    
    // 串口专用字段
    std::string serial_port;
    uint32_t baud_rate = 0;

    // 添加比较运算符
    bool operator==(const EndpointConfig& other) const {
        return type == other.type &&
               port == other.port &&
               ip == other.ip &&
               serial_port == other.serial_port &&
               baud_rate == other.baud_rate;
    }
    
    bool operator!=(const EndpointConfig& other) const {
        return !(*this == other);
    }
};

struct ChannelConfig {
    std::string name;
    EndpointConfig input;
    EndpointConfig output;

    // 添加比较运算符
    bool operator==(const ChannelConfig& other) const {
        return name == other.name &&
               input == other.input &&
               output == other.output;
    }
    
    bool operator!=(const ChannelConfig& other) const {
        return !(*this == other);
    }
};