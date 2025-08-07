 
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <cstring>
#include "database.h"
#include "config_parser.h"
#include <csignal>
// 全局状态
std::atomic_bool running{true};
std::mutex configMutex;
std::vector<ChannelConfig> currentChannels;

// 信号处理函数
void signalHandler(int signum) {
    std::cout << "\n收到中断信号 (" << signum << ")，正在关闭程序..." << std::endl;
    running = false;
}

// 数据库变更回调函数
void dbChangeCallback(const std::string& table, int op, int rowid, const std::any& data) {
    std::lock_guard<std::mutex> lock(configMutex);
    std::string operation;
    switch (op) {
        case SQLITE_INSERT: operation = "插入"; break;
        case SQLITE_UPDATE: operation = "更新"; break;
        case SQLITE_DELETE: operation = "删除"; break;
        default: operation = "未知操作"; break;
    }
    
    std::cout << "[数据库变更] 表: " << table 
              << ", 操作: " << operation 
              << ", 行ID: " << rowid << std::endl;
    
    if (data.has_value()) {
        try {
            if (table == "endpoints") {
                auto endpoint = std::any_cast<EndpointConfig>(data);
                std::cout << "  端点: " << endpoint.type;
                if (!endpoint.ip.empty()) std::cout << ", IP: " << endpoint.ip;
                if (endpoint.port > 0) std::cout << ", 端口: " << endpoint.port;
                if (!endpoint.serial_port.empty()) std::cout << ", 串口: " << endpoint.serial_port;
                if (endpoint.baud_rate > 0) std::cout << ", 波特率: " << endpoint.baud_rate;
                std::cout << std::endl;
            } else if (table == "channels") {
                auto channel = std::any_cast<ChannelConfig>(data);
                std::cout << "  通道: " << channel.name 
                          << ", 输入ID: " << channel.input_id 
                          << ", 输出ID: " << channel.output_id << std::endl;
            }
        } catch (...) {
            std::cout << "  数据解析失败" << std::endl;
        }
    }
}

// 比较两个配置是否相同
bool compareChannels(const std::vector<ChannelConfig>& a, const std::vector<ChannelConfig>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].name != b[i].name) return false;
        if (a[i].input_id != b[i].input_id) return false;
        if (a[i].output_id != b[i].output_id) return false;
        
        if (a[i].input != b[i].input) return false;
        if (a[i].output != b[i].output) return false;
    }
    
    return true;
}

// 打印通道配置
void printChannels(const std::vector<ChannelConfig>& channels) {
    std::cout << "\n当前通道配置 (" << channels.size() << " 个通道):\n";
    for (const auto& channel : channels) {
        std::cout << "  [" << channel.id << "] " << channel.name 
                  << " (输入: " << channel.input_id 
                  << ", 输出: " << channel.output_id << ")\n";
        
        if (channel.input.id > 0) {
            std::cout << "    输入端点: " << channel.input.type;
            if (!channel.input.ip.empty()) std::cout << ", IP: " << channel.input.ip;
            if (channel.input.port > 0) std::cout << ", 端口: " << channel.input.port;
            if (!channel.input.serial_port.empty()) std::cout << ", 串口: " << channel.input.serial_port;
            if (channel.input.baud_rate > 0) std::cout << ", 波特率: " << channel.input.baud_rate;
            std::cout << "\n";
        }
        
        if (channel.output.id > 0) {
            std::cout << "    输出端点: " << channel.output.type;
            if (!channel.output.ip.empty()) std::cout << ", IP: " << channel.output.ip;
            if (channel.output.port > 0) std::cout << ", 端口: " << channel.output.port;
            if (!channel.output.serial_port.empty()) std::cout << ", 串口: " << channel.output.serial_port;
            if (channel.output.baud_rate > 0) std::cout << ", 波特率: " << channel.output.baud_rate;
            std::cout << "\n";
        }
    }
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::string configFile;
    bool updateOnly = false;
    int pollInterval = 5; // 默认5秒轮询
    
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--update") == 0 && i + 1 < argc) {
            updateOnly = true;
            configFile = argv[++i];
        }
        else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            configFile = argv[++i];
        }
        else if (strcmp(argv[i], "--poll") == 0 && i + 1 < argc) {
            pollInterval = std::stoi(argv[++i]);
            if (pollInterval < 1) pollInterval = 1;
        }
        else if (strcmp(argv[i], "--help") == 0) {
            std::cout << "用法:\n"
                      << "  " << argv[0] << " --config <file.yml> [--poll <seconds>]\n"
                      << "  " << argv[0] << " --update <file.yml>\n"
                      << "选项:\n"
                      << "  --config <file>   指定配置文件并进入监控模式\n"
                      << "  --update <file>    更新数据库配置并退出\n"
                      << "  --poll <seconds>   设置轮询间隔(默认5秒)\n"
                      << "  --help             显示帮助信息\n";
            return 0;
        }
    }
    
    if (configFile.empty()) {
        std::cerr << "错误: 必须指定配置文件 (使用 --config 或 --update 参数)\n";
        return 1;
    }
    
    // 初始化数据库
    Database db("config.db");
    db.registerCallback(dbChangeCallback);
    
    // 如果使用 --update 参数，只更新数据库并退出
    if (updateOnly) {
        try {
            auto parser = ConfigParserFactory::createParser(configFile);
            std::vector<ChannelConfig> newChannels = parser->parse(configFile);
            
            std::cout << "从文件加载 " << newChannels.size() << " 个通道配置\n";
            db.replaceChannels(newChannels);
            std::cout << "数据库配置已更新\n";
        } catch (const std::exception& e) {
            std::cerr << "错误: " << e.what() << std::endl;
            return 1;
        }
        return 0;
    }
    
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 加载初始配置
    std::vector<ChannelConfig> lastChannels;
    try {
        lastChannels = db.loadChannels();
        {
            std::lock_guard<std::mutex> lock(configMutex);
            currentChannels = lastChannels;
        }
        std::cout << "初始配置加载成功: " << lastChannels.size() << " 个通道\n";
        printChannels(lastChannels);
    } catch (const std::exception& e) {
        std::cerr << "加载初始配置失败: " << e.what() << std::endl;
        return 1;
    }
    
    // 主循环 - 定时轮询数据库变化
    std::cout << "开始监控数据库变化 (轮询间隔: " << pollInterval << " 秒)\n";
    std::cout << "按 Ctrl+C 退出...\n";
    
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(pollInterval));
        
        try {
            auto newChannels = db.loadChannels();
            
            {
                std::lock_guard<std::mutex> lock(configMutex);
                if (!compareChannels(lastChannels, newChannels)) {
                    std::cout << "\n配置发生变化! (之前: " << lastChannels.size() 
                              << " 通道, 现在: " << newChannels.size() << " 通道)\n";
                    
                    printChannels(newChannels);
                    lastChannels = newChannels;
                    currentChannels = newChannels;
                } else {
                    // std::cout << "[" << std::chrono::system_clock::now() << "] 配置未发生变化" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "加载配置失败: " << e.what() << std::endl;
        }
    }
    
    std::cout << "程序正常退出" << std::endl;
    return 0;
}
 