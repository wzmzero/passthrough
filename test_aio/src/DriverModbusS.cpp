#include "DriverModbusS.h"
#include <iostream>
#include <iomanip>

DriverModbusS::DriverModbusS(std::shared_ptr<ChannelBase> channel)
    : DriverBase(channel) {}

void DriverModbusS::run() {
    // 从站的工作线程可以处理一些后台任务，例如更新寄存器值等
    // 在这个简单的实现中，我们不需要做太多事情，主要工作由组帧线程完成
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void DriverModbusS::frame_assembly() {
    std::vector<uint8_t> frame;
    bool in_frame = false;
    auto last_byte_time = std::chrono::steady_clock::now();
    const auto frame_timeout = std::chrono::milliseconds(50);
    
    while (running_) {
        uint8_t byte;
        if (channel_->getReceiveQueue().try_pop(byte)) {
            auto now = std::chrono::steady_clock::now();
            
            // 检查帧超时
            if (in_frame && (now - last_byte_time > frame_timeout)) {
                // 超时，处理当前帧
                process_frame(frame);
                frame.clear();
                in_frame = false;
            }
            
            frame.push_back(byte);
            in_frame = true;
            last_byte_time = now;
        } else {
            // 没有数据，检查帧超时
            if (in_frame) {
                auto now = std::chrono::steady_clock::now();
                if (now - last_byte_time > frame_timeout) {
                    process_frame(frame);
                    frame.clear();
                    in_frame = false;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void DriverModbusS::process_frame(const std::vector<uint8_t>& frame) {
    if (frame.size() < 8) {
        std::cerr << "Frame too short: " << frame.size() << " bytes" << std::endl;
        return;
    }
    
    // 对于Modbus TCP，有7字节的MBAP头
    // 事务标识符（2字节）| 协议标识符（2字节）| 长度（2字节）| 单元标识符（1字节）
    // 然后才是PDU
    
    uint16_t length = (static_cast<uint16_t>(frame[4]) << 8) | frame[5];
    if (frame.size() != 6 + 1 + length) { // MBAP头6字节 + 单元标识符1字节 + PDU长度
        std::cerr << "Invalid frame length: expected " << (6+1+length) 
                  << ", got " << frame.size() << std::endl;
        return;
    }
    
    uint8_t unit_id = frame[6];
    uint8_t function_code = frame[7];
    
    // 只处理读保持寄存器请求
    if (function_code == 0x03) {
        if (frame.size() < 12) {
            std::cerr << "Invalid read request frame" << std::endl;
            return;
        }
        
        uint16_t start_reg = (static_cast<uint16_t>(frame[8]) << 8) | frame[9];
        uint16_t reg_count = (static_cast<uint16_t>(frame[10]) << 8) | frame[11];
        
        std::cout << "Read holding registers: unit=" << (int)unit_id 
                  << ", start=" << start_reg << ", count=" << reg_count << std::endl;
        
        // 构建响应
        std::vector<uint8_t> response;
        
        // MBAP头（复制请求的事务标识符和协议标识符）
        for (int i = 0; i < 6; ++i) {
            response.push_back(frame[i]);
        }
        
        // 单元标识符
        response.push_back(unit_id);
        
        // PDU部分
        response.push_back(0x03); // 功能码
        response.push_back(reg_count * 2); // 字节数
        
        std::vector<uint16_t> registers;
        if (read_callback_) {
            registers = read_callback_(start_reg, reg_count);
        } else {
            // 默认返回0
            for (uint16_t i = 0; i < reg_count; ++i) {
                registers.push_back(0);
            }
        }
        
        for (auto reg : registers) {
            response.push_back(reg >> 8);
            response.push_back(reg & 0xFF);
        }
        
        // 更新MBAP头中的长度字段（PDU长度+1字节单元标识符）
        uint16_t pdu_len = static_cast<uint16_t>(response.size() - 6); // 整个响应减去MBAP头（6字节）
        response[4] = pdu_len >> 8;
        response[5] = pdu_len & 0xFF;
        
        // 发送响应
        if (!channel_->send(response)) {
            std::cerr << "Failed to send Modbus response" << std::endl;
        }
    } else {
        std::cerr << "Unsupported function code: " << (int)function_code << std::endl;
    }
}