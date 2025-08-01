// driver_modbus_s.h
#pragma once
#include "driver_base.h"
#include <unordered_map>

class DriverModbusS : public DriverBase {
public:
    explicit DriverModbusS(uint8_t address = 1, 
                        ModbusTransportMode mode = ModbusTransportMode::RTU);
    
    // 核心接口
    bool open() override;
    void close() override;
    size_t write(const uint8_t* data, size_t len) override;
    
    // 帧处理
    bool parseRequest(const uint8_t* frame, size_t len, ModbusFrameInfo& info);
    std::vector<uint8_t> createResponse(const ModbusFrameInfo& info);
    
    // 内存映射
    void setCoil(uint16_t addr, bool value) { coils_[addr] = value; }
    bool getCoil(uint16_t addr) const;
    void setHoldingRegister(uint16_t addr, uint16_t value) { holdings_[addr] = value; }
    uint16_t getHoldingRegister(uint16_t addr) const;
    
    // 地址管理
    void setAddress(uint8_t addr) { address_ = addr; }
    uint8_t getAddress() const { return address_; }
    
    // 请求处理
    ModbusFrameInfo processRequest(const ModbusFrameInfo& request);

private:
    uint8_t address_;
    
    // 内存映射区
    std::unordered_map<uint16_t, bool> coils_;
    std::unordered_map<uint16_t, bool> discreteInputs_;
    std::unordered_map<uint16_t, uint16_t> holdings_;
    std::unordered_map<uint16_t, uint16_t> inputs_;
    
    // TCP事务ID缓存
    uint16_t transactionId_ = 0;
};