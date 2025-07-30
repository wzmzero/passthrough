// UdpServerEndpoint.cpp
#include "UdpServerEndpoint.hpp"

UdpServerEndpoint::UdpServerEndpoint(int port) : port_(port) {}

UdpServerEndpoint::~UdpServerEndpoint() {
    stop();
}

bool UdpServerEndpoint::open() {
    if (server_) return true;
    
    server_ = std::make_unique<NetworkServer>(0, port_); // TCP port 0
    server_->startUdpServer();
    return true;
}

void UdpServerEndpoint::close() {
    if (server_) {
        server_->stop();
        server_.reset();
    }
}

bool UdpServerEndpoint::isOpen() const {
    return server_ != nullptr;
}

bool UdpServerEndpoint::write(const std::string& data) {
    broadcast(data);
    return true;
}

void UdpServerEndpoint::start() {
    if (running_) return;
    
    if (!open()) {
        callErrorCallback("Failed to start UDP server");
        return;
    }
    
    server_->setUdpMessageHandler([this](const sockaddr_in& client_addr, const std::string& msg) {
        callDataCallback(msg);
        return true; // Handled
    });
    
    running_ = true;
    server_thread_ = std::thread(&UdpServerEndpoint::serverThreadFunc, this);
}

void UdpServerEndpoint::stop() {
    if (!running_) return;
    
    running_ = false;
    close();
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void UdpServerEndpoint::serverThreadFunc() {
    while (running_ && server_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

std::string UdpServerEndpoint::getConfigString() const {
    return "UDP Server: listening on port " + std::to_string(port_);
}

void UdpServerEndpoint::broadcast(const std::string& data) {
    if (server_) {
        server_->broadcastUdpMessage(data);
    }
}