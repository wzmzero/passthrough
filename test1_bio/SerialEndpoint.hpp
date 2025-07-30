#pragma once
#include "BaseEndpoint.hpp"
#include "Serial.hpp"

class SerialEndpoint : public BaseEndpoint {
public:
    SerialEndpoint(const std::string& port, int baud_rate)
        : port_(port), baud_rate_(baud_rate) {}
    
    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool write(const std::string& data) override;
    void start() override;
    void stop() override;
    
    Type getType() const override { return SERIAL; }
    std::string getConfigString() const override;
    
private:
    std::string port_;
    int baud_rate_;
    Serial serial_;
};