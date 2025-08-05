#include "database.h"
#include <stdexcept>
#include <iostream>
#include <cstdint>
Database::Database(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db_)));
    }
    initDatabase();
}

Database::~Database() {
    sqlite3_close(db_);
}

void Database::initDatabase() {
    executeSQL(R"(
        PRAGMA encoding = 'UTF-8';
        PRAGMA foreign_keys = ON;
        
        CREATE TABLE IF NOT EXISTS channels (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL UNIQUE
        );
        
        CREATE TABLE IF NOT EXISTS endpoints (
            id INTEGER PRIMARY KEY,
            channel_id INTEGER NOT NULL,
            role TEXT NOT NULL CHECK(role IN ('input', 'output')),
            type TEXT NOT NULL,
            port INTEGER,
            ip TEXT,
            serial_port TEXT,
            baud_rate INTEGER,
            FOREIGN KEY(channel_id) REFERENCES channels(id) ON DELETE CASCADE
        );
    )");
}

void Database::executeSQL(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error = "SQL error: " + std::string(errMsg);
        sqlite3_free(errMsg);
        throw std::runtime_error(error);
    }
}

std::vector<ChannelConfig> Database::loadChannels() {
    const char* sql = R"(
        SELECT c.name, 
               i.type AS input_type, i.port AS input_port, i.ip AS input_ip,
               i.serial_port AS input_serial_port, i.baud_rate AS input_baud,
               
               o.type AS output_type, o.port AS output_port, o.ip AS output_ip,
               o.serial_port AS output_serial_port, o.baud_rate AS output_baud
               
        FROM channels c
        JOIN endpoints i ON c.id = i.channel_id AND i.role = 'input'
        JOIN endpoints o ON c.id = o.channel_id AND o.role = 'output'
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    
    std::vector<ChannelConfig> channels;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ChannelConfig config;
        config.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        
        // 输入端点配置
        config.input.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) 
            config.input.port = sqlite3_column_int(stmt, 2);
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) 
            config.input.ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) 
            config.input.serial_port = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) 
            config.input.baud_rate = sqlite3_column_int(stmt, 5);
        
        // 输出端点配置
        config.output.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) 
            config.output.port = sqlite3_column_int(stmt, 7);
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) 
            config.output.ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) 
            config.output.serial_port = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) 
            config.output.baud_rate = sqlite3_column_int(stmt, 10);
        
        channels.push_back(config);
    }
    
    sqlite3_finalize(stmt);
    return channels;
}

void Database::saveChannels(const std::vector<ChannelConfig>& channels) {
    // 移除了事务开始和提交/回滚的代码
    // 准备插入通道的语句
    sqlite3_stmt* channelStmt;
    const char* channelSql = "INSERT INTO channels (name) VALUES (?);";
    if (sqlite3_prepare_v2(db_, channelSql, -1, &channelStmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    
    // 准备插入端点的语句
    sqlite3_stmt* endpointStmt;
    const char* endpointSql = R"(
        INSERT INTO endpoints 
        (channel_id, role, type, port, ip, serial_port, baud_rate)
        VALUES (?, ?, ?, ?, ?, ?, ?);
    )";
    if (sqlite3_prepare_v2(db_, endpointSql, -1, &endpointStmt, nullptr) != SQLITE_OK) {
        sqlite3_finalize(channelStmt);
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    
    for (const auto& channel : channels) {
        // 插入通道
        sqlite3_bind_text(channelStmt, 1, channel.name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(channelStmt) != SQLITE_DONE) {
            throw std::runtime_error("Failed to insert channel: " + channel.name);
        }
        sqlite3_int64 channelId = sqlite3_last_insert_rowid(db_);
        sqlite3_reset(channelStmt);
        
        // 插入输入端点
        insertEndpoint(endpointStmt, channelId, "input", channel.input);
        
        // 插入输出端点
        insertEndpoint(endpointStmt, channelId, "output", channel.output);
    }
    
    sqlite3_finalize(channelStmt);
    sqlite3_finalize(endpointStmt);
}

void Database::insertEndpoint(sqlite3_stmt* stmt, sqlite3_int64 channelId, 
                             const std::string& role, const EndpointConfig& config) {
    sqlite3_bind_int64(stmt, 1, channelId);
    sqlite3_bind_text(stmt, 2, role.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, config.type.c_str(), -1, SQLITE_TRANSIENT);
    
    // 绑定端口
    if (config.port > 0) {
        sqlite3_bind_int(stmt, 4, config.port);
    } else {
        sqlite3_bind_null(stmt, 4);
    }
    
    // 绑定IP
    if (!config.ip.empty()) {
        sqlite3_bind_text(stmt, 5, config.ip.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 5);
    }
    
    // 绑定串口
    if (!config.serial_port.empty()) {
        sqlite3_bind_text(stmt, 6, config.serial_port.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 6);
    }
    
    // 绑定波特率
    if (config.baud_rate > 0) {
        sqlite3_bind_int(stmt, 7, config.baud_rate);
    } else {
        sqlite3_bind_null(stmt, 7);
    }
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("Failed to insert endpoint: " + config.type);
    }
    
    sqlite3_reset(stmt);
}

void Database::replaceChannels(const std::vector<ChannelConfig>& channels) {
    executeSQL("BEGIN TRANSACTION;");
    try {
        executeSQL("DELETE FROM endpoints;");
        executeSQL("DELETE FROM channels;");
        saveChannels(channels);  // 现在在同一个事务中
        executeSQL("COMMIT;");
    } catch (...) {
        executeSQL("ROLLBACK;");
        throw;
    }
}