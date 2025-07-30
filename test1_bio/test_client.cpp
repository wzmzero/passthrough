#include "NetworkClient.hpp"
#include "ConfigReader.hpp"
#include "Serial.hpp"
#include <iostream>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <cstring>
int main(int argc, char *argv[])
{
// 解析命令行参数
    bool show_help = false;
    bool update_database = false;
    std::string server_ip;
    int server_port = 0;
    std::string serial_port;
    int baud_rate = 0;
    
    // 检查是否显示帮助
    if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        show_help = true;
    }
    
    if (show_help || argc < 2) {
        std::cout << "Usage: " << argv[0] << " <tcp|udp|serial> [options]\n"
                  << "Options:\n"
                  << "  --server_ip <ip>       Set server IP address (for tcp/udp)\n"
                  << "  --server_port <port>   Set server port number (for tcp/udp)\n"
                  << "  --serial_port <port>   Set serial port device (for serial)\n"
                  << "  --baud_rate <rate>     Set baud rate (for serial)\n"
                  << "  -h, --help            Show this help message\n";
        return show_help ? 0 : 1;
    }

    std::string protocol_arg = argv[1];
    ConfigReader::Protocol protocol;

    if (protocol_arg == "tcp")
    {
        protocol = ConfigReader::TCP;
    }
    else if (protocol_arg == "udp")
    {
        protocol = ConfigReader::UDP;
    }
    else if (protocol_arg == "serial")
    {
        protocol = ConfigReader::SERIAL;
    }
    else
    {
        std::cerr << "Unsupported protocol type: " << protocol_arg << std::endl;
        return 1;
    }
// 解析其他命令行参数
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--server_ip") == 0 && i + 1 < argc) {
            server_ip = argv[++i];
            update_database = true;
        } else if (strcmp(argv[i], "--server_port") == 0 && i + 1 < argc) {
            server_port = std::atoi(argv[++i]);
            update_database = true;
        } else if (strcmp(argv[i], "--serial_port") == 0 && i + 1 < argc) {
            serial_port = argv[++i];
            update_database = true;
        } else if (strcmp(argv[i], "--baud_rate") == 0 && i + 1 < argc) {
            baud_rate = std::atoi(argv[++i]);
            update_database = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            std::cout << "Usage: " << argv[0] << " <tcp|udp|serial> [options]\n"
                      << "Options:\n"
                      << "  --server_ip <ip>       Set server IP address (for tcp/udp)\n"
                      << "  --server_port <port>   Set server port number (for tcp/udp)\n"
                      << "  --serial_port <port>   Set serial port device (for serial)\n"
                      << "  --baud_rate <rate>     Set baud rate (for serial)\n"
                      << "  -h, --help            Show this help message\n";
            return 0;
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            std::cerr << "Use -h for help" << std::endl;
            return 1;
        }
    }
    ConfigReader configReader("config.db");

    ConfigReader::ClientConfig config;
    bool config_ok = configReader.readClientConfig(protocol, config);
    
    // 应用命令行参数
    if (protocol == ConfigReader::TCP || protocol == ConfigReader::UDP) {
        if (!server_ip.empty()) {
            config.server_ip = server_ip;
        }
        if (server_port > 0) {
            config.server_port = server_port;
        }
    } else if (protocol == ConfigReader::SERIAL) {
        if (!serial_port.empty()) {
            config.serial_port = serial_port;
        }
        if (baud_rate > 0) {
            config.baud_rate = baud_rate;
        }
    }
    
    // 如果配置缺失，使用默认值
    if (!config_ok) {
        std::cerr << "Using fallback configuration for " << protocol_arg << std::endl;
        if (config.server_ip.empty()) config.server_ip = "127.0.0.1";
        if (config.server_port == 0) config.server_port = (protocol == ConfigReader::TCP) ? 8080 : 9090;
        if (config.serial_port.empty()) config.serial_port = "/dev/ttyUSB0";
        if (config.baud_rate == 0) config.baud_rate = 115200;
    }
    
    // 更新数据库配置
    if (update_database) {
        if (configReader.writeClientConfig(protocol, config)) {
            std::cout << "Updated configuration in database for " << protocol_arg << ":\n";
            if (protocol == ConfigReader::TCP || protocol == ConfigReader::UDP) {
                std::cout << "  Server IP: " << config.server_ip << "\n";
                std::cout << "  Server Port: " << config.server_port << "\n";
            } else {
                std::cout << "  Serial Port: " << config.serial_port << "\n";
                std::cout << "  Baud Rate: " << config.baud_rate << "\n";
            }
        } else {
            std::cerr << "Warning: Failed to update configuration in database" << std::endl;
        }
    }
    
    // 显示最终配置
    std::cout << "Using configuration for " << protocol_arg << ":\n";
    if (protocol == ConfigReader::TCP || protocol == ConfigReader::UDP) {
        std::cout << "  Server IP: " << config.server_ip << "\n";
        std::cout << "  Server Port: " << config.server_port << "\n";
    } else {
        std::cout << "  Serial Port: " << config.serial_port << "\n";
        std::cout << "  Baud Rate: " << config.baud_rate << "\n";
    }

    // 串口透传模式
    if (protocol == ConfigReader::SERIAL)
    {

        Serial serial;
        if (!serial.open(config.serial_port, config.baud_rate))
        {
            return 1;
        }

        std::atomic<bool> running(true);

        // 串口接收数据的回调
        serial.setDataCallback([&](const std::string &data)
                               { std::cout << "Received Serial Data: " << data << std::endl; });

        // 启动串口读取线程
        serial.startReading();
        std::cout << "Serial to network bridge active.\nType 'exit' to quit.\n";

        // 启动发送线程
        std::thread send_thread([&]
                                {
        std::string input;
        while (running) {
            std::cout << "Enter message: ";
            if (!std::getline(std::cin, input)) {
                break;  // 输入流出错或EOF
            }
            if (input == "exit") {
                running = false;
                break;
            }
            if (!input.empty()) {
                if (!serial.write(input)) {
                    std::cerr << "Failed to write data to serial" << std::endl;
                }
            }
        } });

        // 主线程等待退出信号
        while (running)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 清理资源
        serial.stopReading();
        serial.close();
        if (send_thread.joinable())
            send_thread.join();

        return 0;
    }

    // 网络客户端模式
    NetworkClient::Protocol net_protocol;
    if (protocol == ConfigReader::TCP)
    {
        net_protocol = NetworkClient::TCP;
    }
    else
    {
        net_protocol = NetworkClient::UDP;
    }

    NetworkClient client(net_protocol, config.server_ip, config.server_port);

    std::atomic<bool> running(true);
    std::cout << "Type 'exit' to quit.\n";

    // 接收线程：不断监听服务器发来的消息
    std::thread recv_thread([&]
                            {
    while (running) {
        std::string reply = client.receiveMessage();
        if (!reply.empty()) {
            std::cout << "\nServer response ("
                      << ((client.getProtocol() == NetworkClient::TCP) ? "TCP" : "UDP")
                      << "): " << reply << "\n";
        } else {
            // TCP断开连接时可退出循环
            if (client.getProtocol() == NetworkClient::TCP) {
                std::cerr << "Connection closed by server.\n";
                running = false;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } });

    // 发送线程：读取用户输入，发送消息
    std::thread send_thread([&]
                            {
    std::string input;
    while (running) {
        std::cout << "Enter message: ";
        if (!std::getline(std::cin, input)) {
            running = false;
            break;
        }

        if (input == "exit") {
            running = false;
            break;
        }

        if (!input.empty()) {
            if (!client.sendMessage(input)) {
                std::cerr << "Failed to send message.\n";
            }
        }
    } });

    // 等待线程结束
    if (send_thread.joinable())
        send_thread.join();
    if (recv_thread.joinable())
        recv_thread.join();

    // 关闭连接
    client.closeConnection();
    return 0;
}