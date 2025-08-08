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
            make_column("data_type", &MasterPoint::data_type),
            make_column("value_type", &MasterPoint::value_type),    
            make_column("value", &MasterPoint::value),
            make_column("timestamp", &MasterPoint::timestamp),
            make_column("unit", &MasterPoint::unit),
            make_column("rw_flag", &MasterPoint::rw_flag),
            make_column("return_flag", &MasterPoint::return_flag)
        ),
        // 添加 TelemetryLink 表
        make_table("telemetry_links",
           make_column("id", &TelemetryPoint::id, primary_key()),
            make_column("name", &TelemetryPoint::name),
            make_column("register_address", &TelemetryPoint::register_address),
            make_column("data_type", &TelemetryPoint::data_type),
            make_column("value_type", &TelemetryPoint::value_type),    
            make_column("value", &TelemetryPoint::value),
            make_column("timestamp", &TelemetryPoint::timestamp),
            make_column("unit", &TelemetryPoint::unit),
            make_column("rw_flag", &TelemetryPoint::rw_flag),
            make_column("return_flag", &TelemetryPoint::return_flag)
        ),
        // // 添加 Slave 表
        make_table("slaves_telemetry",
            make_column("id", &SlavePoint::id, primary_key()),
            make_column("name", &SlavePoint::name),
            make_column("register_address", &SlavePoint::register_address),
            make_column("data_type", &SlavePoint::data_type),
            make_column("value_type", &SlavePoint::value_type),
            make_column("value", &SlavePoint::value),
            make_column("timestamp", &SlavePoint::timestamp),
            make_column("unit", &SlavePoint::unit),
            make_column("rw_flag", &SlavePoint::rw_flag),
            make_column("return_flag", &SlavePoint::return_flag)
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

// 通用写数据函数
    template <typename T>
    void writeData(uint32_t register_address, TelemDataType data_type, const std::any& value) {
        using namespace sqlite_orm;
        storage.transaction([&] {
            auto points = storage.template get_all<T>(where(
                c(&T::data_type) == data_type
            ));
            
            if (!points.empty()) {
                auto& point = points[0];
                point.value = value;
                point.timestamp = std::time(nullptr);
                storage.template update(point);
            }
            return true;
        });
    }

    // 通用读数据函数
    template <typename T>
    std::optional<T> readData(uint32_t register_address, TelemDataType data_type) {
        using namespace sqlite_orm;
        auto points = storage.template get_all<T>(where(
            c(&T::register_address) == register_address &&
            c(&T::data_type) == data_type
        ));
        
        if (!points.empty()) {
            return points[0];
        }
        return std::nullopt;
    }

    // 设置请求标志函数 (仅适用于MasterPoint和TelemetryPoint)
    template <typename T>
    void setRequestFlag(uint32_t register_address, TelemDataType data_type, int rw_flag) {
        static_assert(std::is_same_v<T, MasterPoint> || std::is_same_v<T, TelemetryPoint>, 
                      "setRequestFlag only supports MasterPoint and TelemetryPoint");
        using namespace sqlite_orm;
        storage.transaction([&] {
            auto points = storage.template get_all<T>(where(
                c(&T::register_address) == register_address &&
                c(&T::data_type) == data_type
            ));
            
            if (!points.empty()) {
                auto point = points[0];
                point.rw_flag = rw_flag;
                storage.template update(point);
            }
            return true;
        });
    }
    // 设置响应标志函数 (仅适用于MasterPoint和TelemetryPoint)
    template <typename T>
    void setResponseFlag(uint32_t register_address, TelemDataType data_type, int return_flag) {
        static_assert(std::is_same_v<T, MasterPoint> || std::is_same_v<T, TelemetryPoint>, 
                      "setResponseFlag only supports MasterPoint and TelemetryPoint");
        
        using namespace sqlite_orm;
        storage.transaction([&] {
            auto points = storage.template get_all<T>(where(
                c(&T::register_address) == register_address &&
                c(&T::data_type) == data_type
            ));
            
            if (!points.empty()) {
                auto point = points[0];
                point.return_flag = return_flag;
                storage.template update(point);
            }
            return true;
        });
    }

    // 重置标志函数 (仅适用于MasterPoint和TelemetryPoint)
    template <typename T>
    void resetFlag(uint32_t register_address, TelemDataType data_type) {
        static_assert(std::is_same_v<T, MasterPoint> || std::is_same_v<T, TelemetryPoint>, 
                      "resetFlag only supports MasterPoint and TelemetryPoint");
        
        using namespace sqlite_orm;
        storage.transaction([&] {
            auto points = storage.template get_all<T>(where(
                c(&T::register_address) == register_address &&
                c(&T::data_type) == data_type
            ));
            
            if (!points.empty()) {
                auto point = points[0];
                point.rw_flag = 0;
                point.return_flag = 0;
                storage.template update(point);
            }
            return true;
        });
    }

    // 初始化模拟数据
    void initSampleData();
    template <typename T>
    void initPoints(const std::vector<TelemetryPoint>& points) {
        using namespace sqlite_orm;
        storage.transaction([&] {
            // 清除现有数据
            storage.template remove_all<T>();
            
            // 转换并插入数据
            for (const auto& point : points) {
                storage.template insert(point.as<T>());
            }
            return true;
        });
    }
private:
    static StorageType createStorage(const std::string& filename);
    static void updateHook(void* self, int op, const char* dbName, const char* tableName, sqlite3_int64 rowid);

    void handleTableUpdate(const std::string& table, int rowid, std::any& data) {
        try {
            if (table == "slaves_telemetry") {
                data = storage.get<SlavePoint>(rowid);
            } else if (table == "masters_telemetry") {
                data = storage.get<MasterPoint>(rowid);
            } else if (table == "telemetry_points") {
                data = storage.get<TelemetryPoint>(rowid);
            } else if (table == "channels") {
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
