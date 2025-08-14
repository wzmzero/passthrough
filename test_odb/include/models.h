#pragma once
#include <string>
#include <memory>
#include <ctime>
#include <cstdint>  // 添加 int64_t 支持
#include <odb/core.hxx> // ODB 核心头文件

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

#pragma db object polymorphic
struct TelemPoint {
    // 添加虚析构函数以满足多态要求
    virtual ~TelemPoint() = default;
    
    #pragma db id auto
    int64_t id = 0;                         
    
    #pragma db not_null
    std::string name;                       // 名称
    
    #pragma db not_null
    std::string register_address;           // 寄存器地址
    
    #pragma db not_null
    TelemDataType data_type;                // 四遥类型
    
    #pragma db not_null
    ValueType value_type;                   // 值类型
    
    // 替换 variant 为独立字段 + 类型标记
    #pragma db not_null
    bool bool_value = false;
    
    #pragma db not_null
    int32_t int_value = 0;
    
    #pragma db not_null
    float float_value = 0.0f;
    
    #pragma db not_null
    std::time_t timestamp;                  // 时间戳
    
    #pragma db null
    std::string unit;                       // 单位
    
    #pragma db not_null
    int request_flag = 0;                   // 请求标志
    
    // 获取实际值的辅助函数
    template <typename T>
    T get_value() const {
        switch (value_type) {
            case ValueType::Boolean: return static_cast<T>(bool_value);
            case ValueType::Integer: return static_cast<T>(int_value);
            case ValueType::Float:   return static_cast<T>(float_value);
            default: return T{};
        }
    }
    
    // 设置实际值的辅助函数
    void set_value(bool v)        { 
        bool_value = v; 
        value_type = ValueType::Boolean; 
    }
    
    void set_value(int32_t v)     { 
        int_value = v;  
        value_type = ValueType::Integer; 
    }
    
    void set_value(float v)       { 
        float_value = v; 
        value_type = ValueType::Float; 
    }
};

#pragma db object
struct MasterPoint: public TelemPoint {
    #pragma db not_null
    int rw_flag = 0;              // 读写标志
    
    #pragma db not_null
    int return_flag = 0;           // 返回状态标志
};

#pragma db object
struct SlavePoint {
    #pragma db id auto
    int64_t id = 0;
    
    // 使用指针关联而不是包含，避免多态对象直接包含
    #pragma db not_null
    #pragma db column("base_id")
    std::shared_ptr<TelemPoint> base;  // 指向基类对象的指针
    
    #pragma db transient           // 不存储到数据库
    void* additional_data = nullptr; // 替换 any 为指针
};

// 枚举类型映射
#pragma db value(TelemDataType) type("INT")
#pragma db value(ValueType) type("INT")

// 定义对象关系
#pragma db object(TelemPoint) session
#pragma db object(MasterPoint) session
#pragma db object(SlavePoint) session