// driver_modbus_s.h
#pragma once
#include "driver_base.h"
#include <memory>
#include "common.h"

class DriverModbusS : public DriverBase {
public:
    explicit DriverModbusS(uint8_t address = 1, 
                        ModbusTransportMode mode = ModbusTransportMode::RTU,
                        ITelemetryDatabase* database = nullptr);
    
    // 核心接口
    bool open() override;
    void close() override;
    size_t write(const uint8_t* data, size_t len) override;
    
    // 帧处理
    bool parseRequest(const uint8_t* frame, size_t len, ModbusFrameInfo& info);
    std::vector<uint8_t> createResponse(const ModbusFrameInfo& info);
    
    // 设置数据库
    void setDatabase(ITelemetryDatabase* db) { database_ = db; }
    
    // 请求处理
    ModbusFrameInfo processRequest(const ModbusFrameInfo& request);

private:
    uint8_t address_;
    ITelemetryDatabase* database_;  // 四遥数据库
    
    // 将四遥数据点转换为Modbus数据点
    ModbusDataPoint convertToModbusPoint(const TelemetryPoint& tp);
    
    // 处理遥测数据读取
    bool processReadYC(uint16_t start, uint16_t count, std::vector<ModbusDataPoint>& points);
    
    // 处理遥信数据读取
    bool processReadYX(uint16_t start, uint16_t count, std::vector<ModbusDataPoint>& points);
    
    // 处理遥控命令
    bool processWriteYK(uint16_t address, bool value);
    
    // 处理遥调命令
    bool processWriteYT(uint16_t address, uint16_t value);
};