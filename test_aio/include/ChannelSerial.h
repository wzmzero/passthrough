#pragma once

#include "ChannelBase.h"
#include <boost/asio/serial_port.hpp>

class ChannelSerial : public ChannelBase {
public:
    ChannelSerial(boost::asio::io_context& io_context, 
                 const std::string& port, 
                 unsigned int baud_rate);
    ~ChannelSerial();
    
    bool start() override;
    void stop() override;
    bool send(const std::vector<uint8_t>& data) override;

private:
    void run_receive() override;
    void start_read();
    void handle_read(const boost::system::error_code& ec, size_t bytes_transferred);

    boost::asio::serial_port serial_port_;
    std::array<uint8_t, 128> read_buffer_;
};
