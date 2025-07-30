#pragma once
#include <boost/asio.hpp>
#include <thread>
#include <atomic>
#include <string> // 添加string头文件
#include "common/message.hpp"

class TCPClient {
public:
    TCPClient(boost::asio::io_context& io_context, 
              const std::string& host, const std::string& port);
    void connect();
    void disconnect();
    void run();
    void send(const TimestampedMsg& msg);

private:
    void do_connect();
    void do_read_header();
    void do_read_body();
    void start_sender_thread();

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::socket socket_;
    std::array<char, sizeof(TimestampedMsg)> read_buffer_;
    std::atomic<bool> connected_{false};
    std::thread sender_thread_;
    std::atomic<bool> running_{false};
    
    // 添加host和port成员变量
    std::string host_;
    std::string port_;
};