// src/main.cpp
#include "ChannelBridge.hpp"
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>
#include "Logger.h" // 添加头文件

// 定义日志宏
#define LOG(prefix, level, message) \
    Logger::instance().log(prefix, level, message)

using json = nlohmann::json;
namespace fs = std::filesystem;

std::vector<BridgeConfig> load_config(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filename);
    }

    json config_json;
    file >> config_json;

    std::vector<BridgeConfig> bridges;
    for (const auto& item : config_json["bridges"]) {
        BridgeConfig bridge;
        bridge.id = item["id"];
        
        auto parse_channel = [](const json& j) -> ChannelConfig {
            ChannelConfig ch;
            ch.type = j["type"];
            if (j.contains("host")) ch.host = j["host"];
            if (j.contains("port")) ch.port = j["port"];
            if (j.contains("device")) ch.device = j["device"];
            if (j.contains("baud_rate")) ch.baudRate = j["baud_rate"];
            return ch;
        };
        
        bridge.channel1 = parse_channel(item["channel1"]);
        bridge.channel2 = parse_channel(item["channel2"]);
        
        bridges.push_back(bridge);
    }
    
    return bridges;
}

int main() {
    try {
        const std::string config_file = "bridges.json";
        auto bridge_configs = load_config(config_file);
        
        if (bridge_configs.empty()) {
            LOG("main",LogLevel::ERROR, "没有Bridge配置");
            return 1;
        }
        
        boost::asio::io_context io;
        std::vector<std::unique_ptr<ChannelBridge>> bridges;
        
        LOG("main", LogLevel::INFO, "应用程序启动");
        for (const auto& config : bridge_configs) {
            try {
                auto bridge = std::make_unique<ChannelBridge>(io, config);
                bridge->start();
                bridges.push_back(std::move(bridge));
                LOG("main", LogLevel::INFO, "Bridge #" + std::to_string(config.id) + " started");
   
            } catch (const std::exception& e) {
                LOG("main", LogLevel::ERROR, "Bridge异常" + std::to_string(config.id) + ": " + e.what());
            }
        }
        
        if (bridges.empty()) {
            LOG("main", LogLevel::ERROR, "Bridge启动失败，程序退出");
            return 1;
        }
        
 
        // 添加信号处理
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            LOG("main", LogLevel::INFO, "接收到退出信号，程序停止");
            for (auto& bridge : bridges) {
                bridge->stop();
            }
            io.stop();
        });
        
        io.run();
        LOG("main", LogLevel::INFO, "所有桥已停止，程序退出");
 
    }
    catch (const std::exception& e) {
        Logger::instance().log("main", LogLevel::ERROR, "Fatal error: " + std::string(e.what()));
        return 1;
    }
    return 0;
}