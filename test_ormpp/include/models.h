#pragma once
#include <string>
#include <memory>
#include <variant>
#include <ctime>
// 四遥数据类型枚举
enum class TelemDataType {
    YX,     // 遥测     0001H-4001H
    YC,     // 遥信     4001H-5000H
    YK,     // 遥控     6001H-6100H
    YT      // 遥调      
};

// 值类型枚举
enum class ValueType {
    Boolean,      // 布尔值 (0/1)
    Integer,      // 整数
    Float         // 浮点数
};

// 四遥数据点结构体
struct TelemPoint {
    int64_t id = 0;                         // 主键ID
    std::string name;                       // 名称 (e.g., "voltage", "coil_switch")
    std::string register_address;           // 寄存器地址 (e.g., "0x4001")
    TelemDataType data_type;               // 四遥类型
    ValueType value_type;                   // 值类型
    std::variant<bool, int32_t, float> value; // 存储实际值
    std::time_t timestamp;              // 时间戳
    std::string unit;                       // 单位 (e.g., "V", "A", "m/s")
    int request_flag = 0;                   // 请求标志 (0-未请求, 1-请求中, 2-已请求)
};


// 端点配置结构体
struct EndpointConfig {
    int id = 0;  // 主键
    std::string type;
    uint16_t port = 0;
    std::string ip;
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

// 通道配置结构体
struct ChannelConfig {
    int id = 0;  // 主键
    std::string name;
    int input_id = 0;  // 外键 -> EndpointConfig.id
    int output_id = 0; // 外键 -> EndpointConfig.id
    // 嵌套对象（程序内使用）
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
