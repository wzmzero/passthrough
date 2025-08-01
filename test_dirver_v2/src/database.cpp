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




// database.cpp
#include "database.h"
#include <stdexcept>
#include <ctime>

FourTeleDatabase::FourTeleDatabase(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db_)));
    }
    initDatabase();
}

FourTeleDatabase::~FourTeleDatabase() {
    sqlite3_close(db_);
}

void FourTeleDatabase::initDatabase() {
    executeSQL(R"(
        PRAGMA journal_mode = WAL;
        PRAGMA synchronous = NORMAL;
        
        CREATE TABLE IF NOT EXISTS tele_signal (
            address INTEGER PRIMARY KEY,
            value BOOLEAN NOT NULL,
            timestamp INTEGER NOT NULL
        );
        
        CREATE TABLE IF NOT EXISTS tele_measure (
            address INTEGER PRIMARY KEY,
            value REAL NOT NULL,
            timestamp INTEGER NOT NULL
        );
        
        CREATE TABLE IF NOT EXISTS tele_control (
            address INTEGER PRIMARY KEY,
            value BOOLEAN NOT NULL,
            timestamp INTEGER NOT NULL,
            pending BOOLEAN NOT NULL DEFAULT 0
        );
        
        CREATE TABLE IF NOT EXISTS tele_adjust (
            address INTEGER PRIMARY KEY,
            value REAL NOT NULL,
            timestamp INTEGER NOT NULL,
            pending BOOLEAN NOT NULL DEFAULT 0
        );
    )");
}

void FourTeleDatabase::executeSQL(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error = "SQL error: " + std::string(errMsg);
        sqlite3_free(errMsg);
        throw std::runtime_error(error);
    }
}

bool FourTeleDatabase::getTeleSignal(uint16_t address, TeleSignalPoint& point) {
    const char* sql = "SELECT value, timestamp FROM tele_signal WHERE address = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) 
        return false;
    
    sqlite3_bind_int(stmt, 1, address);
    bool result = false;
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        point.address = address;
        point.value = sqlite3_column_int(stmt, 0) != 0;
        point.timestamp = sqlite3_column_int64(stmt, 1);
        result = true;
    }
    
    sqlite3_finalize(stmt);
    return result;
}

bool FourTeleDatabase::updateTeleSignal(uint16_t address, bool value) {
    const char* sql = R"(
        INSERT OR REPLACE INTO tele_signal (address, value, timestamp)
        VALUES (?, ?, ?);
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) 
        return false;
    
    sqlite3_bind_int(stmt, 1, address);
    sqlite3_bind_int(stmt, 2, value ? 1 : 0);
    sqlite3_bind_int64(stmt, 3, std::time(nullptr));
    
    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

// 类似实现其他get/update方法...

bool FourTeleDatabase::executePendingCommands() {
    bool success = true;
    
    // 执行遥控命令
    const char* yk_sql = "SELECT address, value FROM tele_control WHERE pending = 1;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, yk_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            uint16_t addr = sqlite3_column_int(stmt, 0);
            bool value = sqlite3_column_int(stmt, 1) != 0;
            
            // 在实际系统中这里会执行硬件控制
            // 执行成功后更新状态
            const char* update_sql = "UPDATE tele_control SET pending = 0 WHERE address = ?;";
            sqlite3_stmt* update_stmt;
            
            if (sqlite3_prepare_v2(db_, update_sql, -1, &update_stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(update_stmt, 1, addr);
                if (sqlite3_step(update_stmt) != SQLITE_DONE) {
                    success = false;
                }
                sqlite3_finalize(update_stmt);
            }
        }
        sqlite3_finalize(stmt);
    }
    
    // 类似执行遥调命令...
    
    return success;
}