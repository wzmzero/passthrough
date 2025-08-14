// driver_modbus_s.cpp
#include "driver_modbus_s.h"
#include <algorithm>
#include <stdexcept>

DriverModbusS::DriverModbusS(uint8_t address, ModbusTransportMode mode) 
    : address_(address) {
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
    
    try {
        switch (request.functionCode) {
            // 读操作处理
            case ModbusFunctionCode::READ_COILS:
                for (const auto& point : request.dataPoints) {
                    response.dataPoints.push_back({point.address, getCoil(point.address)});
                }
                break;
                
            case ModbusFunctionCode::READ_HOLDING_REGISTERS:
                for (const auto& point : request.dataPoints) {
                    response.dataPoints.push_back({point.address, getHoldingRegister(point.address)});
                }
                break;
                
            // 写操作处理
            case ModbusFunctionCode::WRITE_SINGLE_COIL:
                if (!request.dataPoints.empty()) {
                    const auto& point = request.dataPoints.front();
                    setCoil(point.address, point.value > 0);
                    response.dataPoints = request.dataPoints;
                }
                break;
                
            case ModbusFunctionCode::WRITE_SINGLE_REGISTER:
                if (!request.dataPoints.empty()) {
                    const auto& point = request.dataPoints.front();
                    setHoldingRegister(point.address, point.value);
                    response.dataPoints = request.dataPoints;
                }
                break;
                
            case ModbusFunctionCode::SHGK_WRITE:
                for (const auto& point : request.dataPoints) {
                    setHoldingRegister(point.address, point.value);
                }
                response.dataPoints = request.dataPoints;
                break;
                
            default:
                throw std::runtime_error("Unsupported function code");
        }
    } catch (...) {
        response.isException = true;
        response.exceptionCode = 0x04; // 从站设备故障
    }
    
    return response;
}

// 内存映射访问实现
bool DriverModbusS::getCoil(uint16_t addr) const {
    auto it = coils_.find(addr);
    return it != coils_.end() ? it->second : false;
}

uint16_t DriverModbusS::getHoldingRegister(uint16_t addr) const {
    auto it = holdings_.find(addr);
    return it != holdings_.end() ? it->second : 0;
}