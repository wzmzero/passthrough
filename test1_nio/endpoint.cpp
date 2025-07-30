#pragma once
#include "endpoint.h"

bool Endpoint::open() {
    if (running_) return true;
    
    running_ = true;
    restartFlag_ = false;
    workThread_ = std::thread(workThread, this);
    return true;
}

void Endpoint::close() {
    if (!running_) return;
    
    running_ = false;
    if (workThread_.joinable()) {
        workThread_.join();
    }
    disconnect();
    status_ = DISCONNECTED;
    log("Endpoint closed");
}

void Endpoint::restart() {
    restartFlag_ = true;
    log("Restart requested");
}

void Endpoint::setReceiveCallback(DataCallback callback) {
    dataCallback_ = callback;
}

void Endpoint::setLogCallback(LogCallback callback) {
    logCallback_ = callback;
}

void Endpoint::log(const std::string& msg) {
    if (logCallback_) {
        logCallback_("[" + getInfo() + "] " + msg);
    }
}

void* Endpoint::workThread(void* arg) {
    Endpoint* endpoint = static_cast<Endpoint*>(arg);
    endpoint->log("Work thread started");
    
    while (endpoint->running_) {
        if (endpoint->status_ == DISCONNECTED) {
            // Attempt to open connection
            if (!endpoint->connect()) {
                endpoint->log("Connection failed, retrying...");
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
            endpoint->status_ = CONNECTED;
        }
        
        // Check for restart request
        if (endpoint->restartFlag_) {
            endpoint->log("Restarting connection...");
            endpoint->disconnect();
            endpoint->restartFlag_ = false;
            endpoint->status_ = DISCONNECTED;
            continue;
        }
        
        // Run the main processing loop
        endpoint->run();
    }
    
    endpoint->disconnect();
    endpoint->log("Work thread exiting");
    return nullptr;
}