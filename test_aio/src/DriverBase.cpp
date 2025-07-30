#include "DriverBase.h"

DriverBase::DriverBase(std::shared_ptr<ChannelBase> channel)
    : channel_(channel), running_(false) {}

DriverBase::~DriverBase() {
    stop();
}

bool DriverBase::start() {
    if (running_) return true;
    running_ = true;
    
    // 启动通道
    if (!channel_->start()) {
        return false;
    }
    
    // 启动工作线程和组帧线程
    work_thread_ = std::thread(&DriverBase::run, this);
    frame_thread_ = std::thread(&DriverBase::frame_assembly, this);
    return true;
}

void DriverBase::stop() {
    running_ = false;
    
    if (work_thread_.joinable()) {
        work_thread_.join();
    }
    
    if (frame_thread_.joinable()) {
        frame_thread_.join();
    }
    
    channel_->stop();
}