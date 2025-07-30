#pragma once
#include <vector>
#include <functional>
#include <memory>
#include "ChannelBase.h"

struct ModbusRequest {
    uint8_t functionCode;
    uint16_t startingAddress;
    uint16_t quantity;
    std::vector<uint8_t> data;
    uint8_t deviceId;
};

using ResponseCallback = std::function<void(const ModbusRequest&, const std::vector<uint8_t>&)>;

class DriverBase {
public:
    virtual ~DriverBase() = default;
    
    virtual void processData(const std::vector<uint8_t>& data) = 0;
    virtual void sendRequest(const ModbusRequest& request) = 0;
    virtual void setResponseCallback(ResponseCallback callback) = 0;
    
    void setChannel(std::shared_ptr<ChannelBase> channel) {
        this->channel = channel;
        if (channel) {
            channel->setReceiveCallback([this](const std::vector<uint8_t>& data) {
                this->processData(data);
            });
        }
    }

protected:
    std::shared_ptr<ChannelBase> channel;
    ResponseCallback responseCallback;
};