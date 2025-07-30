 
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <atomic>
#include "ChannelBase.hpp"
#include "ChannelTcpServer.hpp"
#include "ChannelTcpClient.hpp"
#include "ChannelUdpServer.hpp"
#include "ChannelUdpClient.hpp"
#include "ChannelSerial.hpp"

namespace po = boost::program_options;

int main(int argc, char* argv[]) {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "Show help")
        ("protocol", po::value<std::string>()->required(), "Protocol (tcp, udp, serial)")
        ("type", po::value<std::string>(), "Type (server, client)")
        ("host", po::value<std::string>(), "Host/IP")
        ("port", po::value<uint16_t>(), "Port")
        ("device", po::value<std::string>(), "Serial device")
        ("baud", po::value<unsigned int>(), "Baud rate");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n" << desc << "\n";
        return 1;
    }

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    boost::asio::io_context io_context;
    std::unique_ptr<ChannelBase> channel;
    std::atomic<bool> running{true};

    try {
        const std::string protocol = vm["protocol"].as<std::string>();
        
        if (protocol == "tcp") {
            const std::string type = vm["type"].as<std::string>();
            const std::string host = vm["host"].as<std::string>();
            const uint16_t port = vm["port"].as<uint16_t>();
            
            if (type == "server") {
                channel = std::make_unique<ChannelTcpServer>(io_context, host, port);
            } else if (type == "client") {
                channel = std::make_unique<ChannelTcpClient>(io_context, host, port);
            } else {
                throw std::runtime_error("Invalid type for TCP");
            }
        } 
        else if (protocol == "udp") {
            const std::string type = vm["type"].as<std::string>();
            
            if (type == "server") {
                const uint16_t port = vm["port"].as<uint16_t>();
                channel = std::make_unique<ChannelUdpServer>(io_context, port);
            } else if (type == "client") {
                const std::string host = vm["host"].as<std::string>();
                const uint16_t port = vm["port"].as<uint16_t>();
                channel = std::make_unique<ChannelUdpClient>(io_context, host, port);
            } else {
                throw std::runtime_error("Invalid type for UDP");
            }
        } 
        else if (protocol == "serial") {
            const std::string device = vm["device"].as<std::string>();
            const unsigned int baud = vm["baud"].as<unsigned int>();
            channel = std::make_unique<ChannelSerial>(io_context, device, baud);
        } 
        else {
            throw std::runtime_error("Unsupported protocol");
        }
    } catch (const std::exception& e) {
        std::cerr << "Configuration error: " << e.what() << "\n";
        return 1;
    }

    // 设置接收回调
    channel->setReceiveCallback([](const std::string& data) {
        std::cout << "Received: " << data << "\n";
    });

    // 启动通道
    channel->start();

    // 创建输入线程处理用户输入
    std::thread input_thread([&]() {
        std::string input;
        while (running) {
            std::getline(std::cin, input);
            if (input == "exit") {
                running = false;
                io_context.stop();
                break;
            }
            // 在主线程中发送数据
            boost::asio::post(io_context, [&channel, input]() {
                channel->send(input + "\n");
            });
        }
    });

    // 运行IO上下文
    io_context.run();

    // 等待输入线程结束
    if (input_thread.joinable()) {
        input_thread.join();
    }

    return 0;
}
 