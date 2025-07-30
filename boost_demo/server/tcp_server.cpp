#include "tcp_server.hpp"
#include <iostream>

// TCPSession
TCPSession::TCPSession(boost::asio::ip::tcp::socket socket)
    : socket_(std::move(socket)) {}

void TCPSession::start() {
    do_read_header();
}

void TCPSession::do_read_header() {
    auto self(shared_from_this());
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_buffer_.data(), sizeof(uint64_t)),
        [this, self](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                do_read_body();
            } else {
                // Handle disconnect
            }
        });
}

void TCPSession::do_read_body() {
    auto self(shared_from_this());
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_buffer_.data() + sizeof(uint64_t), 
                           sizeof(TimestampedMsg) - sizeof(uint64_t)),
        [this, self](boost::system::error_code ec, std::size_t) {
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
            }
        });
}

void TCPSession::deliver(const TimestampedMsg& msg) {
    bool write_in_progress = false;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_in_progress = !write_queue_.empty();
        write_queue_.push(msg);
    }
    
    if (!write_in_progress) {
        do_write();
    }
}

void TCPSession::do_write() {
    auto self(shared_from_this());
    TimestampedMsg msg;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        msg = write_queue_.front();
    }

    // 转换为网络字节序
    to_network_order(msg.counter);
    
    boost::asio::async_write(socket_,
        boost::asio::buffer(&msg, sizeof(TimestampedMsg)),
        [this, self](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                std::lock_guard<std::mutex> lock(write_mutex_);
                write_queue_.pop();
                if (!write_queue_.empty()) {
                    do_write();
                }
            }
        });
}

// TCPServer
TCPServer::TCPServer(boost::asio::io_context& io_context, short port)
    : io_context_(io_context),
      acceptor_(io_context, 
                boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
    do_accept();
}

void TCPServer::run() {
    running_ = true;
    start_broadcast_thread();
    io_context_.run();
}

void TCPServer::stop() {
    running_ = false;
    io_context_.stop();
    if (broadcast_thread_.joinable()) {
        broadcast_thread_.join();
    }
}

void TCPServer::broadcast(const TimestampedMsg& msg) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    for (auto& session : sessions_) {
        session->deliver(msg);
    }
}

void TCPServer::do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                std::lock_guard<std::mutex> lock(session_mutex_);
                sessions_.push_back(
                    std::make_shared<TCPSession>(std::move(socket)));
                sessions_.back()->start();
            }
            do_accept();
        });
}

void TCPServer::start_broadcast_thread() {
    broadcast_thread_ = std::thread([this]() {
        uint64_t counter = 0;
        while (running_) {
            TimestampedMsg msg;
            msg.counter = counter++;
            msg.timestamp = std::chrono::system_clock::now();
            
            broadcast(msg);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}