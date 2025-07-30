#include "ConfigReader.hpp"
#include <iostream>
#include <cstdlib>
#include <map>
#include <sstream> // 新增
#include <nlohmann/json.hpp>
using json = nlohmann::json;
ConfigReader::ConfigReader(const std::string& db_path) : db_path_(db_path) {}

ConfigReader::~ConfigReader() {
    closeDatabase();
}

bool ConfigReader::openDatabase() {
    if (db_) return true; // 已经打开
    
    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
     // 创建表结构
    const char* createTables = 
        "CREATE TABLE IF NOT EXISTS endpoints ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    type TEXT NOT NULL,"
        "    name TEXT NOT NULL,"
        "    config TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS channels ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name TEXT NOT NULL,"
        "    input_id INTEGER NOT NULL,"
        "    output_id INTEGER NOT NULL,"
        "    FOREIGN KEY(input_id) REFERENCES endpoints(id),"
        "    FOREIGN KEY(output_id) REFERENCES endpoints(id)"
        ");";
    
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, createTables, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}
// 加载所有通道配置
std::vector<ChannelConfig> ConfigReader::loadChannels() {
    std::vector<ChannelConfig> channels;
    if (!db_) return channels;
    
    const char* query = 
        "SELECT c.id, c.name, "
        "       i.id AS input_id, i.type AS input_type, i.name AS input_name, i.config AS input_config, "
        "       o.id AS output_id, o.type AS output_type, o.name AS output_name, o.config AS output_config "
        "FROM channels c "
        "JOIN endpoints i ON c.input_id = i.id "
        "JOIN endpoints o ON c.output_id = o.id";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
        return channels;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ChannelConfig channel;
        channel.id = sqlite3_column_int(stmt, 0);
        channel.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        
        // 输入端点
        channel.input.id = sqlite3_column_int(stmt, 2);
        channel.input.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        channel.input.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        channel.input.config_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        
        // 输出端点
        channel.output.id = sqlite3_column_int(stmt, 6);
        channel.output.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        channel.output.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        channel.output.config_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        
        channels.push_back(channel);
    }
    
    sqlite3_finalize(stmt);
    return channels;
}
void ConfigReader::closeDatabase() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool ConfigReader::executeQuery(const std::string& sql, 
                               int (*callback)(void*, int, char**, char**), 
                               void* data) {
    if (!openDatabase()) return false;
    
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), callback, data, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

std::string ConfigReader::protocolToString(Protocol protocol) {
    switch (protocol) {
        case TCP: return "tcp";
        case UDP: return "udp";
        case SERIAL: return "serial";
        default: return "tcp";
    }
}

// 客户端配置回调函数
static int clientConfigCallback(void* data, int argc, char** argv, char** colName) {
    auto* config = static_cast<ConfigReader::ClientConfig*>(data);
    if (argc >= 4) {
        config->server_ip = argv[0] ? argv[0] : "";
        config->server_port = argv[1] ? std::atoi(argv[1]) : 0;
        config->serial_port = argv[2] ? argv[2] : "";
        config->baud_rate = argv[3] ? std::atoi(argv[3]) : 115200;
    }
    return 0;
}

bool ConfigReader::readClientConfig(Protocol protocol, ClientConfig& config) {
    std::string protocolStr = protocolToString(protocol);
    std::string sql = "SELECT server_ip, server_port, serial_port, baud_rate "
                     "FROM client_config WHERE protocol = '" + 
                     protocolStr + "' LIMIT 1;";
    
    return executeQuery(sql, clientConfigCallback, &config);
}

// 服务器配置回调函数
static int serverConfigCallback(void* data, int argc, char** argv, char** colName) {
    auto* config = static_cast<ConfigReader::ServerConfig*>(data);
    if (argc >= 1 && argv[0]) {
        config->listen_port = std::atoi(argv[0]);
    }
    return 0;
}

bool ConfigReader::readServerConfig(Protocol protocol, ServerConfig& config) {
    std::string protocolStr = protocolToString(protocol);
    std::string sql = "SELECT listen_port FROM server_config WHERE protocol = '" + 
                     protocolStr + "' LIMIT 1;";
    
    return executeQuery(sql, serverConfigCallback, &config);
}

// 新增：执行更新操作（INSERT/UPDATE）
bool ConfigReader::executeUpdate(const std::string& sql) {
    if (!openDatabase()) return false;
    
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// 新增：写入服务器配置
bool ConfigReader::writeServerConfig(Protocol protocol, const ServerConfig& config) {
    std::string protocolStr = protocolToString(protocol);
    
    // 创建表（如果不存在）
    std::string createTableSql = "CREATE TABLE IF NOT EXISTS server_config ("
                                "protocol TEXT PRIMARY KEY,"
                                "listen_port INTEGER NOT NULL);";
    if (!executeUpdate(createTableSql)) {
        return false;
    }
    
    // 插入或更新配置
    std::ostringstream sql;
    sql << "INSERT OR REPLACE INTO server_config (protocol, listen_port) "
        << "VALUES ('" << protocolStr << "', " << config.listen_port << ");";
    
    return executeUpdate(sql.str());
}

// 新增：写入客户端配置
bool ConfigReader::writeClientConfig(Protocol protocol, const ClientConfig& config) {
    std::string protocolStr = protocolToString(protocol);
    
    // 创建表（如果不存在）
    std::string createTableSql = "CREATE TABLE IF NOT EXISTS _config ("
                                "protocol TEXT PRIMARY KEY,"
                                "server_ip TEXT,"
                                "server_port INTEGER,"
                                "serial_port TEXT,"
                                "baud_rate INTEGER);";
    if (!executeUpdate(createTableSql)) {
        return false;
    }
    
    // 插入或更新配置
    std::ostringstream sql;

        sql << "INSERT OR REPLACE INTO client_config (protocol, server_ip, server_port, serial_port, baud_rate) "
            << "VALUES ('" << protocolStr << "', '" << config.server_ip << "', "
            << config.server_port <<    ", '" << config.serial_port << "', " << config.baud_rate << ");";
 


  
    return executeUpdate(sql.str());
}
