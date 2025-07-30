#include "endpoint.h"
#include <iostream>

Endpoint::Endpoint() = default;

Endpoint::~Endpoint() {
    stopThread();
}

void Endpoint::setDataCallback(DataCallback cb) {
    _dataCallback = std::move(cb);
}

void Endpoint::setLogCallback(LogCallback cb) {
    _logCallback = std::move(cb);
}

void Endpoint::setErrorCallback(ErrorCallback cb) {
    _errorCallback = std::move(cb);
}

bool Endpoint::isRunning() const {
    return _running;
}

bool Endpoint::isConnected() const {
    return _state == State::CONNECTED;
}

void Endpoint::startThread() {
    if (_running) return;
    
    _running = true;
    _worker = std::thread([this] {
        run();
    });
}

void Endpoint::stopThread() {
    if (!_running) return;
    
    _running = false;
    notifyThread();
    
    if (_worker.joinable()) {
        _worker.join();
    }
}

void Endpoint::notifyThread() {
    _cv.notify_one();
}

void Endpoint::setState(State newState) {
    _state = newState;
}

Endpoint::State Endpoint::getState() const {
    return _state;
}

void Endpoint::logMessage(const std::string& msg) {
    if (_logCallback) {
        _logCallback(msg);
    } else {
        std::cout << "[LOG] " << msg << std::endl;
    }
}

void Endpoint::logError(const std::string& error) {
    if (_errorCallback) {
        _errorCallback(error);
    } else {
        std::cerr << "[ERROR] " << error << std::endl;
    }
    setState(State::ERROR);
}

void Endpoint::processData(const uint8_t* data, size_t len) {
    if (_dataCallback) {
        _dataCallback(data, len);
    }
}