#pragma once
#include "DriverBase.h"
#include <mutex>
#include <queue>
#include <condition_variable>
#include <cstdint>

class DriverModbus : public DriverBase {
public:
    DriverModbus(bool isMaster = true);
    
    void processData(const std::vector<uint8_t>& data) override;
    void sendRequest(const ModbusRequest& request) override;
    void setResponseCallback(ResponseCallback callback) override;
    
private:
    void processModbusFrame(const std::vector<uint8_t>& frame);
    uint16_t calculateCRC(const uint8_t* data, size_t length);
    void sendResponse(const ModbusRequest& request, const std::vector<uint8_t>& data);
    
    bool isMaster;
};