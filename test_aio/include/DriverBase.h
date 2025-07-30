#pragma once

#include "ChannelBase.h"
#include <thread>
#include <atomic>
#include <memory>
#include <functional>

class DriverBase {
public:
    using FrameCallback = std::function<void(const std::vector<uint8_t>&)>;
    
    DriverBase(std::shared_ptr<ChannelBase> channel);
    virtual ~DriverBase();
    
    virtual bool start();
    virtual void stop();
    
    void setFrameCallback(FrameCallback cb) { frame_callback_ = std::move(cb); }

protected:
    virtual void run() = 0; // 工作线程函数
    virtual void frame_assembly() = 0; // 组帧线程函数
    
    std::shared_ptr<ChannelBase> channel_;
    std::atomic<bool> running_;
    std::thread work_thread_;
    std::thread frame_thread_;
    FrameCallback frame_callback_;
};