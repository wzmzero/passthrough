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
#include "common.h"
#include "driver_104_m.h" // 使用现有的头文件

// 使用 IEC104 命名空间中的类型
using IEC104::TypeIdentify;
using IEC104::TransCause;
using IEC104::InfoObjectAddr;
using IEC104::ASDU;
using IEC104::APDU;

// 打印 IEC 104 数据点
void printIEC104DataPoints(const ASDU& asdu) {
    std::cout << "Type: " << static_cast<int>(asdu.type)
              << ", Cause: " << static_cast<int>(asdu.cause)
              << ", Elements: " << static_cast<int>(asdu.numElements) << "\n";
              
    for (int i = 0; i < asdu.numElements; i++) {
        std::cout << "  Address: " << asdu.ioAddrs[i]
                  << ", Value: " << asdu.values[i] << "\n";
    }
}

int main(int argc, char* argv[]) {
    int interval_ms = 0;
    std::mutex data_mutex;
    std::atomic<bool> running(true);
    std::atomic<bool> periodic_active(false);
    bool is_104_master = false;
    bool is_104_slave = false;
    std::unique_ptr<Driver104M> iec104_master;
    std::unique_ptr<Endpoint> endpoint;

    // 创建四遥数据库
    auto database = std::make_shared<SimpleMemoryDatabase>();
    
    // 初始化测试数据
    database->updateYCValue(40001, 25.5);
    database->updateYCValue(40002, 101.3);
    database->updateYXValue(10001, true);

    // 解析 -n 参数
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            interval_ms = std::atoi(argv[i + 1]);
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
                  << "  104_slave  : Not implemented\n" // 从站暂未实现
                  << "Transports:\n"
                  << "  tcp        : TCP transport\n"
                  << "Options:\n"
                  << "  For master: <server_ip> <server_port>\n"
                  << "Example:\n"
                  << "  " << argv[0] << " 104_master tcp 192.168.1.100 2404\n";
        return 1;
    }

    std::string mode = argv[1];
    std::string transport = argv[2];
    is_104_master = (mode == "104_master");
    
    if (!is_104_master) {
        std::cerr << "Only 104_master mode is currently implemented\n";
        return 1;
    }
    
    if (transport != "tcp") {
        std::cerr << "IEC 104 requires TCP transport\n";
        return 1;
    }

    if (argc != 5) {
        std::cerr << "Invalid arguments for IEC 104 master\n"
                  << "Usage: " << argv[0] << " 104_master tcp <server_ip> <server_port>\n";
        return 1;
    }
    
    std::string server_ip = argv[3];
    int server_port = std::stoi(argv[4]);
    
    try {
        endpoint = std::make_unique<TcpClientEndpoint>(server_ip, server_port);
        iec104_master = std::make_unique<Driver104M>(1); // 公共地址设为1
        
        // 设置数据回调
        // iec104_master->setDataCallback([&](uint32_t addr, float value) {
        //     std::cout << "\n[104 Callback] Addr: " << addr << ", Value: " << value << "\n";
        // });
        
        // 设置端点回调
        endpoint->setDataCallback([&](const uint8_t* data, size_t len) {
            iec104_master->write(data, len);
        });
        
        endpoint->setLogCallback([](const std::string& msg) {
            std::cout << "[LOG] " << msg << std::endl;
        });
        
        endpoint->setErrorCallback([](const std::string& msg) {
            std::cerr << "[ERROR] " << msg << std::endl;
        });
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    // 打开端点
    if (!endpoint->open()) {
        std::cerr << "Failed to start endpoint" << std::endl;
        return 1;
    }
    
    // 启动104主站
    if (!iec104_master->open()) {
        std::cerr << "Failed to start IEC 104 master" << std::endl;
        return 1;
    }

    // 用户界面
    std::cout << "\n========================================" << std::endl;
    std::cout << "IEC 104 Master Mode (TCP)" << std::endl;
    std::cout << "Server: " << server_ip << ":" << server_port << std::endl;
    std::cout << "Available commands:\n"
              << "  gi        - Send general interrogation\n"
              << "  cmd <addr> <0|1> - Send control command\n"
              << "  exit      - Quit program\n";
    std::cout << "========================================\n";

    std::string input;
    while (true) {
        std::cout << "\n> ";
        std::getline(std::cin, input);
        
        if (input == "exit") {
            break;
        } 
        else if (input == "gi") {
            if (iec104_master->sendGeneralCall()) {
                std::cout << "Sent general interrogation" << std::endl;
            } else {
                std::cerr << "Failed to send general interrogation" << std::endl;
            }
        }
        else if (input.substr(0, 3) == "cmd") {
            std::istringstream iss(input);
            std::string cmd;
            uint32_t addr;
            uint16_t value;
            
            iss >> cmd >> addr >> value;
            
            if (iss && (value == 0 || value == 1)) {
                if (iec104_master->sendCommand(addr, value == 1)) {
                    std::cout << "Sent command to address " << addr 
                              << ", value: " << value << std::endl;
                } else {
                    std::cerr << "Failed to send command" << std::endl;
                }
            } else {
                std::cerr << "Invalid command. Usage: cmd <address> <0|1>" << std::endl;
            }
        }
        else {
            std::cerr << "Unknown command: " << input << std::endl;
        }
    }

    // 清理工作
    iec104_master->close();
    endpoint->close();
    
    std::cout << "Program exited cleanly" << std::endl;
    return 0;
}