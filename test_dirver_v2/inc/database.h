#pragma once
#include "shared_structs.h"
#include <vector>
#include <string>
#include <sqlite3.h>

class Database {
public:
    explicit Database(const std::string& db_path = "config.dbconfig.db");
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




class FourTeleDatabase {
public:
    FourTeleDatabase(const std::string& db_path);
    ~FourTeleDatabase();
    
    // 四遥数据访问
    bool getTeleSignal(uint16_t address, TeleSignalPoint& point);
    bool getTeleMeasure(uint16_t address, TeleMeasurePoint& point);
    bool getTeleControl(uint16_t address, TeleControlPoint& point);
    bool getTeleAdjust(uint16_t address, TeleAdjustPoint& point);
    
    // 四遥数据更新
    bool updateTeleSignal(uint16_t address, bool value);
    bool updateTeleMeasure(uint16_t address, double value);
    bool setTeleControl(uint16_t address, bool value);
    bool setTeleAdjust(uint16_t address, double value);
    
    // 执行待处理的遥控/遥调命令
    bool executePendingCommands();
    
private:
    sqlite3* db_;
    
    void initDatabase();
    void executeSQL(const std::string& sql);
};