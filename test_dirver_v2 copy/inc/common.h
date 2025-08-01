// common.h
#pragma once
#include <cstdint>
#include <vector>

// 传输模式枚举
enum class ModbusTransportMode {
    RTU,
    TCP
};

// 功能码枚举
enum class ModbusFunctionCode : uint8_t {
    // 读功能码 (0x01-0x04)
    READ_COILS = 0x01,
    READ_DISCRETE_INPUTS = 0x02,
    READ_HOLDING_REGISTERS = 0x03,
    READ_INPUT_REGISTERS = 0x04,
    
    // 写功能码 (0x05-0x10)
    WRITE_SINGLE_COIL = 0x05,
    WRITE_SINGLE_REGISTER = 0x06,
    WRITE_MULTIPLE_COILS = 0x0F,
    WRITE_MULTIPLE_REGISTERS = 0x10,
    
    // 自定义功能码
    SHGK_WRITE = 0x13
};

// 数据点信息
struct ModbusDataPoint {
    uint16_t address;     // 寄存器地址
    uint16_t value;       // 寄存器值
};

// 报文信息结构
struct ModbusFrameInfo {
    ModbusFunctionCode functionCode;
    bool isException = false;
    uint8_t exceptionCode = 0;
    std::vector<ModbusDataPoint> dataPoints;
    
    // TCP模式专用
    uint16_t transactionId = 0;
    uint16_t bytesFollowing = 0;
};