#pragma once
#include <string>
#include <memory>

// 端点配置结构体
struct EndpointConfig {
    int id = 0;  // 主键
    std::string type;
    uint16_t port = 0;
    std::string ip;
    std::string serial_port;
    uint32_t baud_rate = 0;

    bool operator==(const EndpointConfig& other) const {
        return id == other.id &&
               type == other.type &&
               port == other.port &&
               ip == other.ip &&
               serial_port == other.serial_port &&
               baud_rate == other.baud_rate;
    }
    
    // 添加比较运算符用于map
    bool operator<(const EndpointConfig& other) const {
        if (type != other.type) return type < other.type;
        if (port != other.port) return port < other.port;
        if (ip != other.ip) return ip < other.ip;
        if (serial_port != other.serial_port) return serial_port < other.serial_port;
        return baud_rate < other.baud_rate;
    }
};

// 通道配置结构体
struct ChannelConfig {
    int id = 0;  // 主键
    std::string name;
    int input_id = 0;  // 外键 -> EndpointConfig.id
    int output_id = 0; // 外键 -> EndpointConfig.id
    // 嵌套对象（程序内使用）
    std::shared_ptr<EndpointConfig> input;  
    std::shared_ptr<EndpointConfig> output; 

    bool operator==(const ChannelConfig& other) const {
        return id == other.id &&
               name == other.name &&
               input_id == other.input_id &&
               output_id == other.output_id;
    }
};