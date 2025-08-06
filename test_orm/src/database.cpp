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
        sqlite3_update_hook(db, &Database::staticUpdateHook, this);
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

// 静态钩子函数
void Database::staticUpdateHook(void* self, int op, const char* dbName, const char* tableName, sqlite3_int64 rowid) {
    static_cast<Database*>(self)->handleUpdate(op, tableName, rowid);
}

// 处理更新
void Database::handleUpdate(int op, const char* tableName, sqlite3_int64 rowid) {
    const std::string table(tableName);
    const int rid = static_cast<int>(rowid);

    std::any data;
    try {
        if (table == "endpoints") {
            data = storage.get<EndpointConfig>(rid);
        } else if (table == "channels") {
            data = storage.get<ChannelConfig>(rid);
        }
    } catch (...) {
        // 数据获取失败
    }

    std::lock_guard<std::mutex> lock(callbackMutex);
    for (const auto& cb : callbacks) {
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
                auto input = storage.get_pointer<EndpointConfig>(channel.input_id);
                if (input) {
                    channel.input = std::make_shared<EndpointConfig>(*input);
                }
            }
            if (channel.output_id > 0) {
                auto output = storage.get_pointer<EndpointConfig>(channel.output_id);
                if (output) {
                    channel.output = std::make_shared<EndpointConfig>(*output);
                }
            }
        }
        return true;
    });
    
    return channels;
}

// 保存通道配置
void Database::saveChannels(const std::vector<ChannelConfig>& channels) {
    storage.transaction([&] {
        for (const auto& channel : channels) {
            // 保存端点配置
            if (channel.input) {
                storage.replace(*channel.input);
            }
            if (channel.output) {
                storage.replace(*channel.output);
            }
            
            // 保存通道配置
            storage.replace(channel);
        }
        return true;
    });
}

// 替换所有通道配置（原子操作）
void Database::replaceChannels(std::vector<ChannelConfig>& channels) {
    // 端点比较函数（忽略ID）
    auto endpointComparator = [](const EndpointConfig& a, const EndpointConfig& b) {
        return a.type == b.type &&
               a.port == b.port &&
               a.ip == b.ip &&
               a.serial_port == b.serial_port &&
               a.baud_rate == b.baud_rate;
    };
    
    storage.transaction([&] {
        // 删除所有现有配置
        storage.remove_all<ChannelConfig>();
        storage.remove_all<EndpointConfig>();
        
        // 端点映射（内容到ID）
        std::map<EndpointConfig, int, decltype(endpointComparator)> endpointMap(endpointComparator);
        
        // 处理输入端点
        for (auto& channel : channels) {
            if (channel.input) {
                auto it = endpointMap.find(*channel.input);
                if (it == endpointMap.end()) {
                    // 插入新端点
                    auto id = storage.insert(*channel.input);
                    endpointMap[*channel.input] = id;
                    channel.input->id = id;
                } else {
                    channel.input->id = it->second;
                }
                channel.input_id = channel.input->id;
            }
            
            // 处理输出端点
            if (channel.output) {
                auto it = endpointMap.find(*channel.output);
                if (it == endpointMap.end()) {
                    // 插入新端点
                    auto id = storage.insert(*channel.output);
                    endpointMap[*channel.output] = id;
                    channel.output->id = id;
                } else {
                    channel.output->id = it->second;
                }
                channel.output_id = channel.output->id;
            }
            
            // 插入通道
            channel.id = storage.insert(channel);
        }
        
        return true;
    });
}