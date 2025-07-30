#include "channel_manager.h"
#include "config_parser.h"
#include "database.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <cstring>
#include <thread>
#include <chrono>
#include <unordered_map>

std::atomic<bool> running{true};

void signalHandler(int signal) {
    running = false;
    
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // 处理命令行参数
        if (argc > 1 && strcmp(argv[1], "--update") == 0) {
            if (argc < 3) {
                std::cerr << "Usage: " << argv[0] << " --update <config.json|config.yaml|config.toml>" << std::endl;
                return 1;
            }
            
            // 使用解析器工厂创建适当的解析器
            auto parser = ConfigParserFactory::createParser(argv[2]);
            auto channels = parser->parse(argv[2]);
            
            // 保存到数据库
            Database db;
            db.replaceChannels(channels);
            
            std::cout << "Configuration updated from " << argv[2] << std::endl;
            return 0;
        }
        
        // 从数据库加载初始配置
        Database db;
        auto channels = db.loadChannels();
        
        ChannelManager manager;
        std::unordered_map<std::string, ChannelConfig> last_configs;
        
        // 初始加载配置
        for (const auto& config : channels) {
            manager.addChannel(std::make_unique<ProtocolChannel>(config.name, config.input, config.output));
            last_configs[config.name] = config;
        }
        
        std::cout << "Protocol converter started." << std::endl;
        
        // 主循环：定期检查数据库更新
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            try {
                // 加载最新配置
                auto new_configs = db.loadChannels();
                std::unordered_map<std::string, ChannelConfig> new_config_map;
                for (const auto& config : new_configs) {
                    new_config_map[config.name] = config;
                }
                
                // 检查更新：停止并移除已删除或修改的通道
                for (auto it = last_configs.begin(); it != last_configs.end(); ) {
                    const auto& name = it->first;
                    if (new_config_map.find(name) == new_config_map.end()) {
                        // 通道被删除
                        manager.removeChannel(name);
                        std::cout << "Removed channel: " << name << std::endl;
                        it = last_configs.erase(it);
                    } else if (new_config_map[name] != it->second) {
                        // 通道配置被修改
                        manager.removeChannel(name);
                        std::cout << "Updated channel: " << name << std::endl;
                        it = last_configs.erase(it);
                    } else {
                        ++it;
                    }
                }
                
                // 添加新增或修改的通道
                for (const auto& config : new_configs) {
                    const auto& name = config.name;
                    if (last_configs.find(name) == last_configs.end()) {
                        manager.addChannel(std::make_unique<ProtocolChannel>(name, config.input, config.output));
                        last_configs[name] = config;
                        std::cout << "Added channel: " << name << std::endl;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error updating channels: " << e.what() << std::endl;
            }
        }
        
        // 停止所有通道
        std::cout << "Stopping all channels..." << std::endl;
        manager.stopAll();
        std::cout << "Protocol converter stopped." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}