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
    db->handleTableUpdate<EndpointConfig>(table, rid, data);

    std::lock_guard<std::mutex> lock(db->callbackMutex);
    for (const auto& cb : db->callbacks) {
        cb(table, op, rid, data);
    }
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

// 保存通道配置
void Database::saveChannels(std::vector<ChannelConfig>& channels) {
    storage.transaction([&] {
        for (auto& channel : channels) {
            // 保存输入端点
            if (channel.input.id == 0) {
                channel.input.id = storage.insert(channel.input);
            } else {
                storage.update(channel.input);
            }
            channel.input_id = channel.input.id;
            
            // 保存输出端点
            if (channel.output.id == 0) {
                channel.output.id = storage.insert(channel.output);
            } else {
                storage.update(channel.output);
            }
            channel.output_id = channel.output.id;
            
            // 保存通道配置
            if (channel.id == 0) {
                channel.id = storage.insert(channel);
            } else {
                storage.update(channel);
            }
        }
        return true;
    });
}

// 端点比较函数（忽略ID）
bool endpointEquals(const EndpointConfig& a, const EndpointConfig& b) {
    return a.type == b.type &&
           a.port == b.port &&
           a.ip == b.ip &&
           a.serial_port == b.serial_port &&
           a.baud_rate == b.baud_rate;
}

// 替换所有通道配置（原子操作）
void Database::replaceChannels(std::vector<ChannelConfig>& channels) {
    storage.transaction([&] {
        // 删除所有现有配置
        storage.remove_all<ChannelConfig>();
        storage.remove_all<EndpointConfig>();
        
        // 端点映射（内容到ID）
        std::vector<std::pair<EndpointConfig, int>> endpointList;
        
        // 处理端点
        for (auto& channel : channels) {
            // 处理输入端点
            bool found = false;
            for (auto& [ep, id] : endpointList) {
                if (endpointEquals(ep, channel.input)) {
                    channel.input_id = id;
                    channel.input.id = id;
                    found = true;
                    break;
                }
            }
            if (!found) {
                channel.input.id = 0; // 确保插入新记录
                auto newId = storage.insert(channel.input);
                endpointList.push_back({channel.input, newId});
                channel.input_id = newId;
                channel.input.id = newId;
            }
            
            // 处理输出端点
            found = false;
            for (auto& [ep, id] : endpointList) {
                if (endpointEquals(ep, channel.output)) {
                    channel.output_id = id;
                    channel.output.id = id;
                    found = true;
                    break;
                }
            }
            if (!found) {
                channel.output.id = 0; // 确保插入新记录
                auto newId = storage.insert(channel.output);
                endpointList.push_back({channel.output, newId});
                channel.output_id = newId;
                channel.output.id = newId;
            }
            
            // 插入通道
            channel.id = storage.insert(channel);
        }
        
        return true;
    });
}