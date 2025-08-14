// test_dirver_v2/tests/test_driver.cpp
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
#include "udp_server_endpoint.h"
#include "udp_client_endpoint.h"
#include "serial_endpoint.h"
#include "driver_modbus_m.h"
#include "driver_modbus_s.h"

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

// 打印Modbus数据点
void printModbusDataPoints(const std::vector<ModbusDataPoint>& points) {
    for (const auto& point : points) {
        std::cout << "  Address: 0x" << std::hex << std::setw(4) << std::setfill('0') << point.address
                  << ", Value: " << std::dec << point.value << "\n";
    }
}

int main(int argc, char* argv[]) {
    int interval_ms = 0; // 发送间隔（毫秒）
    std::string send_data; // 要发送的数据
    std::mutex data_mutex; // 保护发送数据
    std::atomic<bool> running(true); // 控制线程运行
    std::atomic<bool> periodic_active(false); // 周期性发送是否激活
    bool is_modbus_master = false;
    bool is_modbus_slave = false;
    std::unique_ptr<DriverModbusM> modbus_master;
    std::unique_ptr<DriverModbusS> modbus_slave;
    uint8_t modbus_slave_address = 1;
    ModbusTransportMode transport_mode = ModbusTransportMode::RTU;

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
                  << "  master : Modbus Master\n"
                  << "  slave  : Modbus Slave\n"
                  << "Transports:\n"
                  << "  tcp    : TCP transport\n"
                  << "  udp    : UDP transport\n"
                  << "  serial : Serial transport\n"
                  << "Options for TCP/UDP:\n"
                  << "  For master: <server_ip> <server_port> <slave_address>\n"
                  << "  For slave: <listen_port> <slave_address>\n"
                  << "Options for serial:\n"
                  << "  For master/slave: <device> <baud_rate> <slave_address>\n"
                  << "Example:\n"
                  << "  " << argv[0] << " master tcp 192.168.1.100 502 1\n"
                  << "  " << argv[0] << " slave tcp 5020 1\n"
                  << "  " << argv[0] << " master serial COM1 9600 1\n"
                  << "  " << argv[0] << " slave serial COM2 9600 1\n";
        return 1;
    }

    std::string mode = argv[1];
    std::string transport = argv[2];
    is_modbus_master = (mode == "master");
    is_modbus_slave = (mode == "slave");
    
    if (!is_modbus_master && !is_modbus_slave) {
        std::cerr << "Invalid mode. Must be 'master' or 'slave'.\n";
        return 1;
    }

    std::unique_ptr<Endpoint> endpoint;
    try {
        if (transport == "tcp") {
            if (is_modbus_master && argc == 6) {
                std::string server_ip = argv[3];
                int server_port = std::stoi(argv[4]);
                modbus_slave_address = std::stoi(argv[5]);
                endpoint = std::make_unique<TcpClientEndpoint>(server_ip, server_port);
                transport_mode = ModbusTransportMode::TCP;
            } 
            else if (is_modbus_slave && argc == 5) {
                int listen_port = std::stoi(argv[3]);
                modbus_slave_address = std::stoi(argv[4]);
                endpoint = std::make_unique<TcpServerEndpoint>(listen_port);
                transport_mode = ModbusTransportMode::TCP;
            }
            else {
                std::cerr << "Invalid arguments for TCP transport.\n";
                return 1;
            }
        }
        else if (transport == "udp") {
            if (is_modbus_master && argc == 6) {
                std::string server_ip = argv[3];
                int server_port = std::stoi(argv[4]);
                modbus_slave_address = std::stoi(argv[5]);
                endpoint = std::make_unique<UdpClientEndpoint>(server_ip, server_port);
                transport_mode = ModbusTransportMode::TCP; // Use TCP format for UDP
            } 
            else if (is_modbus_slave && argc == 5) {
                int listen_port = std::stoi(argv[3]);
                modbus_slave_address = std::stoi(argv[4]);
                endpoint = std::make_unique<UdpServerEndpoint>(listen_port);
                transport_mode = ModbusTransportMode::TCP; // Use TCP format for UDP
            }
            else {
                std::cerr << "Invalid arguments for UDP transport.\n";
                return 1;
            }
        }
        else if (transport == "serial") {
            if (argc == 6) {
                std::string device = argv[3];
                int baud_rate = std::stoi(argv[4]);
                modbus_slave_address = std::stoi(argv[5]);
                endpoint = std::make_unique<SerialEndpoint>(device, baud_rate);
                transport_mode = ModbusTransportMode::RTU;
            }
            else {
                std::cerr << "Invalid arguments for serial transport.\n";
                return 1;
            }
        }
        else {
            std::cerr << "Invalid transport type. Must be 'tcp', 'udp' or 'serial'.\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    // 创建Modbus驱动实例
    if (is_modbus_master) {
        modbus_master = std::make_unique<DriverModbusM>(modbus_slave_address, transport_mode);
    } else if (is_modbus_slave) {
        modbus_slave = std::make_unique<DriverModbusS>(modbus_slave_address, transport_mode);
    }

    // 设置回调
    endpoint->setDataCallback(dataCallback);
    endpoint->setLogCallback(logCallback);
    endpoint->setErrorCallback(errorCallback);
    
    // 设置Modbus从站的数据回调
    if (is_modbus_slave && modbus_slave) {
        endpoint->setDataCallback([&](const uint8_t* data, size_t len) {
            dataCallback(data, len); // 调用原始回调
            
            ModbusFrameInfo requestInfo;
            if (modbus_slave->parseRequest(data, len, requestInfo)) {
                std::cout << "\n=== Modbus Slave Request ===" << std::endl;
                std::cout << "Function: " << static_cast<int>(requestInfo.functionCode)
                          << ", Success: " << !requestInfo.isException << std::endl;
                printModbusDataPoints(requestInfo.dataPoints);
                
                // 处理请求
                ModbusFrameInfo responseInfo = modbus_slave->processRequest(requestInfo);
                
                // 生成响应帧
                auto response = modbus_slave->createResponse(responseInfo);
                
                std::cout << "\n=== Modbus Slave Response ===" << std::endl;
                std::cout << "Function: " << static_cast<int>(responseInfo.functionCode)
                          << ", Success: " << !responseInfo.isException << std::endl;
                printModbusDataPoints(responseInfo.dataPoints);
                
                // 发送响应
                endpoint->write(response.data(), response.size());
            }
        });
    }
    
    // 设置Modbus主站的数据回调
    if (is_modbus_master && modbus_master) {
        endpoint->setDataCallback([&](const uint8_t* data, size_t len) {
            dataCallback(data, len); // 调用原始回调
            
            ModbusFrameInfo responseInfo;
            if (modbus_master->parseResponse(data, len, responseInfo)) {
                std::cout << "\n=== Modbus Master Response ===" << std::endl;
                std::cout << "Function: " << static_cast<int>(responseInfo.functionCode)
                          << ", Success: " << !responseInfo.isException << std::endl;
                printModbusDataPoints(responseInfo.dataPoints);
            }
        });
    }
    
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
    std::cout << "\n========================================" << std::endl;
    std::cout << "Modbus " << (is_modbus_master ? "Master" : "Slave") 
              << " Mode (" << transport << " transport)" << std::endl;
    std::cout << "Slave Address: " << static_cast<int>(modbus_slave_address) << std::endl;
    std::cout << "========================================" << std::endl;
    
    if (is_modbus_master) {
        std::cout << "Available commands:\n"
                  << "  read_coils <start> <count>        - Read coils\n"
                  << "  read_registers <start> <count>    - Read holding registers\n"
                  << "  write_coil <address> <value>      - Write single coil (0 or 1)\n"
                  << "  write_register <address> <value>  - Write single register\n"
                  << "  shgk_write <address> <value>      - SHGK write (32-bit value)\n"
                  << "  exit                             - Quit program\n";
    } else if (is_modbus_slave) {
        std::cout << "Memory manipulation commands:\n"
                  << "  set_coil <address> <0|1>          - Set coil value\n"
                  << "  get_coil <address>                - Get coil value\n"
                  << "  set_reg <address> <value>         - Set holding register\n"
                  << "  get_reg <address>                 - Get holding register\n"
                  << "  exit                             - Quit program\n";
    }
    
    if (interval_ms > 0) {
        std::cout << "\nPeriodic sending every " << interval_ms << "ms is available\n";
        std::cout << "Use 'start' to begin periodic sending, 'stop' to pause\n";
    }
    
    std::string input;
    while (true) {
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
        else if (is_modbus_master && !input.empty()) {
            // 处理Modbus主站命令
            std::istringstream iss(input);
            std::string cmd;
            iss >> cmd;
            
            if (cmd == "read_coils") {
                uint16_t start, count;
                if (iss >> start >> count) {
                    auto request = modbus_master->createReadRequest(
                        ModbusFunctionCode::READ_COILS, start, count);
                    endpoint->write(request.data(), request.size());
                    std::cout << "Sent read coils request (start: " << start 
                              << ", count: " << count << ")" << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: read_coils <start> <count>" << std::endl;
                }
            } else if (cmd == "read_registers") {
                uint16_t start, count;
                if (iss >> start >> count) {
                    auto request = modbus_master->createReadRequest(
                        ModbusFunctionCode::READ_HOLDING_REGISTERS, start, count);
                    endpoint->write(request.data(), request.size());
                    std::cout << "Sent read registers request (start: " << start 
                              << ", count: " << count << ")" << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: read_registers <start> <count>" << std::endl;
                }
            } else if (cmd == "write_coil") {
                uint16_t address;
                uint16_t value;
                if (iss >> address >> value) {
                    auto request = modbus_master->createWriteRequest(
                        ModbusFunctionCode::WRITE_SINGLE_COIL, address, 
                        value ? 0xFF00 : 0x0000);
                    endpoint->write(request.data(), request.size());
                    std::cout << "Sent write coil request (address: " << address 
                              << ", value: " << (value ? "ON" : "OFF") << ")" << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: write_coil <address> <0|1>" << std::endl;
                }
            } else if (cmd == "write_register") {
                uint16_t address, value;
                if (iss >> address >> value) {
                    auto request = modbus_master->createWriteRequest(
                        ModbusFunctionCode::WRITE_SINGLE_REGISTER, address, value);
                    endpoint->write(request.data(), request.size());
                    std::cout << "Sent write register request (address: " << address 
                              << ", value: " << value << ")" << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: write_register <address> <value>" << std::endl;
                }
            } else if (cmd == "shgk_write") {
                uint16_t address;
                uint32_t value;
                if (iss >> address >> value) {
                    auto request = modbus_master->createSHGKWriteRequest(address, value);
                    endpoint->write(request.data(), request.size());
                    std::cout << "Sent SHGK write request (address: " << address 
                              << ", value: " << value << ")" << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: shgk_write <address> <value>" << std::endl;
                }
            } else {
                std::cerr << "Unknown command: " << cmd << std::endl;
            }
        } else if (is_modbus_slave && !input.empty()) {
            // 处理Modbus从站命令
            std::istringstream iss(input);
            std::string cmd;
            iss >> cmd;
            
            if (cmd == "set_coil") {
                uint16_t address;
                uint16_t value;
                if (iss >> address >> value) {
                    modbus_slave->setCoil(address, value != 0);
                    std::cout << "Set coil " << address << " to " 
                              << (value ? "ON" : "OFF") << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: set_coil <address> <0|1>" << std::endl;
                }
            } else if (cmd == "get_coil") {
                uint16_t address;
                if (iss >> address) {
                    bool value = modbus_slave->getCoil(address);
                    std::cout << "Coil " << address << " = " 
                              << (value ? "ON" : "OFF") << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: get_coil <address>" << std::endl;
                }
            } else if (cmd == "set_reg") {
                uint16_t address, value;
                if (iss >> address >> value) {
                    modbus_slave->setHoldingRegister(address, value);
                    std::cout << "Set register " << address << " to " << value << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: set_reg <address> <value>" << std::endl;
                }
            } else if (cmd == "get_reg") {
                uint16_t address;
                if (iss >> address) {
                    uint16_t value = modbus_slave->getHoldingRegister(address);
                    std::cout << "Register " << address << " = " << value << std::endl;
                } else {
                    std::cerr << "Invalid arguments. Usage: get_reg <address>" << std::endl;
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
    
    endpoint->close();
    std::cout << "Program exited cleanly" << std::endl;
    return 0;
}
 