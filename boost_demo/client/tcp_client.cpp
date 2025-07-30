#include "tcp_client.hpp"
#include <iostream>
#include <chrono>

TCPClient::TCPClient(boost::asio::io_context& io_context, 
                     const std::string& host, const std::string& port)
    : io_context_(io_context),
      resolver_(io_context),
      socket_(io_context),
      host_(host),  // 初始化host_
      port_(port) { // 初始化port_
    // 移除do_connect()调用，由connect()方法触发连接
}

void TCPClient::connect() {
    if (!connected_) {
        do_connect();
    }
}


void TCPClient::disconnect() {
    if (connected_) {
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
        connected_ = false;
    }
}

void TCPClient::run() {
    running_ = true;
    start_sender_thread();
    io_context_.run();
}

void TCPClient::send(const TimestampedMsg& msg) {
    if (!connected_) return;
    
    // 转换为网络字节序
    TimestampedMsg net_msg = msg;
    to_network_order(net_msg.counter);
    
    boost::asio::async_write(socket_,
        boost::asio::buffer(&net_msg, sizeof(TimestampedMsg)),
        [](boost::system::error_code ec, std::size_t) {
            if (ec) {
                std::cerr << "Send failed: " << ec.message() << std::endl;
            }
        });
}
void TCPClient::do_connect() {
    resolver_.async_resolve(host_, port_,
        [this](boost::system::error_code ec, 
               boost::asio::ip::tcp::resolver::results_type results) {
            if (!ec) {
                boost::asio::async_connect(socket_, results,
                    [this](boost::system::error_code ec, 
                           const boost::asio::ip::tcp::endpoint&) {
                        if (!ec) {
                            connected_ = true;
                            do_read_header();
                        } else {
                            std::cerr << "Connect failed: " << ec.message() << std::endl;
                        }
                    });
            } else {
                std::cerr << "Resolve failed: " << ec.message() << std::endl;
            }
        });
}


void TCPClient::do_read_header() {
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_buffer_.data(), sizeof(uint64_t)),
        [this](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                do_read_body();
            } else {
                disconnect();
            }
        });
}

void TCPClient::do_read_body() {
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_buffer_.data() + sizeof(uint64_t), 
                           sizeof(TimestampedMsg) - sizeof(uint64_t)),
        [this](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                // 处理接收到的消息
                TimestampedMsg received;
                std::memcpy(&received, read_buffer_.data(), sizeof(TimestampedMsg));
                to_network_order(received.counter);
                
                auto now = std::chrono::system_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    now - received.timestamp).count();
                
                std::cout << "Received #" << received.counter 
                          << " | Latency: " << latency << "us" << std::endl;
                
                do_read_header();
            } else {
                disconnect();
            }
        });
}

void TCPClient::start_sender_thread() {
    sender_thread_ = std::thread([this]() {
        uint64_t counter = 0;
        while (running_) {
            if (connected_) {
                TimestampedMsg msg;
                msg.counter = counter++;
                msg.timestamp = std::chrono::system_clock::now();
                send(msg);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}