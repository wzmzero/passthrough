#include "ChannelTcpClient.h"
#include <iostream>

using namespace boost::asio;
using namespace boost::system;

ChannelTcpClient::ChannelTcpClient(boost::asio::io_context& io_context, 
                                   const std::string& ip, 
                                   uint16_t port)
    : ChannelBase(io_context),
      ip_(ip),
      port_(port),
      socket_(io_context),
      resolver_(io_context) {}

ChannelTcpClient::~ChannelTcpClient() {
    stop();
}

bool ChannelTcpClient::start() {
    if (running_) return true;
    running_ = true;
    receive_thread_ = std::thread(&ChannelTcpClient::run_receive, this);
    return true;
}

void ChannelTcpClient::stop() {
    running_ = false;
    error_code ec;
    socket_.close(ec);
    ChannelBase::stop();
}

bool ChannelTcpClient::send(const std::vector<uint8_t>& data) {
    if (!connected_) return false;
    error_code ec;
    size_t len = boost::asio::write(socket_, boost::asio::buffer(data), ec);
    return !ec && len == data.size();
}

void ChannelTcpClient::do_connect() {
    resolver_.async_resolve(ip_, std::to_string(port_),
        [this](const error_code& ec, ip::tcp::resolver::results_type endpoints) {
            if (!ec) {
                boost::asio::async_connect(socket_, endpoints,
                    [this](const error_code& ec, const ip::tcp::endpoint&) {
                        handle_connect(ec);
                    });
            } else {
                std::cerr << "Resolve error: " << ec.message() << std::endl;
                // 尝试重新连接
                if (running_) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    do_connect();
                }
            }
        });
}

void ChannelTcpClient::handle_connect(const error_code& ec) {
    if (!ec) {
        connected_ = true;
        std::cout << "Connected to server" << std::endl;
        // 开始读取数据
        socket_.async_read_some(
            boost::asio::buffer(read_buffer_),
            [this](const error_code& ec, size_t bytes_transferred) {
                handle_read(ec, bytes_transferred);
            });
    } else {
        std::cerr << "Connect error: " << ec.message() << std::endl;
        // 尝试重新连接
        if (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            do_connect();
        }
    }
}

void ChannelTcpClient::handle_read(const error_code& ec, size_t bytes_transferred) {
    if (!ec && bytes_transferred > 0) {
        for (size_t i = 0; i < bytes_transferred; ++i) {
            receive_queue_.push(read_buffer_[i]);
        }
        // 继续读取
        socket_.async_read_some(
            boost::asio::buffer(read_buffer_),
            [this](const error_code& ec, size_t bytes_transferred) {
                handle_read(ec, bytes_transferred);
            });
    } else if (ec != error::operation_aborted) {
        std::cerr << "Read error: " << ec.message() << std::endl;
        connected_ = false;
        // 尝试重新连接
        if (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            do_connect();
        }
    }
}

void ChannelTcpClient::run_receive() {
    do_connect();
    while (running_) {
        io_context_.run();
    }
}