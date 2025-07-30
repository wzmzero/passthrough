#include "channel_manager.h"
#include "config_parser.h"  // 更新为新的解析器工厂
#include "database.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <cstring>

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
        
        // 从数据库加载配置
        Database db;
        auto channels = db.loadChannels();
        
        ChannelManager manager;

        // 创建通道
        for (const auto& config : channels) {
            manager.addChannel(
                std::make_unique<ProtocolChannel>(
                    config.name,
                    std::move(config.input), 
                    std::move(config.output)
                )
            );
            std::cout << "Created channel: " << config.name << std::endl;
        }
        
        // 启动所有通道
        manager.startAll();
        std::cout << "Protocol converter started. Press Ctrl+C to stop." << std::endl;
        
        // 主循环
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
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