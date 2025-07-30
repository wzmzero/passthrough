#pragma once

#include "DriverBase.h"
#include <vector>
#include <cstdint>
#include <functional>
#include <mutex>

class DriverModbusS : public DriverBase {
public:
    using ReadCallback = std::function<std::vector<uint16_t>(uint16_t, uint16_t)>;

    DriverModbusS(std::shared_ptr<ChannelBase> channel);
    
    void setReadHoldingRegistersCallback(ReadCallback cb) {
        read_callback_ = std::move(cb);
    }

protected:
    void run() override;
    void frame_assembly() override;
    void process_frame(const std::vector<uint8_t>& frame);
    std::vector<uint8_t> build_response(const std::vector<uint8_t>& request);

    ReadCallback read_callback_;
    std::vector<uint8_t> current_frame_;
    std::mutex frame_mutex_;
};