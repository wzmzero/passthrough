// ChannelBase.h
#pragma once
#include <vector>
#include <functional>
#include <memory>

using ReceiveCallback = std::function<void(const std::vector<uint8_t>&)>;

class ChannelBase {
public:
    virtual ~ChannelBase() = default;
    
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool send(const std::vector<uint8_t>& data) = 0;
    
    void setReceiveCallback(ReceiveCallback callback) {
        receiveCallback = callback;
    }

protected:
    ReceiveCallback receiveCallback;
};