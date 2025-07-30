#include "ChannelTcpServer.h"
#include <iostream>

using namespace boost::asio;
using namespace boost::system;

ChannelTcpServer::ChannelTcpServer(boost::asio::io_context& io_context, uint16_t port)
    : ChannelBase(io_context),
      acceptor_(io_context, ip::tcp::endpoint(ip::tcp::v4(), port)) {}

ChannelTcpServer::~ChannelTcpServer() {
    stop();
}

bool ChannelTcpServer::start() {
    if (running_) return true;
    running_ = true;
    do_accept();
    receive_thread_ = std::thread(&ChannelTcpServer::run_receive, this);
    return true;
}

void ChannelTcpServer::stop() {
    running_ = false;
    error_code ec;
    acceptor_.close(ec);
    if (socket_ && socket_->is_open()) {
        socket_->shutdown(ip::tcp::socket::shutdown_both, ec);
        socket_->close(ec);
    }
    ChannelBase::stop();
}

bool ChannelTcpServer::send(const std::vector<uint8_t>& data) {
    if (!connected_) return false;
    error_code ec;
    size_t len = socket_->write_some(buffer(data), ec);
    return !ec && len == data.size();
}

void ChannelTcpServer::do_accept() {
    socket_ = std::make_shared<ip::tcp::socket>(io_context_);
    acceptor_.async_accept(*socket_, [this](const error_code& ec) {
        if (!ec) {
            connected_ = true;
            std::cout << "TCP client connected." << std::endl;
        } else {
            std::cerr << "TCP accept error: " << ec.message() << std::endl;
        }
    });
}

void ChannelTcpServer::handle_connection(std::shared_ptr<ip::tcp::socket> socket) {
    try {
        while (running_ && socket->is_open()) {
            uint8_t byte;
            error_code ec;
            size_t len = socket->read_some(buffer(&byte, 1), ec);
            
            if (ec == error::eof) {
                std::cout << "TCP client disconnected." << std::endl;
                break;
            } else if (ec) {
                throw system_error(ec);
            }
            
            if (len == 1) {
                receive_queue_.push(byte);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "TCP connection error: " << e.what() << std::endl;
    }
    
    connected_ = false;
    error_code ec;
    socket->shutdown(ip::tcp::socket::shutdown_both, ec);
    socket->close(ec);
    do_accept(); // 继续接受新连接
}

void ChannelTcpServer::run_receive() {
    while (running_) {
        if (connected_) {
            handle_connection(socket_);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}