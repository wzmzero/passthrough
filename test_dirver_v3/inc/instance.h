#ifndef INSTANCE_H
#define INSTANCE_H

#include "common.h"
#include "driver_base.h"
#include "endpoint.h"
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

class Instance {
public:
    explicit Instance(const InstanceParm& param);
    ~Instance();
    
    bool open();
    void close();
    
    std::string getName() const { return param_.name; }
    
    void setEndpointLogCallback(std::function<void(const std::string&)> callback);
    void setEndpointErrorCallback(std::function<void(const std::string&)> callback);

private:
    void createEndpoint();
    void createDriver();
    void sendThreadFunc();  // 发送线程函数
    
    InstanceParm param_;
    std::unique_ptr<DriverBase> driver_;
    std::unique_ptr<Endpoint> endpoint_;
    
    std::thread send_thread_;         // 发送线程
    std::atomic<bool> running_{false}; // 线程运行标志
    std::atomic<bool> thread_exit_{false}; // 线程退出标志
};

#endif // INSTANCE_H