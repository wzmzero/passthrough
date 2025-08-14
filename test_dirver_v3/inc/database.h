#pragma once
#include <functional>
#include <vector>
#include <mutex>
#include "models.h"
#include "common.h"
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
        // Instance实例表
        make_table("instance",
            make_column("id", &InstanceParm::id, primary_key()),
            make_column("name", &InstanceParm::name),
            make_column("instance_type", &InstanceParm::type)
        ),
        // Channel表
        make_table("channel",
            make_column("id", &EndpointConfig::id, primary_key()),
            make_column("type", &EndpointConfig::type),
            make_column("port", &EndpointConfig::port),
            make_column("ip", &EndpointConfig::ip),
            make_column("serial_port", &EndpointConfig::serial_port),
            make_column("baud_rate", &EndpointConfig::baud_rate),
            make_column("instance_id", &EndpointConfig::instance_id),
            foreign_key(&EndpointConfig::instance_id)
                .references(&InstanceParm::id)
                .on_update.cascade()
        ),
        // 透传表
        make_table("passthrough",
            make_column("id", &ChannelConfig::id, primary_key()),
            make_column("name", &ChannelConfig::name),
            make_column("input_id", &ChannelConfig::input_id),
            make_column("output_id", &ChannelConfig::output_id),
            foreign_key(&ChannelConfig::input_id)
                .references(&EndpointConfig::id)
                .on_update.cascade(),
            foreign_key(&ChannelConfig::output_id)
                .references(&EndpointConfig::id)
                .on_update.cascade()
        ),
        // Driver协议表
        make_table("driver",
            make_column("id", &DriverParam_Mid::id, primary_key()),
            make_column("proto_type", &DriverParam_Mid::proto_type),
            make_column("param_name", &DriverParam_Mid::param_name),   //其他扩展的配置参数
            make_column("param_value", &DriverParam_Mid::param_value), 
            make_column("desc", &DriverParam_Mid::desc),
            make_column("instance_id", &DriverParam_Mid::instance_id), 
            foreign_key(&DriverParam_Mid::instance_id)
                .references(&InstanceParm::id)
                .on_update.cascade()
        ),
        //DevInfo表
        make_table("devInfo",
            make_column("dataId", &DevInfo::dataId),
            make_column("description", &DevInfo::description),
            make_column("slave_addr", &DevInfo::slave_addr),
            make_column("proAddr", &DevInfo::proAddr),
            make_column("data_type", &DevInfo::data_type),
            make_column("value_type", &DevInfo::value_type),    
            make_column("value", &DevInfo::value),
            make_column("unit", &DevInfo::unit),
            make_column("instance_id", &DevInfo::instance_id),
            foreign_key(&DevInfo::instance_id)
                .references(&InstanceParm::id)
                .on_update.cascade()
        ),
        //Data数据表
        make_table("dataset",
            make_column("dataId", &Dataset::dataId),
            make_column("name", &Dataset::name),
            make_column("data_type", &Dataset::data_type),
            make_column("value_type", &Dataset::value_type),    
            make_column("value", &Dataset::value),
            make_column("timestamp", &Dataset::timestamp),
            make_column("unit", &Dataset::unit)
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
 
    // 实例操作函数
    std::vector<InstanceParm> loadInstances();
    void replaceInstances(std::vector<InstanceParm>& instances);
    
    void initSampleData();

private:
    static StorageType createStorage(const std::string& filename);
    static void updateHook(void* self, int op, const char* dbName, const char* tableName, sqlite3_int64 rowid);

    // 转换函数：将DriverParam_Mid转换为DriverParam
    DriverParam convertToDriverParam(const std::vector<DriverParam_Mid>& params);
    // 转换函数：将DriverParam转换为std::vector<DriverParam_Mid>
    std::vector<DriverParam_Mid> convertFromDriverParam(const DriverParam& driverParam, int instanceId);
    
    void handleTableUpdate(const std::string& table, int rowid, std::any& data) {
        try {
            if (table == "slaves_telemetry") {
                data = storage.get<ChannelConfig>(rowid);
            }
        } catch (...) { // 数据获取失败时不设置data
        }
    }
    StorageType storage;
    sqlite3* db = nullptr;
    std::vector<DbChangeCallback> callbacks;
    std::mutex callbackMutex;
};
