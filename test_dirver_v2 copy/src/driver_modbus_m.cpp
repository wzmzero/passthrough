#include "driver_modbus_m.h"

DriverModbusM::DriverModbusM(uint8_t slaveAddr, ModbusTransportMode mode) 
    : slaveAddress_(slaveAddr) {
    transportMode_ = mode;
}

bool DriverModbusM::open() { 
    // 硬件初始化代码
    return true; 
}

void DriverModbusM::close() { 
    // 硬件关闭代码 
}

size_t DriverModbusM::write(const uint8_t* data, size_t len) { 
    // 数据发送实现
    return len; 
}

std::vector<uint8_t> DriverModbusM::createReadRequest(
    ModbusFunctionCode func, 
    uint16_t startAddr, 
    uint16_t quantity) 
{
    ModbusFrameInfo info;
    info.functionCode = func;
    
    for (uint16_t i = 0; i < quantity; ++i) {
        info.dataPoints.push_back({static_cast<uint16_t>(startAddr + i), 0});
    }
    
    return createRequestFrame(info);
}

std::vector<uint8_t> DriverModbusM::createWriteRequest(
    ModbusFunctionCode func, 
    uint16_t addr, 
    uint16_t value) 
{
    ModbusFrameInfo info;
    info.functionCode = func;
    info.dataPoints.push_back({addr, value});
    return createRequestFrame(info);
}

std::vector<uint8_t> DriverModbusM::createSHGKWriteRequest(uint16_t addr, uint32_t value) {
    ModbusFrameInfo info;
    info.functionCode = ModbusFunctionCode::SHGK_WRITE;
    // 只存储低16位
    info.dataPoints.push_back({addr, static_cast<uint16_t>(value & 0xFFFF)});
    return createRequestFrame(info);
}

std::vector<uint8_t> DriverModbusM::createRequestFrame(const ModbusFrameInfo& info) {
    std::vector<uint8_t> frame;
    
    // TCP头部
    if (isTCP()) {
        frame.push_back(transactionId_ >> 8);
        frame.push_back(transactionId_ & 0xFF);
        transactionId_++;
        
        frame.push_back(0x00); // 协议ID高字节
        frame.push_back(0x00); // 协议ID低字节
        // 长度占位符
        frame.push_back(0x00); 
        frame.push_back(0x00);
        frame.push_back(slaveAddress_);
    }
    // RTU头部
    else {
        frame.push_back(slaveAddress_);
    }
    
    frame.push_back(static_cast<uint8_t>(info.functionCode));
    
    // 构建不同功能码的报文
    switch (info.functionCode) {
        // 读请求
        case ModbusFunctionCode::READ_COILS:
        case ModbusFunctionCode::READ_HOLDING_REGISTERS: {
            if (!info.dataPoints.empty()) {
                uint16_t startAddr = info.dataPoints.front().address;
                uint16_t quantity = static_cast<uint16_t>(info.dataPoints.size());
                
                frame.push_back(startAddr >> 8);
                frame.push_back(startAddr & 0xFF);
                frame.push_back(quantity >> 8);
                frame.push_back(quantity & 0xFF);
            }
            break;
        }
            
        // 写请求
        case ModbusFunctionCode::WRITE_SINGLE_COIL:
        case ModbusFunctionCode::WRITE_SINGLE_REGISTER: {
            if (!info.dataPoints.empty()) {
                const auto& point = info.dataPoints.front();
                frame.push_back(point.address >> 8);
                frame.push_back(point.address & 0xFF);
                frame.push_back(point.value >> 8);
                frame.push_back(point.value & 0xFF);
            }
            break;
        }
            
        // 自定义写请求
        case ModbusFunctionCode::SHGK_WRITE: {
            if (!info.dataPoints.empty()) {
                const auto& point = info.dataPoints.front();
                frame.push_back(point.address >> 8);
                frame.push_back(point.address & 0xFF);
                // 32位值处理
                uint32_t value = point.value;
                frame.push_back(value >> 24);
                frame.push_back(value >> 16);
                frame.push_back(value >> 8);
                frame.push_back(value & 0xFF);
            }
            break;
        }
            
        default:
            frame.clear();
            break;
    }
    
    // 更新TCP长度字段
    if (isTCP() && !frame.empty()) {
        uint16_t length = frame.size() - 6; // 减去MBAP头
        frame[4] = length >> 8;
        frame[5] = length & 0xFF;
    }
    // 添加RTU CRC
    else if (isRTU()) {
        uint16_t crc = DriverBase::calculateCRC(frame.data(), frame.size());
        frame.push_back(crc & 0xFF);
        frame.push_back(crc >> 8);
    }
    
    return frame;
}

bool DriverModbusM::parseResponse(const uint8_t* frame, size_t len, ModbusFrameInfo& info) {
    // 基础长度检查
    if (len < (isTCP() ? 9 : 5)) return false;
    
    size_t index = 0;
    
    // TCP头部处理
    if (isTCP()) {
        info.transactionId = (frame[index++] << 8) | frame[index++];
        // 跳过协议ID
        index += 2;  
        uint16_t length = (frame[index++] << 8) | frame[index++];
        // 长度校验
        if (len < index + length + 1) return false;
        uint8_t unitId = frame[index++];
        // 地址校验
        if (unitId != slaveAddress_) return false;
    }
    // RTU头部处理
    else {
        uint8_t address = frame[index++];
        // 地址校验
        if (address != slaveAddress_) return false;
        // CRC校验
        uint16_t crc_calculated = DriverBase::calculateCRC(frame, len - 2);
        uint16_t crc_frame = (static_cast<uint16_t>(frame[len-1]) << 8) | frame[len-2];
        if (crc_calculated != crc_frame) {
            return false;
        }
    }
    
    // 功能码解析
    uint8_t funcByte = frame[index++];
    info.isException = (funcByte & 0x80) != 0;
    info.functionCode = static_cast<ModbusFunctionCode>(funcByte & 0x7F);
    
    // 异常响应处理
    if (info.isException) {
        if (len > index) info.exceptionCode = frame[index];
        return true;
    }
    
    // 解析不同功能码的响应
    switch (info.functionCode) {
        // 读线圈响应
        case ModbusFunctionCode::READ_COILS: {
            if (len < index + 1) return false;
            uint8_t byteCount = frame[index++];
            uint16_t bitCounter = 0;
            
            for (uint8_t i = 0; i < byteCount; ++i) {
                uint8_t byteVal = frame[index++];
                for (uint8_t bit = 0; bit < 8; ++bit) {
                    info.dataPoints.push_back({
                        bitCounter++,
                        static_cast<uint16_t>((byteVal >> bit) & 0x01)
                    });
                }
            }
            break;
        }
            
        // 读寄存器响应
        case ModbusFunctionCode::READ_HOLDING_REGISTERS: {
            if (len < index + 1) return false;
            uint8_t byteCount = frame[index++];
            if (byteCount % 2 != 0) return false;
            
            for (uint8_t i = 0; i < byteCount; i += 2) {
                uint16_t value = (frame[index++] << 8) | frame[index++];
                info.dataPoints.push_back({static_cast<uint16_t>(i/2), value});
            }
            break;
        }
            
        // 写操作响应
        case ModbusFunctionCode::WRITE_SINGLE_COIL:
        case ModbusFunctionCode::WRITE_SINGLE_REGISTER: {
            if (len < index + 4) return false;
            uint16_t addr = (frame[index++] << 8) | frame[index++];
            uint16_t value = (frame[index++] << 8) | frame[index++];
            info.dataPoints.push_back({addr, value});
            break;
        }
            
        default:
            return false;
    }
    
    return true;
}