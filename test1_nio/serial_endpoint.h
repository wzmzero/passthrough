#pragma once
#include "endpoint.h"
#include <termios.h>
#include <sys/select.h>

class SerialEndpoint : public Endpoint {
public:
    SerialEndpoint(const std::string& port, uint32_t baudRate);
    virtual ~SerialEndpoint();
    
    void send(const uint8_t* data, size_t len) override;
    std::string getInfo() const override;

protected:
    bool connect() override;
    void disconnect() override;
    void run() override;
    speed_t getBaudRate(uint32_t baud);

private:
    std::string port_;
    uint32_t baudRate_;
    int fd_ = -1;
    fd_set readFds_;
};