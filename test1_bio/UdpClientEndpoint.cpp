// UdpClientEndpoint.cpp
#include "UdpClientEndpoint.hpp"

bool UdpClientEndpoint::open() {
    if (client_ && client_->getProtocol() == NetworkClient::UDP) return true;
    
    client_ = std::make_unique<NetworkClient>(NetworkClient::UDP, server_ip_, port_);
    if (!client_->connect()) {
        callErrorCallback("Failed to initialize UDP client");
        return false;
    }
    return true;
}

void UdpClientEndpoint::close() {
    if (client_) {
        client_->closeConnection();
        client_.reset();
    }
}

bool UdpClientEndpoint::isOpen() const {
    return client_ != nullptr;
}

bool UdpClientEndpoint::write(const std::string& data) {
    if (!isOpen()) return false;
    return client_->sendMessage(data);
}

void UdpClientEndpoint::start() {
    if (running_) return;
    
    if (!open()) {
        callErrorCallback("UDP client failed to open");
        return;
    }
    
    running_ = true;
    receive_thread_ = std::thread(&UdpClientEndpoint::receiveThreadFunc, this);
}

void UdpClientEndpoint::stop() {
    if (!running_) return;
    
    running_ = false;
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    close();
}

void UdpClientEndpoint::receiveThreadFunc() {
    while (running_) {
        if (!isOpen() && !open()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        
        std::string data = client_->receiveMessage();
        if (!data.empty()) {
            callDataCallback(data);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::string UdpClientEndpoint::getConfigString() const {
    return "UDP Client: " + server_ip_ + ":" + std::to_string(port_);
}