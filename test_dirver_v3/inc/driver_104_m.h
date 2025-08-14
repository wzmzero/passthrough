#pragma once
#include "driver_base.h"
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <ctime>

// 104M专用数据结构
namespace IEC104 {

// 类型标识 (ASDU Type)
enum TypeIdentify : uint8_t {
    M_SP_NA = 1,    // 单点信息
    M_ME_NA = 9,    // 测量值（归一化）
    C_SC_NA = 45,   // 单点遥控命令
    C_IC_NA = 100   // 总召唤命令
};

// 传输原因
enum TransCause : uint8_t {
    SPONT = 3,      // 自发
    ACT = 6,        // 激活
    ACTCON = 7      // 激活确认
};

// 控制域类型
enum CtrlFormat {
    I_FORMAT = 0,   // I帧
    S_FORMAT = 1,   // S帧
    U_FORMAT = 3    // U帧
};

// U帧功能
enum UFunction : uint8_t {
    START_ACT = 0x07,   // 启动激活
    START_CON = 0x0B,   // 启动确认
    TEST_ACT = 0x43,    // 测试激活
    TEST_CON = 0x83     // 测试确认
};

// 信息对象地址结构
struct InfoObjectAddr {
    uint8_t bytes[3]; // 3字节地址
    operator uint32_t() const {
        return (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];
    }
};

// ASDU 结构
struct ASDU {
    TypeIdentify type;       // 类型标识
    bool isSequence;         // 是否连续地址
    uint8_t numElements;     // 元素数量
    TransCause cause;        // 传输原因
    uint16_t commonAddr;     // 公共地址
    std::vector<uint32_t> ioAddrs; // 信息对象地址
    std::vector<float> values;    // 数据值
};

// APCI 结构
struct APCI {
    CtrlFormat format;
    union {
        struct { // I格式
            uint16_t sendSeq;
            uint16_t recvSeq;
        };
        struct { // S格式
            uint16_t ackSeq;
        };
        struct { // U格式
            UFunction function;
        };
    };
};

// 完整APDU结构
struct APDU {
    APCI control;
    ASDU asdu;
};

} // namespace IEC104

class Driver104M : public DriverBase {
public:
    explicit Driver104M(uint16_t commonAddr = 1);
    ~Driver104M() override;

    // 核心接口
    bool open() override;
    void close() override;
    size_t write(const uint8_t* data, size_t len) override;
    
    // 104M特有接口
    bool sendStartActivation();
    bool sendGeneralCall();
    bool sendCommand(uint32_t ioAddr, bool value);
    void setDataCallback(std::function<void(uint32_t addr, float value)> callback);

private:
    void processThread();
    bool parseAPDU(const uint8_t* data, size_t len, IEC104::APDU& apdu);
    std::vector<uint8_t> buildAPDU(const IEC104::APDU& apdu);
    void handleIFrame(const IEC104::APDU& apdu);
    void handleSFrame(const IEC104::APDU& apdu);
    void handleUFrame(const IEC104::APDU& apdu);
    void sendSFrame();
    
    // 状态管理
    void resetLink();
    void updateT1Timer();
    void checkTimeouts();

    // 104M参数
    uint16_t commonAddr_;            // 公共地址
    uint16_t t1_timeout_ = 15;       // T1超时(秒)
    uint16_t t2_timeout_ = 10;       // T2超时(秒)
    uint16_t t3_timeout_ = 20;       // T3超时(秒)
    uint8_t k_factor_ = 12;          // 未确认I帧最大数
    uint8_t w_factor_ = 8;           // 接收窗口大小

    // 链路状态
    std::atomic<bool> running_{false};
    std::atomic<bool> linkActive_{false};
    std::atomic<bool> startDtConfirmed_{false};
    std::thread workerThread_;
    
    // 序列号管理
    uint16_t sendSeq_ = 0;
    uint16_t recvSeq_ = 0;
    uint16_t ackSeq_ = 0;
    
    // 时间管理
    time_t lastT1Time_ = 0;
    time_t lastT2Time_ = 0;
    time_t lastT3Time_ = 0;
    bool t1Active_ = false;
    
    // 数据队列
    std::queue<IEC104::APDU> sendQueue_;
    std::mutex queueMutex_;
    std::queue<std::vector<uint8_t>> recvQueue_;
    
    // 回调函数
    std::function<void(uint32_t addr, float value)> dataCallback_;
    
    // 重发管理
    std::map<uint16_t, IEC104::APDU> unackIFrames_; // 未确认的I帧
};