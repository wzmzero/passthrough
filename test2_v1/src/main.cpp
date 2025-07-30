// src/main.cpp
#include "ChannelBridge.hpp"
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>

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
            std::cerr << "No bridges configured in " << config_file << std::endl;
            return 1;
        }
        
        boost::asio::io_context io;
        std::vector<std::unique_ptr<ChannelBridge>> bridges;
        
        std::cout << "Starting " << bridge_configs.size() << " bridge(s)...\n";
        
        for (const auto& config : bridge_configs) {
            try {
                auto bridge = std::make_unique<ChannelBridge>(io, config);
                bridge->start();
                bridges.push_back(std::move(bridge));
                std::cout << "Bridge #" << config.id << " started\n";
            } catch (const std::exception& e) {
                std::cerr << "Failed to start bridge #" << config.id 
                          << ": " << e.what() << std::endl;
            }
        }
        
        if (bridges.empty()) {
            std::cerr << "No bridges started successfully\n";
            return 1;
        }
        
        // // 添加状态显示定时器
        // auto status_timer = std::make_shared<boost::asio::steady_timer>(io);
        // std::function<void()> show_status = [&] {
        //     status_timer->expires_after(std::chrono::seconds(5));
        //     status_timer->async_wait([&](const boost::system::error_code& ec) {
        //         if (ec) return;
                
        //         std::cout << "\n=== Bridge Status ===";
        //         for (const auto& bridge : bridges) {
        //             if (bridge->isRunning()) {
        //                 std::cout << "\nBridge #" << bridge->getId() << ": RUNNING";
        //             } else {
        //                 std::cout << "\nBridge #" << bridge->getId() << ": STOPPED";
        //             }
        //         }
        //         std::cout << "\n====================\n";
                
        //         show_status();
        //     });
        // };
        
        // show_status();
        
        // 添加信号处理
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            std::cout << "\nStopping bridges...\n";
            for (auto& bridge : bridges) {
                bridge->stop();
            }
            io.stop();
        });
        
        io.run();
        
        std::cout << "All bridges stopped\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}