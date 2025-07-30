#include "ChannelSerial.h"
#include <iostream>

using namespace boost::asio;
using namespace boost::system;

ChannelSerial::ChannelSerial(boost::asio::io_context& io_context, 
                             const std::string& port, 
                             unsigned int baud_rate)
    : ChannelBase(io_context), serial_port_(io_context, port) {
    serial_port_.set_option(serial_port::baud_rate(baud_rate));
    serial_port_.set_option(serial_port::flow_control(serial_port::flow_control::none));
    serial_port_.set_option(serial_port::parity(serial_port::parity::none));
    serial_port_.set_option(serial_port::stop_bits(serial_port::stop_bits::one));
    serial_port_.set_option(serial_port::character_size(8));
}

ChannelSerial::~ChannelSerial() {
    stop();
}

bool ChannelSerial::start() {
    if (running_) return true;
    running_ = true;
    receive_thread_ = std::thread(&ChannelSerial::run_receive, this);
    return true;
}

void ChannelSerial::stop() {
    running_ = false;
    error_code ec;
    serial_port_.cancel(ec);
    serial_port_.close(ec);
    ChannelBase::stop();
}

bool ChannelSerial::send(const std::vector<uint8_t>& data) {
    error_code ec;
    size_t len = serial_port_.write_some(buffer(data), ec);
    return !ec && len == data.size();
}

void ChannelSerial::start_read() {
    serial_port_.async_read_some(
        buffer(read_buffer_),
        [this](const error_code& ec, size_t bytes_transferred) {
            handle_read(ec, bytes_transferred);
        });
}

void ChannelSerial::handle_read(const error_code& ec, size_t bytes_transferred) {
    if (!ec && bytes_transferred > 0) {
        for (size_t i = 0; i < bytes_transferred; ++i) {
            receive_queue_.push(read_buffer_[i]);
        }
        start_read();
    } else if (ec != error::operation_aborted) {
        std::cerr << "Serial read error: " << ec.message() << std::endl;
    }
}

void ChannelSerial::run_receive() {
    start_read();
    while (running_) {
        io_context_.run();
    }
}