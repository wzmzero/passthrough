#include "common.h"
#include "database.h"
#include "instance.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <memory>
#include <vector>

std::atomic<bool> running(true);

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    running = false;
}

int main() {
    signal(SIGINT, signalHandler);
    
    Database db("config/config.db");
    auto instancesParam = db.loadInstances();
    
    if (instancesParam.empty()) {
        std::cout << "Initializing sample data..." << std::endl;
        // db.initSampleData(); // 如果需要初始化数据
        instancesParam = db.loadInstances();
        
        if (instancesParam.empty()) {
            std::cerr << "No instances found!" << std::endl;
            return -1;
        }
    }
    
    std::vector<std::unique_ptr<Instance>> instances;
    
    // 创建并打开所有实例
    for (auto& param : instancesParam) {
        auto instance = std::make_unique<Instance>(param);
        std::cout << "Created instance: " << param.name << std::endl;
        
        if (!instance->open()) {
            std::cerr << "Failed to open instance: " << param.name << std::endl;
            return -1;
        }
        
        // 设置回调（可选）
        instance->setEndpointLogCallback([](const std::string& msg) {
            std::cout << "Endpoint log: " << msg << std::endl;
        });
        
        instance->setEndpointErrorCallback([](const std::string& error) {
            std::cerr << "Endpoint error: " << error << std::endl;
        });
        
        instances.push_back(std::move(instance));
    }
    
    std::cout << "All instances started. Press Ctrl+C to exit." << std::endl;
    
    // 主循环只需等待退出信号
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // 清理所有实例
    for (auto& inst : instances) {
        inst->close();
    }
    
    std::cout << "Program terminated successfully." << std::endl;
    return 0;
}