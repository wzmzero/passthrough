#pragma once
#include <string>
#include <vector>
#include <cstdint>

 
struct TeleSignalPoint {
    uint16_t address;
    bool value;  // 0/1状态
    uint64_t timestamp; // 状态变化时间戳
};

// 遥测点 (YC - 输入寄存器)
struct TeleMeasurePoint {
    uint16_t address;
    double value;  // 测量值
    uint64_t timestamp;
};

// 遥控点 (YK - 线圈)
struct TeleControlPoint {
    uint16_t address;
    bool value;  // 0/1状态
    uint64_t timestamp;
    bool pending; // 是否有待执行的遥控命令
};

// 遥调点 (YT - 保持寄存器)
struct TeleAdjustPoint {
    uint16_t address;
    double value; // 设定值
    uint64_t timestamp;
    bool pending; // 是否有待执行的遥调命令
};

// 四遥数据库结构
struct FourTeleDB {
    std::vector<TeleSignalPoint> yx_points;    // 遥信
    std::vector<TeleMeasurePoint> yc_points;    // 遥测
    std::vector<TeleControlPoint> yk_points;    // 遥控
    std::vector<TeleAdjustPoint> yt_points;     // 遥调
};

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