#ifndef INSTANCE_H
#define INSTANCE_H

#include "common.h"
#include "driver_modbus_m.h"
#include "database.h"
#include "tcp_client_endpoint.h"
#include <memory>
#include <functional>
#include <atomic>

class Instance {
public:
    Instance(const InstanceParm& param);
    ~Instance();
    
    bool open();
    void close();
    void processSendQueue();
    
    // 获取实例名称
    std::string getName() const { return param_.name; }
    
    // 设置端点日志回调
    void setEndpointLogCallback(std::function<void(const std::string&)> callback) {
        if (endpoint_) {
            endpoint_->setLogCallback(callback);
        }
    }
    
    // 设置端点错误回调
    void setEndpointErrorCallback(std::function<void(const std::string&)> callback) {
        if (endpoint_) {
            endpoint_->setErrorCallback(callback);
        }
    }

private:
    void createEndpoint();
    void createDriver();
    
    InstanceParm param_;
    std::unique_ptr<DriverBase> driver_;
    std::unique_ptr<Endpoint> endpoint_;
};

#endif // INSTANCE_H