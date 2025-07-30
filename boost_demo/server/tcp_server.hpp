#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include "common/message.hpp"

class TCPSession : public std::enable_shared_from_this<TCPSession> {
public:
    TCPSession(boost::asio::ip::tcp::socket socket);
    void start();
    void deliver(const TimestampedMsg& msg);

private:
    void do_read_header();
    void do_read_body();
    void do_write();

    boost::asio::ip::tcp::socket socket_;
    std::array<char, sizeof(TimestampedMsg)> read_buffer_;
    std::mutex write_mutex_;
    std::queue<TimestampedMsg> write_queue_;
};

class TCPServer {
public:
    TCPServer(boost::asio::io_context& io_context, short port);
    void run();
    void stop();
    void broadcast(const TimestampedMsg& msg);

private:
    void do_accept();
    void start_broadcast_thread();

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<std::shared_ptr<TCPSession>> sessions_;
    std::mutex session_mutex_;
    std::atomic<bool> running_{false};
    std::thread broadcast_thread_;
};