#include "common.h"
#include "driver_modbus_m.h"
#include "database.h"
#include "tcp_client_endpoint.h"
#include <iostream>
#include <thread>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <iomanip>
#include <functional>
#include <chrono>
#include <atomic>
#include <csignal>

// 重定义hexStr函数用于测试输出
static std::string hexStr(const std::vector<uint8_t>& v) {
    std::ostringstream oss;
    for (auto b : v) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
    }
    return oss.str();
}

// 全局标志，用于控制程序退出
std::atomic<bool> running(true);

// 信号处理函数
void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    running = false;
}

int main() {
    // 注册信号处理
    signal(SIGINT, signalHandler);
    
    // 创建数据库对象
    Database db("config/config.db");
    db.initSampleData();
    // 检查数据库是否为空，为空则初始化样本数据
    if (db.loadInstances().empty()) {
        std::cout << "Database is empty. Initializing sample data..." << std::endl;
    }
    // 从数据库加载所有实例
    auto instances = db.loadInstances();
    if (instances.empty()) {
        std::cerr << "Error: Failed to load instances even after initialization!" << std::endl;
        return -1;
    }
    // 查找第一个 Modbus 主站实例
    InstanceParm* modbusInstance = nullptr;
    for (auto& inst : instances) {
        std::cout<< "driver type:" << inst.driverParam.type<< std::endl;
        if (inst.driverParam.type == MODBUS_M) {
            modbusInstance = &inst;
            break;
        }
    }
    if (!modbusInstance) {
        std::cerr << "Error: No Modbus master instance found in database!" << std::endl;
        return -1;
    }
    std::cout << "Found Modbus master instance: " << modbusInstance->name << "\n";
    
    // 创建 TCP 客户端端点
    auto& epConfig = modbusInstance->channelParam;
    std::unique_ptr<TcpClientEndpoint> endpoint;
    
    if (epConfig.type == "TCP") {
        endpoint = std::make_unique<TcpClientEndpoint>(epConfig.ip, epConfig.port);
        std::cout << "Created TCP endpoint: " << epConfig.ip << ":" << epConfig.port << "\n";
    } 
    // 可以添加其他端点类型支持
    else {
        std::cerr << "Unsupported endpoint type: " << epConfig.type << "\n";
        return -1;
    }
    // 创建 Modbus 主站驱动
    DriverModbusM drv(modbusInstance->vecDevInfo, modbusInstance->driverParam.mModbus_param);
    
    // 打开端点
    if (!endpoint->open()) {
        std::cerr << "Failed to open endpoint!\n";
        return -1;
    }
    // 打开驱动
    if (!drv.open()) {
        std::cerr << "Failed to open driver!\n";
        return -1;
    }
        // 设置端点的数据接收回调
    endpoint->setDataCallback([&drv](const uint8_t* data, size_t len) {
        std::cout << "Endpoint received: " << len << " bytes\n";
        drv.write(data,len);
        std::vector<uint8_t> frame(data, data + len);
        std::cout << "Received data: " << hexStr(frame) << "\n";
    });
    
    // 设置端点错误回调
    endpoint->setErrorCallback([](const std::string& error) {
        std::cerr << "Endpoint error: " << error << "\n";
    });
    // 设置端点日志回调
    endpoint->setLogCallback([](const std::string& msg) {
        std::cout << "Endpoint log: " << msg << "\n";
    });
    // 主循环
    int counter = 0;
    while (running) {
        // 检查驱动发送队列并发送数据
        {
            std::lock_guard<std::mutex> lock(drv.sendMutex_);
            while (!drv.sendqueue_.empty()) {
                auto frame = drv.sendqueue_.front();
                drv.sendqueue_.pop();
                // 通过端点发送数据
                if (endpoint->isConnected()) {
                    std::cout << "Sent frame via endpoint: " << hexStr(frame) << "\n";
                } else {
                    std::cerr << "Cannot send frame - endpoint not connected!\n";
                }
            }
        }
        // 打印端点状态
        if (counter % 30 == 0) {
            std::cout << "Endpoint status: "
                      << (endpoint->isRunning() ? "Running" : "Stopped") << ", "
                      << (endpoint->isConnected() ? "Connected" : "Disconnected") << "\n";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 关闭驱动和端点
    drv.close();
    endpoint->close();
    std::cout << "Test completed successfully.\n";
    return 0;
}