#include "tcp_client.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <csignal>

std::function<void(int)> shutdown_handler;
void signal_handler(int signal) { shutdown_handler(signal); }

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: client <host> <port>" << std::endl;
        return 1;
    }

    boost::asio::io_context io_context;
    TCPClient client(io_context, argv[1], argv[2]);
    
    // 设置信号处理
    shutdown_handler = [&](int signal) {
        std::cout << "\nShutting down client..." << std::endl;
        client.disconnect();
        exit(signal);
    };
    std::signal(SIGINT, signal_handler);
    
    std::cout << "Client starting..." << std::endl;
    client.connect(); // 添加显式连接调用
    client.run();
    return 0;
}