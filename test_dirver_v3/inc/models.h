#pragma once
#include <string>
#include <memory>
#include <variant>
#include <ctime>
#include <any>
#include <stdexcept>
// 四遥数据类型枚举
enum class TelemDataType {
    YX,     // 遥测     0001H-4001H
    YC,     // 遥信     4001H-5000H
    YK,     // 遥控     6001H-6100H
    YT      // 遥调      
};

// 值类型枚举
enum class ValueType {
    Boolean,      // 布尔值 (0/1)
    Integer,      // 整数
    Float         // 浮点数
};

// 四遥数据点结构体
struct TelemPoint {
    int id = 0;                         // 主键ID
    std::string name;                       // 名称 (e.g., "voltage", "coil_switch")
    uint32_t register_address;          // 寄存器地址 (e.g., "0x4001")
    // TelemDataType data_type;                // 四遥类型
    // ValueType value_type;                   // 值类型
    // std::any value;                         // 存储实际值
    // std::time_t timestamp;                  // 时间戳
    std::string unit;                       // 单位 (e.g., "V", "A", "m/s")
};
// Master点结构体
struct MasterPoint  {   
        int id = 0;                         // 主键ID
    std::string name;                       // 名称 (e.g., "voltage", "coil_switch")
    uint32_t register_address;          // 寄存器地址 (e.g., "0x4001")
    // TelemDataType data_type;                // 四遥类型
    // ValueType value_type;                   // 值类型
    // std::any value;                         // 存储实际值
    // std::time_t timestamp;                  // 时间戳
    std::string unit;   
    bool request_flag = false;   // 请求标志 (0-未请求, 1-请求中, 2-已请求)  
};
struct SlavePoint  {
    int id = 0;                         // 主键ID
    std::string name;                       // 名称 (e.g., "voltage", "coil_switch")
    uint32_t register_address;          // 寄存器地址 (e.g., "0x4001")
    // TelemDataType data_type;                // 四遥类型
    // ValueType value_type;                   // 值类型
    // std::any value;                         // 存储实际值
    // std::time_t timestamp;                  // 时间戳
    std::string unit;   
};

// 端点配置结构体
struct EndpointConfig {
    int id = 0;  // 主键
    std::string type;
    uint16_t port = 0;
    std::string ip;
    std::string serial_port;
    uint32_t baud_rate = 0;
    // 添加比较运算符
    bool operator==(const EndpointConfig& other) const {
        return type == other.type &&
               port == other.port &&
               ip == other.ip &&
               serial_port == other.serial_port &&
               baud_rate == other.baud_rate;
    }
    bool operator!=(const EndpointConfig& other) const {
        return !(*this == other);
    }
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
    // ==========     int id = 0;                         // 主键ID
    std::string name;                       // 名称 (e.g., "voltage", "coil_switch")
    uint32_t register_address;          // 寄存器地址 (e.g., "0x4001")
    // TelemDataType data_type;                // 四遥类型
    // ValueType value_type;                   // 值类型
    // std::any value;                         // 存储实际值
    // std::time_t timestamp;                  // 时间戳
    std::string unit;    for TelemDataType ==========
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