#include "instance.h"
#include "driver_modbus_m.h"
#include "tcp_client_endpoint.h"
#include <iostream>
#include <chrono>

Instance::Instance(const InstanceParm& param) 
    : param_(param) {
    createEndpoint();
    createDriver();
}

Instance::~Instance() {
    close();
}

bool Instance::open() {
    if (!endpoint_) {
        std::cerr << "[" << param_.name << "] Endpoint not created" << std::endl;
        return false;
    }
    
    if (!driver_) {
        std::cerr << "[" << param_.name << "] Driver not created" << std::endl;
        return false;
    }
    
    // 打开端点
    if (!endpoint_->open()) {
        std::cerr << "[" << param_.name << "] Failed to open endpoint" << std::endl;
        return false;
    }
    
    // 设置端点数据接收回调
    endpoint_->setDataCallback([this](const uint8_t* data, size_t len) {
        if (driver_) {
            // 将接收到的数据传递给驱动
            driver_->write(data, len);
            
            // 调试输出
            std::vector<uint8_t> frame(data, data + len);
            std::cout << "[" << param_.name << "] Received data: " 
                      << hexStr(frame) << std::endl;
        }
    });
    
    // 打开驱动
    if (!driver_->open()) {
        std::cerr << "[" << param_.name << "] Failed to open driver" << std::endl;
        endpoint_->close();
        return false;
    }
    
    // 启动发送线程
    running_ = true;
    thread_exit_ = false;
    send_thread_ = std::thread(&Instance::sendThreadFunc, this);
    
    return true;
}

void Instance::close() {
    // 通知线程退出
    thread_exit_ = true;
    
    // 等待线程结束
    if (send_thread_.joinable()) {
        send_thread_.join();
    }
    
    if (driver_) {
        driver_->close();
    }
    
    if (endpoint_) {
        endpoint_->close();
    }
    
    running_ = false;
}

void Instance::sendThreadFunc() {
    while (!thread_exit_) {
        if (driver_ && endpoint_) {
            // 直接访问驱动中的发送队列和互斥锁
            std::lock_guard<std::mutex> lock(dynamic_cast<DriverModbusM*>(driver_.get())->sendMutex_);
            
            auto& sendqueue = dynamic_cast<DriverModbusM*>(driver_.get())->sendqueue_;
            while (!sendqueue.empty()) {
                auto frame = sendqueue.front();
                sendqueue.pop();
                
                if (endpoint_->isConnected()) {
                    endpoint_->write(frame.data(), frame.size());
                    std::cout << "[" << param_.name << "] Sent frame: " 
                              << hexStr(frame) << std::endl;
                } else {
                    std::cerr << "[" << param_.name << "] Endpoint not connected" << std::endl;
                }
            }
        }
        
        // 短暂休眠，避免过度占用CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Instance::setEndpointLogCallback(std::function<void(const std::string&)> callback) {
    if (endpoint_) {
        endpoint_->setLogCallback(callback);
    }
}

void Instance::setEndpointErrorCallback(std::function<void(const std::string&)> callback) {
    if (endpoint_) {
        endpoint_->setErrorCallback(callback);
    }
}

void Instance::createEndpoint() {
    auto& epConfig = param_.channelParam;
    
    if (epConfig.type == "tcp_client") {
        endpoint_ = std::make_unique<TcpClientEndpoint>(epConfig.ip, epConfig.port);
        std::cout << "[" << param_.name << "] Created TCP endpoint: " 
                  << epConfig.ip << ":" << epConfig.port << std::endl;
    } else {
        std::cerr << "[" << param_.name << "] Unsupported endpoint type: " 
                  << epConfig.type << std::endl;
    }
}

void Instance::createDriver() {
    if (param_.driverParam.type == MODBUS_M) {
        driver_ = std::make_unique<DriverModbusM>(
            param_.vecDevInfo, 
            param_.driverParam.mModbus_param
        );
        std::cout << "[" << param_.name << "] Created Modbus driver" << std::endl;
    } else {
        std::cerr << "[" << param_.name << "] Unsupported driver type: " 
                  << static_cast<int>(param_.driverParam.type) << std::endl;
    }
}