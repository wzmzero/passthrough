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
        // 遥测数据点表
        make_table("telem_points",
            make_column("id", &TelemPoint::id, primary_key().autoincrement()),
            make_column("name", &TelemPoint::name),
            make_column("register_address", &TelemPoint::register_address),
            make_column("data_type", &TelemPoint::data_type),
            make_column("value_type", &TelemPoint::value_type),
            make_column("value", &TelemPoint::value),
            make_column("timestamp", &TelemPoint::timestamp),
            make_column("unit", &TelemPoint::unit),
            make_column("request_flag", &TelemPoint::request_flag)
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
    
    // 通道操作函数
    std::vector<ChannelConfig> loadChannels();
    void saveChannels(std::vector<ChannelConfig>& channels);
    void replaceChannels(std::vector<ChannelConfig>& channels);

private:
    static StorageType createStorage(const std::string& filename);
    static void updateHook(void* self, int op, const char* dbName, const char* tableName, sqlite3_int64 rowid);
    
    template <typename T>
    void handleTableUpdate(const std::string& table, int rowid, std::any& data) {
        try {
            if (table == "endpoints") {
                data = storage.get<EndpointConfig>(rowid);
            } else if (table == "channels") {
                data = storage.get<ChannelConfig>(rowid);
            }
        } catch (...) {
            // 数据获取失败时不设置data
        }
    }

    StorageType storage;
    sqlite3* db = nullptr;
    std::vector<DbChangeCallback> callbacks;
    std::mutex callbackMutex;
};

// ========== TelemDataType 转换函数 ==========
inline std::string TelemDataTypeToString(TelemDataType type) {
    switch (type) {
        case TelemDataType::YX: return "YX";
        case TelemDataType::YC: return "YC";
        case TelemDataType::YK: return "YK";
        case TelemDataType::YT: return "YT";
    }
    throw std::domain_error("Invalid TelemDataType");
}

inline TelemDataType TelemDataTypeFromString(const std::string& str) {
    if (str == "YX") return TelemDataType::YX;
    if (str == "YC") return TelemDataType::YC;
    if (str == "YK") return TelemDataType::YK;
    if (str == "YT") return TelemDataType::YT;
    throw std::runtime_error("Invalid TelemDataType string: " + str);
}

// ========== ValueType 转换函数 ==========
inline std::string ValueTypeToString(ValueType type) {
    switch (type) {
        case ValueType::Boolean: return "Boolean";
        case ValueType::Integer: return "Integer";
        case ValueType::Float:   return "Float";
    }
    throw std::domain_error("Invalid ValueType");
}

inline ValueType ValueTypeFromString(const std::string& str) {
    if (str == "Boolean") return ValueType::Boolean;
    if (str == "Integer") return ValueType::Integer;
    if (str == "Float")   return ValueType::Float;
    throw std::runtime_error("Invalid ValueType string: " + str);
}

// ========== 整合所有 sqlite_orm 特化到单个命名空间 ==========
namespace sqlite_orm {
    // ========== 特化 for TelemDataType ==========
    template<>
    struct type_printer<TelemDataType> : public text_printer {};
    
    template<>
    struct statement_binder<TelemDataType> {
        int bind(sqlite3_stmt* stmt, int index, const TelemDataType& value) {
            return statement_binder<std::string>().bind(stmt, index, TelemDataTypeToString(value));
        }
    };
    
    template<>
    struct field_printer<TelemDataType> {
        std::string operator()(const TelemDataType& value) const {
            return TelemDataTypeToString(value);
        }
    };
    
    template<>
    struct row_extractor<TelemDataType> {
        TelemDataType extract(const char* columnText) const {
            return TelemDataTypeFromString(columnText);
        }

        TelemDataType extract(sqlite3_stmt* stmt, int columnIndex) const {
            auto str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, columnIndex));
            return this->extract(str);
        }
    };

    // ========== 特化 for ValueType ==========
    template<>
    struct type_printer<ValueType> : public text_printer {};
    
    template<>
    struct statement_binder<ValueType> {
        int bind(sqlite3_stmt* stmt, int index, const ValueType& value) {
            return statement_binder<std::string>().bind(stmt, index, ValueTypeToString(value));
        }
    };
    
    template<>
    struct field_printer<ValueType> {
        std::string operator()(const ValueType& value) const {
            return ValueTypeToString(value);
        }
    };
    
    template<>
    struct row_extractor<ValueType> {
        ValueType extract(const char* columnText) const {
            return ValueTypeFromString(columnText);
        }

        ValueType extract(sqlite3_stmt* stmt, int columnIndex) const {
            auto str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, columnIndex));
            return this->extract(str);
        }
    };

    // ========== 特化 for std::any ==========
    template<>
    struct row_extractor<std::any> {
        std::any extract(sqlite3_stmt* stmt, int columnIndex) const {
            const int type = sqlite3_column_type(stmt, columnIndex);
            switch (type) {
                case SQLITE_NULL:
                    return std::any{};
                case SQLITE_INTEGER:
                    return sqlite3_column_int(stmt, columnIndex);
                case SQLITE_FLOAT:
                    return sqlite3_column_double(stmt, columnIndex);
                case SQLITE_TEXT: {
                    const unsigned char* text = sqlite3_column_text(stmt, columnIndex);
                    return std::string(reinterpret_cast<const char*>(text));
                }
                case SQLITE_BLOB: {
                    const void* blob = sqlite3_column_blob(stmt, columnIndex);
                    const int size = sqlite3_column_bytes(stmt, columnIndex);
                    return std::vector<char>(reinterpret_cast<const char*>(blob),
                                             reinterpret_cast<const char*>(blob) + size);
                }
                default:
                    throw std::runtime_error("Unsupported SQLite column type for std::any");
            }
        }

        std::any extract(sqlite3_value* value) const {
            const int type = sqlite3_value_type(value);
            switch (type) {
                case SQLITE_NULL:
                    return std::any{};
                case SQLITE_INTEGER:
                    return sqlite3_value_int(value);
                case SQLITE_FLOAT:
                    return sqlite3_value_double(value);
                case SQLITE_TEXT: {
                    const unsigned char* text = sqlite3_value_text(value);
                    return std::string(reinterpret_cast<const char*>(text));
                }
                case SQLITE_BLOB: {
                    const void* blob = sqlite3_value_blob(value);
                    const int size = sqlite3_value_bytes(value);
                    return std::vector<char>(reinterpret_cast<const char*>(blob),
                                             reinterpret_cast<const char*>(blob) + size);
                }
                default:
                    throw std::runtime_error("Unsupported SQLite value type for std::any");
            }
        }
    };

    template<>
    struct statement_binder<std::any> {
        int bind(sqlite3_stmt* stmt, int index, const std::any& value) const {
            if (!value.has_value()) {
                return sqlite3_bind_null(stmt, index);
            }

            if (value.type() == typeid(int)) {
                return sqlite3_bind_int(stmt, index, std::any_cast<int>(value));
            } else if (value.type() == typeid(double)) {
                return sqlite3_bind_double(stmt, index, std::any_cast<double>(value));
            } else if (value.type() == typeid(std::string)) {
                const auto& text = std::any_cast<std::string>(value);
                return sqlite3_bind_text(stmt, index, text.c_str(), static_cast<int>(text.size()), SQLITE_TRANSIENT);
            } else if (value.type() == typeid(std::vector<char>)) {
                const auto& blob = std::any_cast<std::vector<char>>(value);
                return sqlite3_bind_blob(stmt, index, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);
            }

            throw std::runtime_error("Unsupported type in std::any statement_binder");
        }
    };

    template<>
    struct type_printer<std::any> : public text_printer {};

    template<>
    struct field_printer<std::any> {
        std::string operator()(const std::any& value) const {
            if (!value.has_value()) {
                return "NULL";
            }

            if (value.type() == typeid(int)) {
                return std::to_string(std::any_cast<int>(value));
            } else if (value.type() == typeid(double)) {
                return std::to_string(std::any_cast<double>(value));
            } else if (value.type() == typeid(std::string)) {
                return std::any_cast<std::string>(value);
            } else if (value.type() == typeid(std::vector<char>)) {
                const auto& blob = std::any_cast<std::vector<char>>(value);
                std::ostringstream oss;
                oss << "0x";
                for (unsigned char c: blob) {
                    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
                }
                return oss.str();
            }

            throw std::runtime_error("Unsupported type in std::any field_printer");
        }
    };
} 