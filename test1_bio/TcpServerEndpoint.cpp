// TcpServerEndpoint.cpp
#include "TcpServerEndpoint.hpp"
#include <iostream>

TcpServerEndpoint::TcpServerEndpoint(int port) : port_(port) {}

TcpServerEndpoint::~TcpServerEndpoint() {
    stop();
}

bool TcpServerEndpoint::open() {
    if (server_) return true;
    
    server_ = std::make_unique<NetworkServer>(port_, 0); // UDP port 0
    server_->startTcpServer();
    return true;
}

void TcpServerEndpoint::close() {
    if (server_) {
        server_->stop();
        server_.reset();
    }
}

bool TcpServerEndpoint::isOpen() const {
    return server_ != nullptr;
}

bool TcpServerEndpoint::write(const std::string& data) {
    // Default broadcast
    broadcast(data);
    return true;
}

void TcpServerEndpoint::start() {
    if (running_) return;
    
    if (!open()) {
        callErrorCallback("Failed to start TCP server");
        return;
    }
    
    server_->setTcpMessageHandler([this](int client_socket, const std::string& msg) {
        onClientMessage(client_socket, msg);
        return true; // Handled
    });
    
    running_ = true;
    server_thread_ = std::thread(&TcpServerEndpoint::serverThreadFunc, this);
}

void TcpServerEndpoint::stop() {
    if (!running_) return;
    
    running_ = false;
    close();
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& [id, thread] : client_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    client_threads_.clear();
}

void TcpServerEndpoint::serverThreadFunc() {
    while (running_ && server_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void TcpServerEndpoint::onClientMessage(int client_socket, const std::string& data) {
    callDataCallback(data);
}

std::string TcpServerEndpoint::getConfigString() const {
    return "TCP Server: listening on port " + std::to_string(port_);
}

void TcpServerEndpoint::broadcast(const std::string& data) {
    if (server_) {
        server_->broadcastTcpMessage(data);
    }
}

bool TcpServerEndpoint::sendToClient(int client_id, const std::string& data) {
    // Implementation would require mapping client_id to socket
    // For simplicity, we'll broadcast in this example
    broadcast(data);
    return true;
}