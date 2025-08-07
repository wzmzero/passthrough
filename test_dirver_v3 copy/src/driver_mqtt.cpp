#include "driver_mqtt.h"
#include <iostream>
#include <chrono>
#include <mqtt/exception.h>

DriverMQTT::DriverMQTT(const std::string& broker, int port, const std::string& clientId)
    : broker_(broker), port_(port), clientId_(clientId) {
    // 构建服务器URI
    std::string serverURI = "tcp://" + broker + ":" + std::to_string(port);
    
    // 创建MQTT客户端
    client_ = std::make_unique<mqtt::async_client>(serverURI, clientId);
    
    // 设置回调
    client_->set_callback(*this);
    
    // 设置连接选项
    connOpts_ = mqtt::connect_options_builder()
        .clean_session(true)
        .automatic_reconnect(false) // 使用自己的重连逻辑
        .finalize();
    
    LOG_INFO("MQTT Driver created: serverURI=%s, clientId=%s", 
             serverURI.c_str(), clientId.c_str());
}

DriverMQTT::~DriverMQTT() {
    close();
}

bool DriverMQTT::open() {
    if (running_) return true;
    
    running_ = true;
    worker_ = std::thread(&DriverMQTT::run, this);
    return true;
}

void DriverMQTT::close() {
    if (!running_) return;
    
    running_ = false;
    cv_.notify_all();
    
    try {
        if (client_ && isConnected()) {
            client_->disconnect()->wait();
        }
    } catch (const mqtt::exception& e) {
        LOG_ERROR("MQTT disconnect error: %s", e.what());
    }
    
    if (worker_.joinable()) {
        worker_.join();
    }
    
    LOG_INFO("MQTT driver closed");
}

size_t DriverMQTT::write(const uint8_t* data, size_t len) {
    // MQTT使用专门的publish接口
    return 0;
}

bool DriverMQTT::publish(const std::string& topic, const std::string& payload, int qos) {
    if (!isConnected()) {
        LOG_WARNING("MQTT not connected, cannot publish");
        return false;
    }
    
    try {
        mqtt::message_ptr pubmsg = mqtt::make_message(topic, payload);
        pubmsg->set_qos(qos);
        
        client_->publish(pubmsg)->wait();
        LOG_INFO("MQTT publish: [%s] %s", topic.c_str(), payload.c_str());
        return true;
    } catch (const mqtt::exception& e) {
        LOG_ERROR("Publish error: %s", e.what());
        reconnect_needed_ = true;
        return false;
    }
}

bool DriverMQTT::subscribe(const std::string& topic, int qos) {
    if (!isConnected()) {
        LOG_WARNING("MQTT not connected, cannot subscribe");
        return false;
    }
    
    try {
        client_->subscribe(topic, qos)->wait();
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            subscriptions_[topic] = qos;
        }
        
        LOG_INFO("MQTT subscribed to topic: %s (QoS: %d)", topic.c_str(), qos);
        return true;
    } catch (const mqtt::exception& e) {
        LOG_ERROR("Subscribe error: %s", e.what());
        return false;
    }
}

void DriverMQTT::unsubscribe(const std::string& topic) {
    if (!isConnected()) return;
    
    try {
        client_->unsubscribe(topic)->wait();
        
        std::lock_guard<std::mutex> lock(mutex_);
        subscriptions_.erase(topic);
        
        LOG_INFO("MQTT unsubscribed from topic: %s", topic.c_str());
    } catch (const mqtt::exception& e) {
        LOG_ERROR("Unsubscribe error: %s", e.what());
    }
}

void DriverMQTT::setMessageCallback(MessageCallback callback) {
    messageCallback_ = callback;
}

bool DriverMQTT::isConnected() const {
    return connected_;
}

// MQTT 回调实现
void DriverMQTT::connected(const std::string& cause) {
    connected_ = true;
    reconnect_attempts_ = 0;
    reconnect_delay_ = 1;
    LOG_INFO("Connected to MQTT broker: %s", cause.c_str());
    
    // 重新订阅所有主题
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& sub : subscriptions_) {
        try {
            client_->subscribe(sub.first, sub.second)->wait();
            LOG_INFO("Resubscribed to topic: %s (QoS: %d)", sub.first.c_str(), sub.second);
        } catch (const mqtt::exception& e) {
            LOG_ERROR("Resubscribe error: %s", e.what());
        }
    }
}

void DriverMQTT::connection_lost(const std::string& cause) {
    LOG_WARNING("Connection lost: %s", cause.c_str());
    connected_ = false;
    reconnect_needed_ = true;
    cv_.notify_one();
}

void DriverMQTT::message_arrived(mqtt::const_message_ptr msg) {
    if (messageCallback_) {
        messageCallback_(msg->get_topic(), msg->get_payload_str());
    }
}

void DriverMQTT::delivery_complete(mqtt::delivery_token_ptr token) {
    LOG_DEBUG("Message delivery complete for token: %d", token->get_message_id());
}

void DriverMQTT::run() {
    LOG_INFO("MQTT worker thread started");
    
    while (running_) {
        tryConnect();
        
        // 等待连接事件或关闭信号
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] {
            return reconnect_needed_ || !running_;
        });
        
        if (!running_) break;
        
        if (reconnect_needed_) {
            reconnect_needed_ = false;
            reconnect();
        }
    }
    
    LOG_INFO("MQTT worker thread exited");
}

void DriverMQTT::tryConnect() {
    if (connected_ || !running_) return;
    
    LOG_INFO("Connecting to MQTT broker: %s:%d", broker_.c_str(), port_);
    
    try {
        client_->connect(connOpts_)->wait();
        connected_ = true;
        reconnect_attempts_ = 0;
        reconnect_delay_ = 1;
    } catch (const mqtt::exception& e) {
        LOG_ERROR("Connection failed: %s", e.what());
        connected_ = false;
        reconnect_attempts_++;
        reconnect_delay_ = std::min(max_reconnect_delay_, reconnect_delay_ * 2);
        
        LOG_WARNING("Connection attempt %d failed. Retrying in %d seconds...", 
                   reconnect_attempts_.load(), reconnect_delay_.load());
        
        // 等待重连延迟
        for (int i = 0; i < reconnect_delay_ && running_; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void DriverMQTT::reconnect() {
    if (!running_) return;
    
    if (connected_) {
        try {
            client_->disconnect()->wait();
        } catch (const mqtt::exception& e) {
            LOG_ERROR("Disconnect error during reconnect: %s", e.what());
        }
        connected_ = false;
    }
    
    tryConnect();
}