// test_endpoint.cpp
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstring>
#include "tcp_server_endpoint.h"
#include "tcp_client_endpoint.h"
#include "udp_server_endpoint.h"
#include "udp_client_endpoint.h"
#include "serial_endpoint.h"

// 接收数据回调函数
void dataCallback(const uint8_t* data, size_t len) {
    std::cout << "Received " << len << " bytes: ";
    for (size_t i = 0; i < len && i < 20; ++i) {
        printf("%02X ", data[i]);
    }
    if (len > 20) std::cout << "...";
    std::cout << std::endl;
}

// 日志回调函数
void logCallback(const std::string& msg) {
    std::cout << "[LOG] " << msg << std::endl;
}

// 错误日志回调函数
void errorCallback(const std::string& msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
}

int main(int argc, char* argv[]) {
    int interval_ms = 0; // 发送间隔（毫秒）
    std::string send_data; // 要发送的数据
    std::mutex data_mutex; // 保护发送数据
    std::atomic<bool> running(true); // 控制线程运行
    std::atomic<bool> periodic_active(false); // 周期性发送是否激活

    // 解析 -n 参数
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            interval_ms = std::atoi(argv[i + 1]);
            // 移除 -n 和间隔参数
            for (int j = i; j + 2 < argc; ++j) {
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            break;
        }
    }

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <type> [options] [-n interval_ms]\n"
                  << "Types:\n"
                  << "  tcp_server <port>\n"
                  << "  tcp_client <ip> <port>\n"
                  << "  udp_server <port>\n"
                  << "  udp_client <ip> <port>\n"
                  << "  serial <device> <baud>\n"
                  << "Options:\n"
                  << "  -n <interval_ms> : Send data periodically every interval_ms milliseconds\n";
        return 1;
    }

    std::unique_ptr<Endpoint> endpoint;
    std::string type = argv[1];
    
    try {
        if (type == "tcp_server" && argc == 3) {
            endpoint = std::make_unique<TcpServerEndpoint>(std::stoi(argv[2]));
        } else if (type == "tcp_client" && argc == 4) {
            endpoint = std::make_unique<TcpClientEndpoint>(argv[2], std::stoi(argv[3]));
        } else if (type == "udp_server" && argc == 3) {
            endpoint = std::make_unique<UdpServerEndpoint>(std::stoi(argv[2]));
        } else if (type == "udp_client" && argc == 4) {
            endpoint = std::make_unique<UdpClientEndpoint>(argv[2], std::stoi(argv[3]));
        } else if (type == "serial" && argc == 4) {
            endpoint = std::make_unique<SerialEndpoint>(argv[2], std::stoi(argv[3]));
        } else {
            std::cerr << "Invalid arguments\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    // 设置回调
    endpoint->setDataCallback(dataCallback);
    endpoint->setLogCallback(logCallback);
    endpoint->setErrorCallback(errorCallback);
    
    // 打开端点
    if (!endpoint->open()) {
        std::cerr << "Failed to start endpoint" << std::endl;
        return 1;
    }

    // 启动周期性发送线程
    std::thread send_thread;
    if (interval_ms > 0) {
        send_thread = std::thread([&]() {
            while (running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                if (periodic_active) {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    if (!send_data.empty()) {
                        endpoint->write(reinterpret_cast<const uint8_t*>(send_data.data()), 
                                      send_data.size());
                    }
                }
            }
        });
    }

    // 用户输入处理
    std::cout << "Endpoint running. Type 'exit' to quit, 'restart' to restart connection.\n";
    if (interval_ms > 0) {
        std::cout << "Periodic sending every " << interval_ms << "ms is active\n";
        std::cout << "Type 'start' to begin periodic sending, 'stop' to pause, or enter data to send\n";
    }
    
    std::string input;
    while (true) {
        std::getline(std::cin, input);
        
        if (input == "exit") {
            break;
        } else if (input == "restart") {
            endpoint->close();
            if (endpoint->open()) {
                std::cout << "Connection restarted" << std::endl;
            } else {
                std::cerr << "Restart failed" << std::endl;
            }
        } else if (interval_ms > 0 && input == "start") {
            periodic_active = true;
            std::cout << "Periodic sending started" << std::endl;
        } else if (interval_ms > 0 && input == "stop") {
            periodic_active = false;
            std::cout << "Periodic sending stopped" << std::endl;
        } else if (!input.empty()) {
            if (interval_ms > 0) {
                // 设置要周期性发送的数据
                std::lock_guard<std::mutex> lock(data_mutex);
                send_data = input;
                if (!periodic_active) {
                    periodic_active = true;
                    std::cout << "Data set and periodic sending started" << std::endl;
                } else {
                    std::cout << "Sending data updated" << std::endl;
                }
            } else {
                // 立即发送数据（非周期性模式）
                endpoint->write(reinterpret_cast<const uint8_t*>(input.data()), 
                              input.size());
            }
        }
    }

    // 清理工作
    running = false;
    if (send_thread.joinable()) {
        send_thread.join();
    }
    
    endpoint->close();
    return 0;
}