#pragma once
#include <sqlite_orm/sqlite_orm.h>
#include <sqlite3.h>
#include <functional>
#include <vector>
#include <mutex>
#include <any>
#include <map>
#include <set>
#include "models.h"

// 数据库变更回调类型
using DbChangeCallback = std::function<void(
    const std::string& table, 
    int operation, 
    int rowid,
    const std::any& data
)>;

// 声明外部 schema 函数
inline auto makeStorageSchema(const std::string& filename) {
    using namespace sqlite_orm;
    return make_storage(
        filename,
        make_table("endpoints",
            make_column("id", &EndpointConfig::id, primary_key().autoincrement()),
            make_column("type", &EndpointConfig::type),
            make_column("port", &EndpointConfig::port),
            make_column("ip", &EndpointConfig::ip),
            make_column("serial_port", &EndpointConfig::serial_port),
            make_column("baud_rate", &EndpointConfig::baud_rate)
        ),
        make_table("channels",
            make_column("id", &ChannelConfig::id, primary_key().autoincrement()),
            make_column("name", &ChannelConfig::name),
            make_column("input_id", &ChannelConfig::input_id),
            make_column("output_id", &ChannelConfig::output_id),
            foreign_key(&ChannelConfig::input_id)
                .references(&EndpointConfig::id)
                .on_delete.cascade()
                .on_update.cascade(),
            foreign_key(&ChannelConfig::output_id)
                .references(&EndpointConfig::id)
                .on_delete.cascade()
                .on_update.cascade()
        )
    );
}

class Database {
public:
    using StorageType = decltype(makeStorageSchema(""));

    Database(const std::string& filename);
    void execute(const std::string& sql);
    void registerCallback(DbChangeCallback callback);
    auto& getStorage() { return storage; }
    
    // 新添加的通道操作函数
    std::vector<ChannelConfig> loadChannels();
    void saveChannels(const std::vector<ChannelConfig>& channels);
    void replaceChannels(std::vector<ChannelConfig>& channels);

private:
    static StorageType createStorage(const std::string& filename);
    static void staticUpdateHook(void* self, int op, const char* dbName, const char* tableName, sqlite3_int64 rowid);
    void handleUpdate(int op, const char* tableName, sqlite3_int64 rowid);

    StorageType storage;
    sqlite3* db = nullptr;
    std::vector<DbChangeCallback> callbacks;
    std::mutex callbackMutex;
};