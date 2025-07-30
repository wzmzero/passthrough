#pragma once
#include "ChannelBase.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <string>

class ChannelSerial : public ChannelBase {
public:
    ChannelSerial(const std::string& port, int baudrate);
    virtual ~ChannelSerial();
    
    bool start() override;
    void stop() override;
    bool send(const std::vector<uint8_t>& data) override;

private:
    void serialThreadFunc();
    bool configureSerialPort();
    
    std::string port;
    int baudrate;
    int serialFd = -1;
    std::atomic<bool> running{false};
    std::thread workerThread;
    std::mutex writeMutex;
};