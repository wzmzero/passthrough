// TcpClientEndpoint.cpp
#include "TcpClientEndpoint.hpp"

bool TcpClientEndpoint::open() {
    if (client_) return true;
    
    client_ = std::make_unique<NetworkClient>(NetworkClient::TCP, server_ip_, port_);
    if (!client_->connect()) {
        callErrorCallback("Failed to connect to TCP server");
        return false;
    }
    return true;
}

void TcpClientEndpoint::close() {
    if (client_) {
        client_->closeConnection();
        client_.reset();
    }
}

bool TcpClientEndpoint::isOpen() const {
    return client_ != nullptr;
}

bool TcpClientEndpoint::write(const std::string& data) {
    if (!isOpen()) return false;
    return client_->sendMessage(data);
}

void TcpClientEndpoint::start() {
    if (running_) return;
    
    if (!open()) {
        callErrorCallback("TCP client failed to open");
        return;
    }
    
    running_ = true;
    receive_thread_ = std::thread(&TcpClientEndpoint::receiveThreadFunc, this);
}

void TcpClientEndpoint::stop() {
    if (!running_) return;
    
    running_ = false;
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    close();
}

void TcpClientEndpoint::receiveThreadFunc() {
    while (running_) {
        if (!isOpen() && !open()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        
        std::string data = client_->receiveMessage();
        if (!data.empty()) {
            callDataCallback(data);
        } else if (!isOpen()) {
            callErrorCallback("TCP connection lost");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::string TcpClientEndpoint::getConfigString() const {
    return "TCP Client: " + server_ip_ + ":" + std::to_string(port_);
}