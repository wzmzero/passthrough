// SerialEndpoint.cpp
#include "SerialEndpoint.hpp"

bool SerialEndpoint::open() {
    if (serial_.isOpen()) return true;
    return serial_.open(port_, baud_rate_);
}

void SerialEndpoint::close() {
    serial_.close();
}

bool SerialEndpoint::isOpen() const {
    return serial_.isOpen();
}

bool SerialEndpoint::write(const std::string& data) {
    return serial_.write(data);
}

void SerialEndpoint::start() {
    if (running_) return;
    
    serial_.setDataCallback([this](const std::string& data) {
        callDataCallback(data);
    });
    
    serial_.startReading();
    running_ = true;
}

void SerialEndpoint::stop() {
    if (!running_) return;
    
    serial_.stopReading();
    running_ = false;
}

std::string SerialEndpoint::getConfigString() const {
    return "Serial: " + port_ + " @ " + std::to_string(baud_rate_) + " baud";
}