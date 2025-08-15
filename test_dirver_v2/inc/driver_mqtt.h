#pragma once
#include "driver_base.h"
#include <string>
#include <functional>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "logrecord.h"
#include <mqtt/async_client.h>
#include <mqtt/callback.h>

class DriverMQTT : public DriverBase, public mqtt::callback {
public:
    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

    DriverMQTT(const std::string& broker, int port, const std::string& clientId);
    ~DriverMQTT() override;
    
    // 核心接口
    bool open() override;
    void close() override;
    size_t write(const uint8_t* data, size_t len) override;
    
    // MQTT特有接口
    bool publish(const std::string& topic, const std::string& payload, int qos = 0);
    bool subscribe(const std::string& topic, int qos = 0);
    void unsubscribe(const std::string& topic);
    
    // 设置消息回调
    void setMessageCallback(MessageCallback callback);
    
    // 状态查询
    bool isConnected() const;
    
    // MQTT 回调接口
    void connected(const std::string& cause) override;
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;

private:
    void run();
    void tryConnect();
    void reconnect();

    std::unique_ptr<mqtt::async_client> client_;
    mqtt::connect_options connOpts_;
    MessageCallback messageCallback_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> reconnect_needed_{false};
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable cv_;
    
    // 订阅管理
    std::map<std::string, int> subscriptions_;
    
    // 连接参数
    const std::string broker_;
    const int port_;
    const std::string clientId_;
    
    // 重连相关
    std::atomic<int> reconnect_attempts_{0};
    std::atomic<int> reconnect_delay_{1}; // 初始重连延迟(秒)
    const int max_reconnect_delay_{30};    // 最大重连延迟(秒)
};