#include "tcp_server.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <csignal>

std::function<void(int)> shutdown_handler;
void signal_handler(int signal) { shutdown_handler(signal); }

int main() {
    boost::asio::io_context io_context;
    TCPServer server(io_context, 12345);
    
    // 设置信号处理
    shutdown_handler = [&](int signal) {
        std::cout << "\nShutting down server..." << std::endl;
        server.stop();
    };
    std::signal(SIGINT, signal_handler);
    
    std::cout << "Server starting..." << std::endl;
    server.run();
    std::cout << "Server stopped" << std::endl;
    return 0;
}