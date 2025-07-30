#include "DriverModbus.h"
#include <iostream>
#include <algorithm>

DriverModbus::DriverModbus(bool isMaster) : isMaster(isMaster) {}

void DriverModbus::processData(const std::vector<uint8_t>& data) {
    // 简单的帧处理 - 实际应用中需要更完整的帧解析
    if (data.size() < 4) return; // 最小帧长度
    
    // 检查CRC
    uint16_t receivedCRC = (data[data.size()-2] << 8) | data[data.size()-1];
    uint16_t calculatedCRC = calculateCRC(data.data(), data.size()-2);
    
    if (receivedCRC != calculatedCRC) {
        std::cerr << "Modbus CRC error: received " << receivedCRC 
                  << ", calculated " << calculatedCRC << std::endl;
        return;
    }
    
    processModbusFrame(std::vector<uint8_t>(data.begin(), data.end()-2));
}

void DriverModbus::sendRequest(const ModbusRequest& request) {
    if (!isMaster) {
        std::cerr << "Cannot send requests from slave device" << std::endl;
        return;
    }
    
    if (!channel) {
        std::cerr << "No channel set for sending" << std::endl;
        return;
    }
    
    // 构建Modbus请求帧
    std::vector<uint8_t> frame;
    frame.push_back(request.deviceId);
    frame.push_back(request.functionCode);
    frame.push_back(static_cast<uint8_t>(request.startingAddress >> 8));
    frame.push_back(static_cast<uint8_t>(request.startingAddress & 0xFF));
    frame.push_back(static_cast<uint8_t>(request.quantity >> 8));
    frame.push_back(static_cast<uint8_t>(request.quantity & 0xFF));
    
    // 添加数据（对于写操作）
    if (request.functionCode == 0x06 || request.functionCode == 0x10) {
        frame.insert(frame.end(), request.data.begin(), request.data.end());
    }
    
    // 添加CRC
    uint16_t crc = calculateCRC(frame.data(), frame.size());
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));
    frame.push_back(static_cast<uint8_t>(crc >> 8));
    
    channel->send(frame);
}

void DriverModbus::setResponseCallback(ResponseCallback callback) {
    responseCallback = callback;
}

void DriverModbus::processModbusFrame(const std::vector<uint8_t>& frame) {
    if (frame.size() < 2) return;
    
    uint8_t deviceId = frame[0];
    uint8_t functionCode = frame[1];
    
    // 主站模式：处理响应
    if (isMaster) {
        if (functionCode & 0x80) {
            // 异常响应
            uint8_t errorCode = frame[2];
            std::cerr << "Modbus exception: function " 
                      << static_cast<int>(functionCode & 0x7F)
                      << ", error " << static_cast<int>(errorCode) << std::endl;
        } else {
            // 正常响应
            ModbusRequest request;
            // 这里需要根据之前的请求匹配响应
            // 简化处理：直接传递数据
            std::vector<uint8_t> data(frame.begin() + 2, frame.end());
            if (responseCallback) {
                responseCallback(request, data);
            }
        }
    } 
    // 从站模式：处理请求
    else {
        ModbusRequest request;
        request.deviceId = deviceId;
        request.functionCode = functionCode;
        
        if (frame.size() >= 6) {
            request.startingAddress = (frame[2] << 8) | frame[3];
            request.quantity = (frame[4] << 8) | frame[5];
            
            if (frame.size() > 6) {
                request.data = std::vector<uint8_t>(frame.begin() + 6, frame.end());
            }
        }
        
        // 处理请求并发送响应
        sendResponse(request, {}); // 简化处理
    }
}

void DriverModbus::sendResponse(const ModbusRequest& request, const std::vector<uint8_t>& data) {
    if (isMaster) {
        std::cerr << "Cannot send responses from master device" << std::endl;
        return;
    }
    
    if (!channel) {
        std::cerr << "No channel set for sending" << std::endl;
        return;
    }
    
    std::vector<uint8_t> frame;
    frame.push_back(request.deviceId);
    frame.push_back(request.functionCode);
    
    if (!data.empty()) {
        frame.insert(frame.end(), data.begin(), data.end());
    } else {
        // 对于读操作，返回模拟数据
        if (request.functionCode == 0x03 || request.functionCode == 0x04) {
            uint16_t byteCount = request.quantity * 2;
            frame.push_back(static_cast<uint8_t>(byteCount));
            
            for (uint16_t i = 0; i < request.quantity; i++) {
                uint16_t value = i + request.startingAddress;
                frame.push_back(static_cast<uint8_t>(value >> 8));
                frame.push_back(static_cast<uint8_t>(value & 0xFF));
            }
        }
        // 对于写操作，返回相同的地址和数量
        else if (request.functionCode == 0x06 || request.functionCode == 0x10) {
            frame.push_back(static_cast<uint8_t>(request.startingAddress >> 8));
            frame.push_back(static_cast<uint8_t>(request.startingAddress & 0xFF));
            frame.push_back(static_cast<uint8_t>(request.quantity >> 8));
            frame.push_back(static_cast<uint8_t>(request.quantity & 0xFF));
        }
    }
    
    // 添加CRC
    uint16_t crc = calculateCRC(frame.data(), frame.size());
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));
    frame.push_back(static_cast<uint8_t>(crc >> 8));
    
    channel->send(frame);
}

uint16_t DriverModbus::calculateCRC(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= static_cast<uint16_t>(data[i]);
        
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}