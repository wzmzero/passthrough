#pragma once
#include <sqlite3.h>
#include <functional>
#include <vector>
#include <mutex>
#include "models.h"
#include <sqlite_orm/sqlite_orm.h>
 
// 数据库变更回调类型
using DbChangeCallback = std::function<void(
    const std::string& table, 
    int operation, 
    int rowid,
    const std::any& data
)>;

// 声明外部 schema 函数
inline auto makeStorageSchema(const std::string& filename) {
    return make_storage(
        filename,
        make_table("endpoints",
            make_column("id", &EndpointConfig::id, primary_key()),
            make_column("type", &EndpointConfig::type),
            make_column("port", &EndpointConfig::port),
            make_column("ip", &EndpointConfig::ip),
            make_column("serial_port", &EndpointConfig::serial_port),
            make_column("baud_rate", &EndpointConfig::baud_rate)
        ),
        make_table("channels",
            make_column("id", &ChannelConfig::id, primary_key()),
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
        ),
         // 添加 Master 表
        make_table("masters_telemetry",
            make_column("id", &MasterPoint::id, primary_key()),
            make_column("name", &MasterPoint::name),
            make_column("register_address", &MasterPoint::register_address),
            // make_column("data_type", &MasterPoint::data_type),
            // make_column("value_type", &MasterPoint::value_type),    
            // make_column("value", &MasterPoint::value),
            // make_column("timestamp", &MasterPoint::timestamp),
            make_column("unit", &MasterPoint::unit),
            make_column("request_flag", &MasterPoint::request_flag)  // 新增请求标志
        ),
        // // 添加 Slave 表
        make_table("slaves_telemetry",
            make_column("id", &SlavePoint::id, primary_key()),
            make_column("name", &SlavePoint::name),
            make_column("register_address", &SlavePoint::register_address),
            // make_column("data_type", &SlavePoint::data_type),
            // make_column("value_type", &SlavePoint::value_type),
            // make_column("value", &SlavePoint::value),
            // make_column("timestamp", &SlavePoint::timestamp),
            make_column("unit", &SlavePoint::unit)
        )

    );
}

class Database {
public:
    using StorageType = decltype(makeStorageSchema(""));

    Database(const std::string& filename= "config/config.db");
    void execute(const std::string& sql);
    void registerCallback(DbChangeCallback callback);
    auto& getStorage() { return storage; }
    
    // 通道操作函数
    std::vector<ChannelConfig> loadChannels();
    void replaceChannels(std::vector<ChannelConfig>& channels);

    // Master点操作函数
    std::vector<MasterPoint> loadMasters();
    std::vector<MasterPoint> getRequestedMasters(); // 获取需要请求的Master点
    void saveMasters(std::vector<MasterPoint>& masters);
    void writeMasterData(int64_t id, const MasterPoint& value);
    
    // Slave点操作函数
    std::vector<SlavePoint> loadSlaves();
    void saveSlaves(std::vector<SlavePoint>& slaves);
    void writeSlaveData(int64_t id, const SlavePoint& value);
    
    // 初始化模拟数据
    void initSampleData();
private:
    static StorageType createStorage(const std::string& filename);
    static void updateHook(void* self, int op, const char* dbName, const char* tableName, sqlite3_int64 rowid);

    void handleTableUpdate(const std::string& table, int rowid, std::any& data) {
        try {
            if (table == "endpoints") {
                data = storage.get<SlavePoint>(rowid);} 
            else if (table == "channels") {
                data = storage.get<ChannelConfig>(rowid);}
                
            } catch (...) { // 数据获取失败时不设置data
        }
    }
    StorageType storage;
    sqlite3* db = nullptr;
    std::vector<DbChangeCallback> callbacks;
    std::mutex callbackMutex;
};


