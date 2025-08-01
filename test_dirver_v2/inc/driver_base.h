// driver_base.h
#pragma once
#include "common.h"
#include <functional>
#include <vector>

class DriverBase {
public:
    virtual ~DriverBase() = default;
    
    // 核心接口
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual size_t write(const uint8_t* data, size_t len) = 0;
    
    // CRC计算
    static uint16_t calculateCRC(const uint8_t* data, size_t length) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < length; ++i) {
            crc ^= static_cast<uint16_t>(data[i]);
            for (int j = 0; j < 8; ++j) {
                if (crc & 0x0001) {
                    crc = (crc >> 1) ^ 0xA001;
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }

protected:
    // 传输模式检测
    bool isRTU() const { return transportMode_ == ModbusTransportMode::RTU; }
    bool isTCP() const { return transportMode_ == ModbusTransportMode::TCP; }
    
    ModbusTransportMode transportMode_ = ModbusTransportMode::RTU;
};