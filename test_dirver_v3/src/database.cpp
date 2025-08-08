#include "database.h"
#include <iostream>
#include <algorithm>


// 创建存储的具体实现
Database::StorageType Database::createStorage(const std::string& filename) {
    return makeStorageSchema(filename);
}

// 构造函数
Database::Database(const std::string& filename)
    : storage(createStorage(filename))  // 初始化存储
{
    storage.sync_schema();

    // 设置数据库连接
    storage.on_open = [this](sqlite3* db_) {
        db = db_;
        sqlite3_update_hook(db, &Database::updateHook, this);
    };
    storage.open_forever();
}

// 执行SQL语句
void Database::execute(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL 执行错误: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}

// 注册回调函数
void Database::registerCallback(DbChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    callbacks.push_back(std::move(callback));
}

// 更新钩子函数
void Database::updateHook(void* self, int op, const char* dbName, const char* tableName, sqlite3_int64 rowid) {
    Database* db = static_cast<Database*>(self);
    const std::string table(tableName);
    const int rid = static_cast<int>(rowid);

    std::any data;
    db->handleTableUpdate(table, rid, data);

    std::lock_guard<std::mutex> lock(db->callbackMutex);
    for (const auto& cb : db->callbacks) {
        cb(table, op, rid, data);
    }
}

  

// 初始化模拟数据
void Database::initSampleData() {
     auto now = std::time(nullptr);
    
    // 统一的数据源
    std::vector<TelemetryPoint> allPoints = {

        {0, "断路器控制", 0x2001, TelemDataType::YK, ValueType::Boolean, 
         false, now, "", 0, 0},
        {0, "目标温度", 0x3001, TelemDataType::YT, ValueType::Float, 
         25.0f, now, "°C", 0, 0},

        {0, "温度反馈", 0x5001, TelemDataType::YC, ValueType::Float, 
         24.5f, now, "°C", 0, 0},
        {0, "压力反馈", 0x5002, TelemDataType::YC, ValueType::Float, 
         1.2f, now, "MPa", 0, 0},

        {0, "电压1", 0x4001, TelemDataType::YC, ValueType::Float, 
         220.5f, now, "V", 0, 0},
        {0, "电流1", 0x4003, TelemDataType::YC, ValueType::Float, 
         15.3f, now, "A", 0, 0}
    };
    
    // 使用模板初始化不同类型的点
    initPoints<MasterPoint>(allPoints);
    initPoints<SlavePoint>(allPoints);
    initPoints<TelemetryPoint>(allPoints);
}


// 加载所有通道配置（包含端点信息）
std::vector<ChannelConfig> Database::loadChannels() {
    std::vector<ChannelConfig> channels;
    storage.transaction([&] {
        // 加载所有通道
        channels = storage.get_all<ChannelConfig>();
        
        // 为每个通道加载端点信息
        for (auto& channel : channels) {
            if (channel.input_id > 0) {
                if (auto input = storage.get_pointer<EndpointConfig>(channel.input_id)) {
                    channel.input = *input;
                }
            }
            if (channel.output_id > 0) {
                if (auto output = storage.get_pointer<EndpointConfig>(channel.output_id)) {
                    channel.output = *output;
                }
            }
        }
        return true;
    });
    
    return channels;
}


// 简化版替换所有通道配置
void Database::replaceChannels(std::vector<ChannelConfig>& channels) {
    storage.transaction([&] {
        // 删除所有现有配置
        storage.remove_all<ChannelConfig>();
        storage.remove_all<EndpointConfig>();
        
        // 直接插入新配置
        for (auto& channel : channels) {
            // 插入端点
            channel.input.id = storage.insert(channel.input);
            channel.input_id = channel.input.id;
            
            channel.output.id = storage.insert(channel.output);
            channel.output_id = channel.output.id;
            
            // 插入通道
            channel.id = storage.insert(channel);
        }
        
        return true;
    });
}