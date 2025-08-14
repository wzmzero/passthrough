#pragma once
#include <string>
#include <memory>
#include <variant>
#include <ctime>
#include <any>
#include <cstdint>
#include <sqlite_orm/sqlite_orm.h>
 
// 枚举类型定义
enum class TelemDataType { YX, YC, YK, YT };
enum class ValueType { Boolean, Integer, Float };

// 四遥数据点结构体
struct TelemPoint {
    int64_t id = 0;                         // 主键ID
    std::string name;                       // 名称 (e.g., "voltage", "coil_switch")
    std::string register_address;           // 寄存器地址 (e.g., "0x4001")
    TelemDataType data_type;               // 四遥类型
    ValueType value_type;                   // 值类型
    std::any value; // 存储实际值
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



// ========== TelemDataType 转换函数 ==========
inline std::string TelemDataTypeToString(TelemDataType type) {
    switch (type) {
        case TelemDataType::YX: return "YX";
        case TelemDataType::YC: return "YC";
        case TelemDataType::YK: return "YK";
        case TelemDataType::YT: return "YT";
    }
    throw std::domain_error("Invalid TelemDataType");
}

inline TelemDataType TelemDataTypeFromString(const std::string& str) {
    if (str == "YX") return TelemDataType::YX;
    if (str == "YC") return TelemDataType::YC;
    if (str == "YK") return TelemDataType::YK;
    if (str == "YT") return TelemDataType::YT;
    throw std::runtime_error("Invalid TelemDataType string: " + str);
}

// ========== ValueType 转换函数 ==========
inline std::string ValueTypeToString(ValueType type) {
    switch (type) {
        case ValueType::Boolean: return "Boolean";
        case ValueType::Integer: return "Integer";
        case ValueType::Float:   return "Float";
    }
    throw std::domain_error("Invalid ValueType");
}

inline ValueType ValueTypeFromString(const std::string& str) {
    if (str == "Boolean") return ValueType::Boolean;
    if (str == "Integer") return ValueType::Integer;
    if (str == "Float")   return ValueType::Float;
    throw std::runtime_error("Invalid ValueType string: " + str);
}