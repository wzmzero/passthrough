#pragma once

#include "DriverBase.h"
#include <vector>
#include <cstdint>
#include <chrono>
#include <mutex>

class DriverModbusM : public DriverBase {
public:
    struct ReadRequest {
        uint8_t unit_id;
        uint16_t start_reg;
        uint16_t reg_count;
        std::chrono::milliseconds timeout;
    };
    
    DriverModbusM(std::shared_ptr<ChannelBase> channel);
    
    void setReadRequest(const ReadRequest& request);
    std::vector<uint16_t> getRegisters() const;
    
protected:
    void run() override;
    void frame_assembly() override;
    
    std::vector<uint8_t> build_read_request();
    bool parse_response(const std::vector<uint8_t>& frame);
    
    ReadRequest current_request_;
    mutable std::mutex request_mutex_;
    std::vector<uint16_t> registers_;
    std::atomic<bool> response_received_{false};
};