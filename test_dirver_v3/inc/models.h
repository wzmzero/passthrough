#pragma once
#include <string>
#include <memory>
#include <variant>
#include <ctime>
#include <any>
#include <stdexcept>
#include <vector>
#include <map>
#include <array>
#include <algorithm>
#include "common.h"

enum CommInsType {
	Ins_Acquire = 1,	//采集实例
	Ins_Transmit		//转发实例
};

struct DriverParam_Mid {
    int id;                       // 主键
    ProtoType proto_type;         // 协议类型
    std::string desc;
    std::string param_name;     
    std::any   param_value;
    int instance_id;
};
typedef std::vector<DriverParam_Mid> VecDriverParam_Mid; // 设备信息向量

struct InstanceParm {
    int id;
    std::string name;   // RTU/DAS
    CommInsType type;    //采集/转发
	DriverParam driverParam;	//驱动参数
    EndpointConfig channelParam;	//通道参数
    VecDevInfo vecDevInfo;		//设备信息
};

// 通道配置结构体
struct ChannelConfig {
    int id = 0;  // 主键
    std::string name;
    int input_id = 0;  // 外键 -> EndpointConfig.id
    int output_id = 0; // 外键 -> EndpointConfig.id
    // 嵌套对象（程序内使用）
    EndpointConfig input;  
    EndpointConfig output; 
    // 添加比较运算符
    bool operator==(const ChannelConfig& other) const {
        return name == other.name &&
               input == other.input &&
               output == other.output;
    }
    
    bool operator!=(const ChannelConfig& other) const {
        return !(*this == other);
    }
};

#include <sqlite_orm/sqlite_orm.h>
using namespace sqlite_orm;

// ========== 前置声明 ==========
namespace sqlite_orm {
    template <typename T>
    struct enum_traits;  // 主模板声明

    // ========== 通用枚举处理模板 ==========
    template <typename T>
    inline std::string enum_to_string(T value) {
        static_assert(std::is_enum_v<T>, "T must be an enum type");
        return enum_traits<T>::toString(value);
    }

    template <typename T>
    inline T enum_from_string(const std::string& str) {
        static_assert(std::is_enum_v<T>, "T must be an enum type");
        return enum_traits<T>::fromString(str);
    }

    // ========== 通用枚举处理基类 ==========
    template <typename T, typename Mapping>
    struct enum_traits_base {
        using value_type = T;
        
        static inline std::string toString(value_type value) {
            constexpr auto& mappings = Mapping::value;
            auto it = std::find_if(std::begin(mappings), std::end(mappings), 
                [value](const auto& pair) { return pair.first == value; });
            if (it != std::end(mappings)) return it->second;
            throw std::domain_error("Invalid enum value");
        }
        
        static inline value_type fromString(const std::string& str) {
            constexpr auto& mappings = Mapping::value;
            auto it = std::find_if(std::begin(mappings), std::end(mappings), 
                [&str](const auto& pair) { return pair.second == str; });
            if (it != std::end(mappings)) return it->first;
            throw std::runtime_error("Invalid enum string: " + str);
        }
    };

    // ========== sqlite_orm 枚举支持 ==========
    template <typename T>
    struct type_printer<T, std::enable_if_t<std::is_enum_v<T>>> : public text_printer {};

    template <typename T>
    struct statement_binder<T, std::enable_if_t<std::is_enum_v<T>>> {
        int bind(sqlite3_stmt* stmt, int index, const T& value) {
            return statement_binder<std::string>().bind(
                stmt, index, enum_to_string(value)
            );
        }
    };

    template <typename T>
    struct field_printer<T, std::enable_if_t<std::is_enum_v<T>>> {
        std::string operator()(const T& value) const {
            return enum_to_string(value);
        }
    };

    template <typename T>
    struct row_extractor<T, std::enable_if_t<std::is_enum_v<T>>> {
        T extract(const char* columnText) const {
            return enum_from_string<T>(columnText);
        }

        T extract(sqlite3_stmt* stmt, int columnIndex) const {
            const char* text = reinterpret_cast<const char*>(
                sqlite3_column_text(stmt, columnIndex)
            );
            return this->extract(text);
        }
    };
} // namespace sqlite_orm

// ========== 映射关系声明 ==========
namespace enum_mappings {

// Data_Type 映射
struct Data_Type_Mapping {
    static constexpr std::array<std::pair<Data_Type, const char*>, 4> value = {{
        {Data_Type::Data_YX, "YX"},
        {Data_Type::Data_YC, "YC"},
        {Data_Type::Data_YK, "YK"},
        {Data_Type::Data_YT, "YT"}
    }};
};

// ValueType 映射
struct ValueType_Mapping {
    static constexpr std::array<std::pair<ValueType, const char*>, 3> value = {{
        {ValueType::Boolean, "Boolean"},
        {ValueType::Integer, "Integer"},
        {ValueType::Float, "Float"}
    }};
};

struct ProtoType_Mapping {
    static constexpr std::array<std::pair<ProtoType, const char*>, 6> value = {{
        {ProtoType::MODBUS_M, "MODBUS_M"},
        {ProtoType::MODBUS_S, "MODBUS_S"},
        {ProtoType::IEC101_M, "IEC101_M"},
        {ProtoType::IEC101_S, "IEC101_S"},
        {ProtoType::IEC104_M, "IEC104_M"},
        {ProtoType::IEC104_S, "IEC104_S"},
    }};
};
// CommInsType 映射
struct CommInsType_Mapping {
    static constexpr std::array<std::pair<CommInsType, const char*>, 2> value = {{
        {CommInsType::Ins_Acquire, "采集实例"},
        {CommInsType::Ins_Transmit, "转发实例"}
    }};
};
} // namespace enum_mappings

// ========== 特化实现 ==========
namespace sqlite_orm {

// Data_Type 特化
template <>
struct enum_traits<Data_Type> 
    : enum_traits_base<Data_Type, enum_mappings::Data_Type_Mapping> {};

// ValueType 特化
template <>
struct enum_traits<ValueType> 
    : enum_traits_base<ValueType, enum_mappings::ValueType_Mapping> {};
// ProtoType 特化
template <>
struct enum_traits<ProtoType> 
    : enum_traits_base<ProtoType, enum_mappings::ProtoType_Mapping> {};
// CommInsType 特化
template <>
struct enum_traits<CommInsType> 
    : enum_traits_base<CommInsType, enum_mappings::CommInsType_Mapping> {};

} // namespace sqlite_orm

// ========== 整合所有 sqlite_orm 特化 ==========
namespace sqlite_orm {
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
} // namespace sqlite_orm