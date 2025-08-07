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
#include "logrecord.h"
std::atomic<bool> running{true};
std::mutex configMutex;
void signalHandler(int signal) {
    running = false;
    if(signal == SIGINT) {
        LOG_WARNING("Received Ctrl+C Signal, Shutting down...");
    } else if (signal == SIGTERM) {
        LOG_WARNING("Received kill <PID>, shutting down...");
    }
}

 
int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    LogRecord::init(true, true, "logs");
    try {
        // 处理命令行参数 
         if (argc > 1 && strcmp(argv[1], "--update") == 0) {
            if (argc < 3) {
                LOG_ERROR("Usage: %s --update <config.json|config.yaml|config.toml>", argv[0]);
                return 1;
            }
            
            // 使用解析器工厂创建适当的解析器
            auto parser = ConfigParserFactory::createParser(argv[2]);
            auto channels = parser->parse(argv[2]);
            
            // 保存到数据库
            Database db("config/config.db");
            db.replaceChannels(channels);
            LOG_INFO("Configuration updated from %s", argv[2]);
            return 0;
        }
        
        
        // 从数据库加载初始配置
        Database db("config/config.db");
        auto channels = db.loadChannels();
   

        ChannelManager manager;
        std::unordered_map<std::string, ChannelConfig> last_configs;
        
        // 初始加载配置
        for (const auto& config : channels) {
            manager.addChannel(std::make_unique<ProtocolChannel>(
                config.name, config.input, config.output, manager.getThreadPool()));
            last_configs[config.name] = config;
        }
        LOG_INFO("Starting protocol converter...");
        
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
                        manager.removeChannel(name);
                        it = last_configs.erase(it);
                    } else if (new_config_map[name] != it->second) {
                        manager.removeChannel(name);
                        it = last_configs.erase(it);
                    } else {
                        ++it;
                    }
                }
                
                // 添加新增或修改的通道
                for (const auto& config : new_configs) {
                    const auto& name = config.name;
                    if (last_configs.find(name) == last_configs.end()) {
                        manager.addChannel(std::make_unique<ProtocolChannel>(
                            name, config.input, config.output, manager.getThreadPool()));
                        last_configs[name] = config;
                    }
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Error updating channels: %s", e.what());
            }
        }
        // 停止所有通道
        manager.stopAll();
    }
    catch (const std::exception& e) {
        LOG_ERROR("Fatal error: %s", e.what());
        return 1;
    }
    
    return 0;
}