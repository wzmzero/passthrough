// common.h
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <ctime>
#include <functional>
#include <unordered_map>
#include <variant>
 
// 传输模式枚举
enum class ModbusTransportMode {
    RTU,
    TCP
};

// 功能码枚举
enum class ModbusFunctionCode : uint8_t {
    // 读功能码 (0x01-0x04)
    READ_COILS = 0x01,
    READ_DISCRETE_INPUTS = 0x02,
    READ_HOLDING_REGISTERS = 0x03,
    READ_INPUT_REGISTERS = 0x04,
    
    // 写功能码 (0x05-0x10)
    WRITE_SINGLE_COIL = 0x05,
    WRITE_SINGLE_REGISTER = 0x06,
    WRITE_MULTIPLE_COILS = 0x0F,
    WRITE_MULTIPLE_REGISTERS = 0x10,
    
    // 自定义功能码
    SHGK_WRITE = 0x13
};

// 数据点信息
struct ModbusDataPoint {
    uint16_t address;     // 寄存器地址
    uint16_t value;       // 寄存器值
};

// 报文信息结构
struct ModbusFrameInfo {
    ModbusFunctionCode functionCode;
    bool isException = false;
    uint8_t exceptionCode = 0;
    std::vector<ModbusDataPoint> dataPoints;
    
    // TCP模式专用
    uint16_t transactionId = 0;
    uint16_t bytesFollowing = 0;
};

// ========== 四遥数据模型 ==========
// 四遥数据类型
enum class TelemetryType {
    YX,     // 遥信（离散输入）
    YC,     // 遥测（输入寄存器） 0/1
    YK,     // 遥控（线圈）
    YT      // 遥调（保持寄存器）
};

// 遥测点值（用于YC/YX）
struct TelemetryPoint {
    uint16_t address;     // 点地址
    double value;         // 测量值（浮点表示）
    uint8_t quality;      // 数据质量（0-100%）
    time_t timestamp;     // 时间戳
};

// 遥控/遥调命令（用于YK/YT）
struct ControlCommand {
    uint16_t address;     // 点地址
    union {
        bool boolValue;   // 用于YK（遥控）
        double numValue;  // 用于YT（遥调）
    };
    time_t timestamp;     // 时间戳
};

// 四遥数据库接口
class ITelemetryDatabase {
public:
    virtual ~ITelemetryDatabase() = default;
    
    // 读取遥测数据
    virtual bool readYC(uint16_t address, TelemetryPoint& point) = 0;
    virtual bool readMultipleYC(uint16_t start, uint16_t count, std::vector<TelemetryPoint>& points) = 0;
    
    // 读取遥信数据
    virtual bool readYX(uint16_t address, TelemetryPoint& point) = 0;
    virtual bool readMultipleYX(uint16_t start, uint16_t count, std::vector<TelemetryPoint>& points) = 0;
    
    // 写入遥控命令
    virtual bool writeYK(uint16_t address, bool value) = 0;
    
    // 写入遥调命令
    virtual bool writeYT(uint16_t address, double value) = 0;
    
    // 注册数据变化回调
    virtual void registerDataChangeCallback(std::function<void(TelemetryType, uint16_t)> callback) = 0;
    
    // 更新遥测值（用于模拟）
    virtual void updateYCValue(uint16_t address, double value) = 0;
    
    // 更新遥信值（用于模拟）
    virtual void updateYXValue(uint16_t address, bool value) = 0;
};

// 简易内存数据库实现
class SimpleMemoryDatabase : public ITelemetryDatabase {
public:
    SimpleMemoryDatabase() {
        // 初始化默认值
        for (uint16_t i = 0; i < 100; i++) {
            yc_data_[i] = {i, 0.0, 100, 0};
            yx_data_[i] = {i, 0.0, 100, 0};
            yk_data_[i] = {i, {false}, 0};
            yt_data_[i] = {i, {.numValue = 0.0}, 0};
        }
    }
    
    bool readYC(uint16_t address, TelemetryPoint& point) override {
        auto it = yc_data_.find(address);
        if (it != yc_data_.end()) {
            point = it->second;
            return true;
        }
        return false;
    }
    
    bool readMultipleYC(uint16_t start, uint16_t count, std::vector<TelemetryPoint>& points) override {
        points.clear();
        for (uint16_t i = 0; i < count; i++) {
            uint16_t addr = start + i;
            auto it = yc_data_.find(addr);
            if (it != yc_data_.end()) {
                points.push_back(it->second);
            } else {
                return false;
            }
        }
        return true;
    }
    
    bool readYX(uint16_t address, TelemetryPoint& point) override {
        auto it = yx_data_.find(address);
        if (it != yx_data_.end()) {
            point = it->second;
            return true;
        }
        return false;
    }
    
    bool readMultipleYX(uint16_t start, uint16_t count, std::vector<TelemetryPoint>& points) override {
        points.clear();
        for (uint16_t i = 0; i < count; i++) {
            uint16_t addr = start + i;
            auto it = yx_data_.find(addr);
            if (it != yx_data_.end()) {
                points.push_back(it->second);
            } else {
                return false;
            }
        }
        return true;
    }
    
    bool writeYK(uint16_t address, bool value) override {
        auto it = yk_data_.find(address);
        if (it != yk_data_.end()) {
            it->second.boolValue = value;
            it->second.timestamp = time(nullptr);
            notifyChange(TelemetryType::YK, address);
            return true;
        }
        return false;
    }
    
    bool writeYT(uint16_t address, double value) override {
        auto it = yt_data_.find(address);
        if (it != yt_data_.end()) {
            it->second.numValue = value;
            it->second.timestamp = time(nullptr);
            notifyChange(TelemetryType::YT, address);
            return true;
        }
        return false;
    }
    
    void registerDataChangeCallback(std::function<void(TelemetryType, uint16_t)> callback) override {
        change_callback_ = callback;
    }
    
    // 模拟数据更新
    void updateYCValue(uint16_t address, double value) override {
        auto it = yc_data_.find(address);
        if (it != yc_data_.end()) {
            it->second.value = value;
            it->second.timestamp = time(nullptr);
            notifyChange(TelemetryType::YC, address);
        }
    }
    
    void updateYXValue(uint16_t address, bool value) override {
        auto it = yx_data_.find(address);
        if (it != yx_data_.end()) {
            it->second.value = value ? 1.0 : 0.0;
            it->second.timestamp = time(nullptr);
            notifyChange(TelemetryType::YX, address);
        }
    }

private:
    void notifyChange(TelemetryType type, uint16_t address) {
        if (change_callback_) {
            change_callback_(type, address);
        }
    }
    
    std::unordered_map<uint16_t, TelemetryPoint> yc_data_;   // 遥测数据
    std::unordered_map<uint16_t, TelemetryPoint> yx_data_;   // 遥信数据
    std::unordered_map<uint16_t, ControlCommand> yk_data_;   // 遥控数据
    std::unordered_map<uint16_t, ControlCommand> yt_data_;   // 遥调数据
    
    std::function<void(TelemetryType, uint16_t)> change_callback_;
};