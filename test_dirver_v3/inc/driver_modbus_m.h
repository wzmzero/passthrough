// driver_modbus_m.h
#pragma once
#include "driver_base.h"

class DriverModbusM : public DriverBase {
public:
    explicit DriverModbusM(uint8_t slaveAddr = 1, 
                         ModbusTransportMode mode = ModbusTransportMode::RTU);
    
    // 核心接口
    bool open() override;
    void close() override;
    size_t write(const uint8_t* data, size_t len) override;
    
    // 请求创建
    std::vector<uint8_t> createReadRequest(ModbusFunctionCode func, 
                                         uint16_t startAddr, 
                                         uint16_t quantity);
    
    std::vector<uint8_t> createWriteRequest(ModbusFunctionCode func, 
                                          uint16_t addr, 
                                          uint16_t value);
    std::vector<uint8_t> createRequestFrame(const ModbusFrameInfo& info);
    // SHGK写请求
    std::vector<uint8_t> createSHGKWriteRequest(uint16_t addr, uint32_t value);
    
    // 响应解析
    bool parseResponse(const uint8_t* frame, size_t len, ModbusFrameInfo& info);
    
    // 地址管理
    void setSlaveAddress(uint8_t addr) { slaveAddress_ = addr; }
    uint8_t getSlaveAddress() const { return slaveAddress_; }

private:
    uint8_t slaveAddress_;
    uint16_t transactionId_ = 0; // TCP事务ID计数器
};