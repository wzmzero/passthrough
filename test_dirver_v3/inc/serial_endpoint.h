#pragma once
#include "endpoint.h"
#include <sys/epoll.h>

class SerialEndpoint : public Endpoint {
public:
    SerialEndpoint(const std::string& device, int baudrate);
    ~SerialEndpoint() override;
    
    bool open() override;
    void close() override;
    void write(const uint8_t* data, size_t len) override;

private:
    void run() override;
    bool configureSerialPort();
    void handleSerialData();

    const std::string _device;
    const int _baudrate;
    int _serialFd = -1;
    int _epollFd = -1;
};