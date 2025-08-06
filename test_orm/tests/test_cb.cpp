#include "database.h"
#include <iostream>
#include <string>

// 打印变更信息
void printChange(const std::string& table, int op, int rowid, const std::any& data) {
    std::cout << "\n===== 数据库变更 =====" << std::endl;
    std::cout << "表: " << table << ", 操作: ";
    
    switch(op) {
        case SQLITE_INSERT: 
            std::cout << "INSERT (新增)";
            break;
        case SQLITE_UPDATE: 
            std::cout << "UPDATE (修改)";
            break;
        case SQLITE_DELETE: 
            std::cout << "DELETE (删除)";
            break;
        default:
            std::cout << "未知操作(" << op << ")";
    }
    
    std::cout << ", 行ID: " << rowid << std::endl;
    
    if (data.has_value()) {
        try {
            if (table == "endpoints") {
                auto ep = std::any_cast<EndpointConfig>(data);
                std::cout << "端点详情: "
                          << "ID=" << ep.id
                          << ", 类型=" << ep.type
                          << ", IP=" << ep.ip
                          << ", 端口=" << ep.port
                          << ", 串口=" << ep.serial_port
                          << ", 波特率=" << ep.baud_rate << std::endl;
            }
            else if (table == "channels") {
                auto ch = std::any_cast<ChannelConfig>(data);
                std::cout << "通道详情: "
                          << "ID=" << ch.id
                          << ", 名称=" << ch.name
                          << ", 输入ID=" << ch.input_id
                          << ", 输出ID=" << ch.output_id << std::endl;
            }
        } catch(const std::bad_any_cast&) {
            std::cout << "数据详情: 无法解析" << std::endl;
        }
    } else {
        std::cout << "数据详情: 无" << std::endl;
    }
    
    std::cout << "=====================\n" << std::endl;
}

int main() {
    // 创建数据库实例
    Database db("database_monitor.db");
    
    // 注册变更回调
    db.registerCallback(printChange);
    
    // 主循环
    std::cout << "数据库监控系统已启动 (输入 'exit' 退出)" << std::endl;
    std::cout << "支持的SQL命令: INSERT, UPDATE, DELETE, SELECT" << std::endl;
    
    while(true) {
        std::cout << "\nSQL> ";
        std::string input;
        std::getline(std::cin, input);
        
        if(input == "exit" || input == "quit") {
            break;
        }
        
        if(input.empty()) continue;
        
        // 执行SQL命令
        db.execute(input);
    }
    
    std::cout << "监控系统已关闭" << std::endl;
    return 0;
}