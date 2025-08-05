#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstring>
#include <sstream>
#include <iomanip>
#include "tcp_server_endpoint.h"
#include "tcp_client_endpoint.h"
#include "common.h" // 包含四遥数据库
#include "driver_104_m.h" // IEC 104 主站驱动
#include "driver_104_s.h" // IEC 104 从站驱动

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

// 打印IEC 104数据点
void printIEC104DataPoints(const std::vector<IEC104DataPoint>& points) {
    for (const auto& point : points) {
        std::cout << "  Type: " << static_cast<int>(point.type)
                  << ", Address: " << point.address
                  << ", Value: " << point.value
                  << ", Quality: " << static_cast<int>(point.quality) << "\n";
    }
}

int main(int argc, char* argv[]) {
    int interval_ms = 0; // 发送间隔（毫秒）
    std::string send_data; // 要发送的数据
    std::mutex data_mutex; // 保护发送数据
    std::atomic<bool> running(true); // 控制线程运行
    std::atomic<bool> periodic_active(false); // 周期性发送是否激活
    bool is_104_master = false;
    bool is_104_slave = false;
    std::unique_ptr<Driver104M> iec104_master;
    std::unique_ptr<Driver104S> iec104_slave;
    std::unique_ptr<Endpoint> endpoint;

    // 创建四遥数据库
    auto database = std::make_shared<SimpleMemoryDatabase>();
    
    // 初始化一些测试数据
    database->updateYCValue(40001, 25.5);  // 温度
    database->updateYCValue(40002, 101.3); // 压力
    database->updateYXValue(10001, true);  // 开关状态

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

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <mode> <transport> [options] [-n interval_ms]\n"
                  << "Modes:\n"
                  << "  104_master : IEC 104 Master (Client)\n"
                  << "  104_slave  : IEC 104 Slave (Server)\n"
                  << "Transports:\n"
                  << "  tcp        : TCP transport (required for 104)\n"
                  << "Options:\n"
                  << "  For master: <server_ip> <server_port>\n"
                  << "  For slave: <listen_port>\n"
                  << "Example:\n"
                  << "  " << argv[0] << " 104_master tcp 192.168.1.100 2404\n"
                  << "  " << argv[0] << " 104_slave tcp 2404\n";
        return 1;
    }

    std::string mode = argv[1];
    std::string transport = argv[2];
    is_104_master = (mode == "104_master");
    is_104_slave = (mode == "104_slave");
    
    if (!is_104_master && !is_104_slave) {
        std::cerr << "Invalid mode. Must be '104_master' or '104_slave'.\n";
        return 1;
    }
    
    if (transport != "tcp") {
        std::cerr << "IEC 104 requires TCP transport.\n";
        return 1;
    }

    // 创建TCP端点
    try {
        if (is_104_master) {
            if (argc != 5) {
                std::cerr << "Invalid arguments for IEC 104 master.\n"
                          << "Usage: " << argv[0] << " 104_master tcp <server_ip> <server_port>\n";
                return 1;
            }
            
            std::string server_ip = argv[3];
            int server_port = std::stoi(argv[4]);
            endpoint = std::make_unique<TcpClientEndpoint>(server_ip, server_port);
        } 
        else if (is_104_slave) {
            if (argc != 4) {
                std::cerr << "Invalid arguments for IEC 104 slave.\n"
                          << "Usage: " << argv[0] << " 104_slave tcp <listen_port>\n";
                return 1;
            }
            
            int listen_port = std::stoi(argv[3]);
            endpoint = std::make_unique<TcpServerEndpoint>(listen_port);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    // 创建IEC 104驱动实例
    if (is_104_master) {
        iec104_master = std::make_unique<Driver104M>();
    } else if (is_104_slave) {
        iec104_slave = std::make_unique<Driver104S>(database.get());
    }

    // 设置回调
    if (endpoint) {
        endpoint->setDataCallback(dataCallback);
        endpoint->setLogCallback(logCallback);
        endpoint->setErrorCallback(errorCallback);
    }
    
    // 设置IEC 104从站的数据回调
    if (is_104_slave && iec104_slave && endpoint) {
        endpoint->setDataCallback([&](const uint8_t* data, size_t len) {
            dataCallback(data, len); // 调用原始回调
            
            // 解析104请求
            IEC104ASDU asdu;
            if (iec104_slave->parseFrame(data, len, asdu)) {
                std::cout << "\n=== IEC 104 Slave Request ===" << std::endl;
                std::cout << "Type: " << static_cast<int>(asdu.type) 
                          << ", Cause: " << static_cast<int>(asdu.cause) << std::endl;
                printIEC104DataPoints(asdu.dataPoints);
                
                // 处理请求并生成响应
                IEC104ASDU response = iec104_slave->processRequest(asdu);
                
                // 生成响应帧
                auto frame = iec104_slave->createFrame(response);
                
                std::cout << "\n=== IEC 104 Slave Response ===" << std::endl;
                std::cout << "Type: " << static_cast<int>(response.type) 
                          << ", Cause: " << static_cast<int>(response.cause) << std::endl;
                printIEC104DataPoints(response.dataPoints);
                
                // 发送响应
                endpoint->write(frame.data(), frame.size());
            }
        });
    }
    
    // 设置IEC 104主站的数据回调
    if (is_104_master && iec104_master && endpoint) {
        endpoint->setDataCallback([&](const uint8_t* data, size_t len) {
            dataCallback(data, len); // 调用原始回调
            
            // 解析104响应
            IEC104ASDU asdu;
            if (iec104_master->parseFrame(data, len, asdu)) {
                std::cout << "\n=== IEC 104 Master Response ===" << std::endl;
                std::cout << "Type: " << static_cast<int>(asdu.type) 
                          << ", Cause: " << static_cast<int>(asdu.cause) << std::endl;
                printIEC104DataPoints(asdu.dataPoints);
            }
        });
    }
    
    // 打开端点
    if (endpoint && !endpoint->open()) {
        std::cerr << "Failed to start endpoint" << std::endl;
        return 1;
    }

    // 启动周期性发送线程（仅主站）
    std::thread send_thread;
    if (interval_ms > 0 && is_104_master) {
        send_thread = std::thread([&]() {
            while (running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                if (periodic_active) {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    if (!send_data.empty() && endpoint) {
                        endpoint->write(reinterpret_cast<const uint8_t*>(send_data.data()), 
                                    send_data.size());
                    }
                }
            }
        });
    }

    // 用户输入处理
    std::cout << "\n========================================" << std::endl;
    if (is_104_master) {
        std::cout << "IEC 104 Master Mode (TCP)" << std::endl;
        std::cout << "Server: " << argv[3] << ":" << argv[4] << std::endl;
        std::cout << "Available commands:\n"
                  << "  gi                         - Send general interrogation\n"
                  << "  read_single <addr>         - Read single point (YX)\n"
                  << "  read_analog <addr>         - Read analog value (YC)\n"
                  << "  send_clock_sync            - Send clock synchronization\n"
                  << "  send_test_frame            - Send test frame\n"
                  << "  exit                      - Quit program\n";
    } else if (is_104_slave) {
        std::cout << "IEC 104 Slave Mode (TCP)" << std::endl;
        std::cout << "Listening on port: " << argv[3] << std::endl;
        std::cout << "Memory manipulation commands:\n"
                  << "  set_yx <addr> <0|1>        - Set YX (digital) value\n"
                  << "  get_yx <addr>              - Get YX value\n"
                  << "  set_yc <addr> <value>      - Set YC (analog) value\n"
                  << "  get_yc <addr>             - Get YC value\n"
                  << "  exit                      - Quit program\n";
    }
    std::cout << "========================================" << std::endl;
    
    if (interval_ms > 0 && is_104_master) {
        std::cout << "\nPeriodic sending every " << interval_ms << "ms is available\n";
        std::cout << "Use 'start' to begin periodic sending, 'stop' to pause\n";
    }
    
    std::string input;
    while (true) {
        if (is_104_slave) {
            // 从站模式需要定期检查连接状态
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        std::cout << "\n> ";
        std::getline(std::cin, input);
        
        if (input == "exit") {
            break;
        } 
        else if (interval_ms > 0 && input == "start") {
            periodic_active = true;
            std::cout << "Periodic sending started" << std::endl;
        } 
        else if (interval_ms > 0 && input == "stop") {
            periodic_active = false;
            std::cout << "Periodic sending stopped" << std::endl;
        } 
        else if (is_104_master && !input.empty()) {
            // 处理IEC 104主站命令
            std::istringstream iss(input);
            std::string cmd;
            iss >> cmd;
            
            if (cmd == "gi") {
                auto frame = iec104_master->createGeneralInterrogation();
                endpoint->write(frame.data(), frame.size());
                std::cout << "Sent general interrogation" << std::endl;
            } 
            else if (cmd == "read_single") {
                uint32_t addr;
                if (iss >> addr) {
                    auto frame = iec104_master->createReadCommand(IEC104Type::M_SP_NA_1, addr);
                    endpoint->write(frame.data(), frame.size());
                    std::cout << "Sent read single point command for address: " << addr << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: read_single <address>" << std::endl;
                }
            } 
            else if (cmd == "read_analog") {
                uint32_t addr;
                if (iss >> addr) {
                    auto frame = iec104_master->createReadCommand(IEC104Type::M_ME_NA_1, addr);
                    endpoint->write(frame.data(), frame.size());
                    std::cout << "Sent read analog value command for address: " << addr << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: read_analog <address>" << std::endl;
                }
            } 
            else if (cmd == "send_clock_sync") {
                auto now = std::chrono::system_clock::now();
                auto frame = iec104_master->createClockSyncCommand(now);
                endpoint->write(frame.data(), frame.size());
                std::cout << "Sent clock synchronization command" << std::endl;
            } 
            else if (cmd == "send_test_frame") {
                auto frame = iec104_master->createTestFrame();
                endpoint->write(frame.data(), frame.size());
                std::cout << "Sent test frame" << std::endl;
            } 
            else {
                std::cerr << "Unknown command: " << cmd << std::endl;
            }
        } else if (is_104_slave && !input.empty()) {
            // 处理IEC 104从站命令
            std::istringstream iss(input);
            std::string cmd;
            iss >> cmd;
            
            if (cmd == "set_yx") {
                uint32_t address;
                uint16_t value;
                if (iss >> address >> value) {
                    database->updateYXValue(address, value != 0);
                    std::cout << "Set YX " << address << " to " 
                              << (value ? "ON" : "OFF") << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: set_yx <address> <0|1>" << std::endl;
                }
            } else if (cmd == "get_yx") {
                uint32_t address;
                if (iss >> address) {
                    TelemetryPoint tp;
                    if (database->readYX(address, tp)) {
                        bool value = (tp.value > 0.5);
                        std::cout << "YX " << address << " = " 
                                  << (value ? "ON" : "OFF") << std::endl;
                    } else {
                        std::cerr << "Address not found: " << address << std::endl;
                    }
                } else {
                    std::cerr << "Invalid arguments. Usage: get_yx <address>" << std::endl;
                }
            } else if (cmd == "set_yc") {
                uint32_t address;
                double value;
                if (iss >> address >> value) {
                    database->updateYCValue(address, value);
                    std::cout << "Set YC " << address << " to " << value << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: set_yc <address> <value>" << std::endl;
                }
            } else if (cmd == "get_yc") {
                uint32_t address;
                if (iss >> address) {
                    TelemetryPoint tp;
                    if (database->readYC(address, tp)) {
                        std::cout << "YC " << address << " = " << tp.value << std::endl;
                    } else {
                        std::cerr << "Address not found: " << address << std::endl;
                    }
                } else {
                    std::cerr << "Invalid arguments. Usage: get_yc <address>" << std::endl;
                }
            } else {
                std::cerr << "Unknown command: " << cmd << std::endl;
            }
        }
    }

    // 清理工作
    running = false;
    if (send_thread.joinable()) {
        send_thread.join();
    }
    
    // 关闭端点
    if (endpoint) endpoint->close();
    
    std::cout << "Program exited cleanly" << std::endl;
    return 0;
}