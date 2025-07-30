// passthrough_manager.cpp
#include "ChannelManager.hpp"
#include "EndpointFactory.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <fstream> // 添加 fstream 头文件
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// 字符串到端点类型的转换函数
Endpoint::Type stringToEndpointType(const std::string& type_str) {
    if (type_str == "serial") return Endpoint::SERIAL;
    if (type_str == "tcp_client") return Endpoint::TCP_CLIENT;
    if (type_str == "tcp_server") return Endpoint::TCP_SERVER;
    if (type_str == "udp_client") return Endpoint::UDP_CLIENT;
    if (type_str == "udp_server") return Endpoint::UDP_SERVER;
    throw std::runtime_error("Unknown endpoint type: " + type_str);
}

std::atomic<bool> running(true);

void signalHandler(int) {
    running = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }
    
    // 加载配置文件
    std::ifstream config_file(argv[1]);
    if (!config_file.is_open()) {
        std::cerr << "Failed to open config file: " << argv[1] << std::endl;
        return 1;
    }
    
    json config;
    try {
        config_file >> config;
    } catch (const json::parse_error& e) {
        std::cerr << "Config parse error: " << e.what() << std::endl;
        return 1;
    }
    
    auto& manager = ChannelManager::instance();
    
    // 创建通道
    for (auto& channel_cfg : config["channels"]) {
        try {
            std::string channel_name = channel_cfg["name"];
            // 解析输入端点
            std::string input_type_str = channel_cfg["input"]["type"];
            Endpoint::Type input_type = stringToEndpointType(input_type_str);
            auto input = EndpointFactory::createEndpoint(input_type, channel_cfg["input"]);
            
            // 解析输出端点
            std::string output_type_str = channel_cfg["output"]["type"];
            Endpoint::Type output_type = stringToEndpointType(output_type_str);
            auto output = EndpointFactory::createEndpoint(output_type, channel_cfg["output"]);
            
            if (input && output) {
                int channel_id = manager.addChannel(std::move(input), std::move(output));
                if (manager.startChannel(channel_id)) {
                    std::cout << "Started channel: " << channel_name 
                              << " [ID: " << channel_id << "]" << std::endl;
                } else {
                    std::cerr << "Failed to start channel: " << channel_name << std::endl;
                }
            } else {
                std::cerr << "Failed to create endpoints for channel: " << channel_name << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error creating channel: " << e.what() << std::endl;
        } 
    }
    
    std::cout << "Passthrough manager started with " 
              << manager.getAllChannelIds().size() << " channels" << std::endl;

    
    // 监控循环
    while (running) {
        for (int id : manager.getAllChannelIds()) {
            if (!manager.isChannelRunning(id)) {
                std::cerr << "Channel " << id << " stopped. Restarting..." << std::endl;
                manager.stopChannel(id);
                manager.startChannel(id);
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    // 清理
    manager.stopAllChannels();
    std::cout << "Passthrough manager stopped" << std::endl;
    return 0;
}