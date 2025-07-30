#pragma once
#include "shared_structs.h"
#include <vector>
#include <string>
#include <sqlite3.h>

class Database {
public:
    explicit Database(const std::string& db_path = "config.db");
    ~Database();
    
    // 加载所有通道配置
    std::vector<ChannelConfig> loadChannels();
    
    // 保存通道配置到数据库
    void saveChannels(const std::vector<ChannelConfig>& channels);
    
    // 清空并替换所有配置
    void replaceChannels(const std::vector<ChannelConfig>& channels);

private:
    sqlite3* db_;
    
    void initDatabase();
    void executeSQL(const std::string& sql);
    void insertEndpoint(sqlite3_stmt* stmt, sqlite3_int64 channelId, 
                       const std::string& role, const EndpointConfig& config);
};