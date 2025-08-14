// driver_modbus_s.cpp
#include "driver_modbus_s.h"
#include <algorithm>
#include <stdexcept>
#include <ctime>

DriverModbusS::DriverModbusS(uint8_t address, ModbusTransportMode mode, ITelemetryDatabase* database)
    : address_(address), database_(database) {
    transportMode_ = mode;
}

bool DriverModbusS::open() { 
    // 硬件初始化代码
    return true; 
}

void DriverModbusS::close() { 
    // 硬件关闭代码 
}

size_t DriverModbusS::write(const uint8_t* data, size_t len) { 
    // 数据发送实现
    return len; 
}

bool DriverModbusS::parseRequest(const uint8_t* frame, size_t len, ModbusFrameInfo& info) {
    // 基础长度检查
    if (len < (isTCP() ? 8 : 4)) return false;
    
    size_t index = 0;
    uint8_t unitId = 0;
    
    // TCP模式处理
    if (isTCP()) {
        info.transactionId = (frame[index++] << 8) | frame[index++];
        // 跳过协议ID
        index += 2;  
        info.bytesFollowing = (frame[index++] << 8) | frame[index++];
        unitId = frame[index++];
    } 
    // RTU模式处理
    else {
        unitId = frame[index++];
        // CRC校验
        uint16_t crc = (frame[len-1] << 8) | frame[len-2];
        if (crc != DriverBase::calculateCRC(frame, len - 2)) return false;
    }
    
    // 地址校验
    if (unitId != address_ && unitId != 0) return false;
    
    // 功能码解析
    uint8_t funcByte = frame[index++];
    info.isException = (funcByte & 0x80) != 0;
    info.functionCode = static_cast<ModbusFunctionCode>(funcByte & 0x7F);
    
    // 异常响应
    if (info.isException) {
        if (len > index) info.exceptionCode = frame[index];
        return true;
    }
    
    // 解析不同功能码
    switch (info.functionCode) {
        case ModbusFunctionCode::READ_COILS:
        case ModbusFunctionCode::READ_DISCRETE_INPUTS: {
            if (len < index + 4) return false;
            uint16_t startAddr = (frame[index++] << 8) | frame[index++];
            uint16_t quantity = (frame[index++] << 8) | frame[index++];
            
            for (uint16_t i = 0; i < quantity; ++i) {
                info.dataPoints.push_back({static_cast<uint16_t>(startAddr + i), 0});
            }
            break;
        }
            
        case ModbusFunctionCode::WRITE_SINGLE_COIL:
        case ModbusFunctionCode::WRITE_SINGLE_REGISTER: {
            if (len < index + 4) return false;
            uint16_t addr = (frame[index++] << 8) | frame[index++];
            uint16_t value = (frame[index++] << 8) | frame[index++];
            info.dataPoints.push_back({addr, value});
            break;
        }
            
        case ModbusFunctionCode::SHGK_WRITE: {
            if (len < index + 6) return false;
            uint16_t addr = (frame[index++] << 8) | frame[index++];
            uint32_t value = (static_cast<uint32_t>(frame[index++]) << 24) |
                            (static_cast<uint32_t>(frame[index++]) << 16) |
                            (static_cast<uint32_t>(frame[index++]) << 8) |
                            frame[index];
            info.dataPoints.push_back({addr, static_cast<uint16_t>(value & 0xFFFF)});
            break;
        }
            
        default:
            info.isException = true;
            info.exceptionCode = 0x01; // 非法功能码
            break;
    }
    
    return true;
}

std::vector<uint8_t> DriverModbusS::createResponse(const ModbusFrameInfo& info) {
    std::vector<uint8_t> frame;
    
    // TCP头部
    if (isTCP()) {
        frame.push_back(info.transactionId >> 8);
        frame.push_back(info.transactionId & 0xFF);
        frame.push_back(0x00); // 协议ID高字节
        frame.push_back(0x00); // 协议ID低字节
        // 长度占位符
        frame.push_back(0x00); 
        frame.push_back(0x00);
        frame.push_back(address_);
    }
    // RTU头部
    else {
        frame.push_back(address_);
    }
    
    // 异常响应
    if (info.isException) {
        frame.push_back(static_cast<uint8_t>(info.functionCode) | 0x80);
        frame.push_back(info.exceptionCode);
    } 
    // 正常响应
    else {
        frame.push_back(static_cast<uint8_t>(info.functionCode));
        
        switch (info.functionCode) {
            // 读线圈/离散量响应
            case ModbusFunctionCode::READ_COILS:
            case ModbusFunctionCode::READ_DISCRETE_INPUTS: {
                uint8_t byteCount = (info.dataPoints.size() + 7) / 8;
                frame.push_back(byteCount);
                
                uint8_t currentByte = 0;
                for (size_t i = 0; i < info.dataPoints.size(); ++i) {
                    if (i % 8 == 0 && i != 0) {
                        frame.push_back(currentByte);
                        currentByte = 0;
                    }
                    if (info.dataPoints[i].value > 0) {
                        currentByte |= (1 << (i % 8));
                    }
                }
                frame.push_back(currentByte);
                break;
            }
                
            // 读寄存器响应
            case ModbusFunctionCode::READ_HOLDING_REGISTERS:
            case ModbusFunctionCode::READ_INPUT_REGISTERS: {
                frame.push_back(info.dataPoints.size() * 2);
                for (const auto& point : info.dataPoints) {
                    frame.push_back(point.value >> 8);
                    frame.push_back(point.value & 0xFF);
                }
                break;
            }
                
            // 写操作响应
            case ModbusFunctionCode::WRITE_SINGLE_COIL:
            case ModbusFunctionCode::WRITE_SINGLE_REGISTER:
            case ModbusFunctionCode::SHGK_WRITE: {
                if (!info.dataPoints.empty()) {
                    const auto& point = info.dataPoints.front();
                    frame.push_back(point.address >> 8);
                    frame.push_back(point.address & 0xFF);
                    frame.push_back(point.value >> 8);
                    frame.push_back(point.value & 0xFF);
                }
                break;
            }
                
            default:
                frame[frame.size() - 1] |= 0x80;
                frame.push_back(0x01); // 非法功能码
                break;
        }
    }
    
    // 更新TCP长度字段
    if (isTCP()) {
        uint16_t length = frame.size() - 6; // 减去MBAP头
        frame[4] = length >> 8;
        frame[5] = length & 0xFF;
    } 
    // 添加RTU CRC
    else {
        uint16_t crc = DriverBase::calculateCRC(frame.data(), frame.size());
        frame.push_back(crc & 0xFF);
        frame.push_back(crc >> 8);
    }
    
    return frame;
}

ModbusFrameInfo DriverModbusS::processRequest(const ModbusFrameInfo& request) {
    ModbusFrameInfo response;
    response.functionCode = request.functionCode;
    response.transactionId = request.transactionId;
    
    if (!database_) {
        response.isException = true;
        response.exceptionCode = 0x04; // 从站设备故障
        return response;
    }
    
    try {
        switch (request.functionCode) {
            // 读操作处理
            case ModbusFunctionCode::READ_COILS: {
                if (!request.dataPoints.empty()) {
                    const auto& point = request.dataPoints.front();
                    if (processReadYX(point.address, point.value, response.dataPoints)) {
                        // 成功读取遥信数据
                    } else {
                        response.isException = true;
                        response.exceptionCode = 0x02; // 非法数据地址
                    }
                }
                break;
            }
                
            case ModbusFunctionCode::READ_HOLDING_REGISTERS: {
                if (!request.dataPoints.empty()) {
                    const auto& point = request.dataPoints.front();
                    if (processReadYC(point.address, point.value, response.dataPoints)) {
                        // 成功读取遥测数据
                    } else {
                        response.isException = true;
                        response.exceptionCode = 0x02; // 非法数据地址
                    }
                }
                break;
            }
                
            // 写操作处理
            case ModbusFunctionCode::WRITE_SINGLE_COIL: {
                if (!request.dataPoints.empty()) {
                    const auto& point = request.dataPoints.front();
                    bool value = (point.value == 0xFF00);
                    if (processWriteYK(point.address, value)) {
                        response.dataPoints = request.dataPoints;
                    } else {
                        response.isException = true;
                        response.exceptionCode = 0x02; // 非法数据地址
                    }
                }
                break;
            }
                
            case ModbusFunctionCode::WRITE_SINGLE_REGISTER: {
                if (!request.dataPoints.empty()) {
                    const auto& point = request.dataPoints.front();
                    if (processWriteYT(point.address, point.value)) {
                        response.dataPoints = request.dataPoints;
                    } else {
                        response.isException = true;
                        response.exceptionCode = 0x02; // 非法数据地址
                    }
                }
                break;
            }
                
            default:
                response.isException = true;
                response.exceptionCode = 0x01; // 非法功能码
        }
    } catch (...) {
        response.isException = true;
        response.exceptionCode = 0x04; // 从站设备故障
    }
    
    return response;
}

ModbusDataPoint DriverModbusS::convertToModbusPoint(const TelemetryPoint& tp) {
    ModbusDataPoint mp;
    mp.address = tp.address;
    // 将浮点值转换为16位整数（实际应用中可能需要更复杂的转换）
    mp.value = static_cast<uint16_t>(tp.value);
    return mp;
}

bool DriverModbusS::processReadYC(uint16_t start, uint16_t count, 
                                 std::vector<ModbusDataPoint>& points) {
    std::vector<TelemetryPoint> ycPoints;
    if (database_->readMultipleYC(start, count, ycPoints)) {
        for (const auto& tp : ycPoints) {
            points.push_back(convertToModbusPoint(tp));
        }
        return true;
    }
    return false;
}

bool DriverModbusS::processReadYX(uint16_t start, uint16_t count, 
                                 std::vector<ModbusDataPoint>& points) {
    std::vector<TelemetryPoint> yxPoints;
    if (database_->readMultipleYX(start, count, yxPoints)) {
        for (const auto& tp : yxPoints) {
            ModbusDataPoint mp;
            mp.address = tp.address;
            mp.value = (tp.value > 0.5) ? 0xFF00 : 0x0000; // 转换为线圈值
            points.push_back(mp);
        }
        return true;
    }
    return false;
}

bool DriverModbusS::processWriteYK(uint16_t address, bool value) {
    return database_->writeYK(address, value);
}

bool DriverModbusS::processWriteYT(uint16_t address, uint16_t value) {
    // 将16位整数转换为浮点数（实际应用中可能需要更复杂的转换）
    double floatValue = static_cast<double>(value);
    return database_->writeYT(address, floatValue);
}