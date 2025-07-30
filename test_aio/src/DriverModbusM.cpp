#include "DriverModbusM.h"
#include <iostream>
#include <iomanip>

DriverModbusM::DriverModbusM(std::shared_ptr<ChannelBase> channel)
    : DriverBase(channel) {
    // 设置默认请求
    current_request_ = {1, 0, 5, std::chrono::milliseconds(3000)};
}

void DriverModbusM::setReadRequest(const ReadRequest& request) {
    std::lock_guard<std::mutex> lock(request_mutex_);
    current_request_ = request;
}

std::vector<uint16_t> DriverModbusM::getRegisters() const {
    return registers_;
}

void DriverModbusM::run() {
    while (running_) {
        ReadRequest req;
        {
            std::lock_guard<std::mutex> lock(request_mutex_);
            req = current_request_;
        }
        
        // 构建并发送请求
        auto request_data = build_read_request();
        if (!channel_->send(request_data)) {
            std::cerr << "Failed to send Modbus request" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        
        std::cout << "Modbus request sent" << std::endl;
        
        // 等待响应
        auto start_time = std::chrono::steady_clock::now();
        response_received_ = false;
        
        while (!response_received_ && 
               std::chrono::steady_clock::now() - start_time < req.timeout) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (!response_received_) {
            std::cerr << "Modbus response timeout" << std::endl;
        }
        
        // 等待下一次请求
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void DriverModbusM::frame_assembly() {
    std::vector<uint8_t> frame;
    bool in_frame = false;
    auto last_byte_time = std::chrono::steady_clock::now();
    const auto frame_timeout = std::chrono::milliseconds(10);
    
    while (running_) {
        uint8_t byte;
        if (channel_->getReceiveQueue().try_pop(byte)) {
            auto now = std::chrono::steady_clock::now();
            
            // 检查帧超时
            if (in_frame && (now - last_byte_time > frame_timeout)) {
                // 超时，处理当前帧
                if (parse_response(frame)) {
                    response_received_ = true;
                }
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
                    if (parse_response(frame)) {
                        response_received_ = true;
                    }
                    frame.clear();
                    in_frame = false;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

std::vector<uint8_t> DriverModbusM::build_read_request() {
    ReadRequest req;
    {
        std::lock_guard<std::mutex> lock(request_mutex_);
        req = current_request_;
    }
    
    std::vector<uint8_t> request;
    
    // MBAP头
    static uint16_t transaction_id = 1;
    request.push_back(transaction_id >> 8);
    request.push_back(transaction_id & 0xFF);
    transaction_id++;
    
    request.push_back(0x00); // Protocol ID (high)
    request.push_back(0x00); // Protocol ID (low)
    
    // Length (will be set later)
    request.push_back(0x00);
    request.push_back(0x00);
    
    request.push_back(req.unit_id); // Unit ID
    
    // PDU
    request.push_back(0x03); // Function code
    request.push_back(req.start_reg >> 8); // Starting address high
    request.push_back(req.start_reg & 0xFF); // Starting address low
    request.push_back(req.reg_count >> 8); // Quantity high
    request.push_back(req.reg_count & 0xFF); // Quantity low
    
    // Set length (PDU length + 1 for unit ID)
    uint16_t length = static_cast<uint16_t>(request.size() - 6);
    request[4] = length >> 8;
    request[5] = length & 0xFF;
    
    return request;
}

bool DriverModbusM::parse_response(const std::vector<uint8_t>& frame) {
    if (frame.size() < 9) {
        std::cerr << "Invalid Modbus response: frame too short" << std::endl;
        return false;
    }
    
    // Check function code
    if (frame[7] != 0x03) {
        if (frame[7] & 0x80) {
            std::cerr << "Modbus exception: code " << static_cast<int>(frame[8]) << std::endl;
        } else {
            std::cerr << "Unexpected function code in response: " 
                      << static_cast<int>(frame[7]) << std::endl;
        }
        return false;
    }
    
    // Get byte count
    uint8_t byte_count = frame[8];
    if (byte_count % 2 != 0 || frame.size() < 9 + byte_count) {
        std::cerr << "Invalid byte count in Modbus response" << std::endl;
        return false;
    }
    
    // Extract register values
    registers_.clear();
    for (int i = 0; i < byte_count; i += 2) {
        uint16_t reg = (static_cast<uint16_t>(frame[9 + i]) << 8) | frame[10 + i];
        registers_.push_back(reg);
    }
    
    std::cout << "Received Modbus response with " << registers_.size() << " registers" << std::endl;
    return true;
}