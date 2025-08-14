// driver_modbus_m.h (建议替换或按需合并)
#pragma once
#include "driver_base.h"
#include "common.h"

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <chrono>

static std::string hexStr(const std::vector<uint8_t>& v) {
    std::ostringstream oss;
    for (auto b : v) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
    }
    return oss.str();
}
// ---------------- CRC16 (Modbus) ----------------
static uint16_t crc16_modbus(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)data[pos];
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

class DriverModbusM : public DriverBase {
public:
    explicit DriverModbusM(VecDevInfo &v_devinfo, ModbusTransportMode mode = ModbusTransportMode::TCP);
    ~DriverModbusM();
    
    // 核心接口
    bool open() override;  //启动线程和资源
    void close() override;   // 停止线程和资源释放
    size_t write(const uint8_t* data, size_t len) override;    // 实现写接口,写入内部的recvqueue_，供给parseMessage使用
    std::queue<std::vector<uint8_t>> sendqueue_; // 发送队列,外部可以调用出队
    std::mutex sendMutex_; // keep sendMutex_ public for demo access if needed

private:
    // 工作线程函数
    void workThread();    // 工作线程，循环解析报文和发送命令 
    
    // 解析报文 recvQueue_队列中多个Frame，单个Frame由parseFrame解析成四遥点向量。目前打印解析结果
    void parseMessage(); 
    // 解帧函数
    int parseFrame(const std::vector<uint8_t>& frame, VecTelemPoint& v_parsetelem);  // 处理recvqueue_中的一帧数据,解析到四遥点向量
    // 发送命令
    void sendMessage();  // 获取配置好的v_devinfo_转换得到v_sendtelem,再将其中每一个sendtlem由MakeFrame组帧得到一帧报文，在存储到sendQueue_

    std::vector<uint8_t>& MakeFrame(TelemPoint &sendtlem);  // 组帧函数,将四遥点向量转换成Modbus帧

    std::queue<std::vector<uint8_t>> recvqueue_; // 接收队列
    std::mutex recvMutex_;                       // 接收队列互斥锁
    std::condition_variable cv_;                 // notify worker
    VecDevInfo v_devinfo_ ;  // 设备信息向量   // 暂定使用模拟的
    VecTelemPoint v_parsetelem_;    // 四遥点向量
    VecTelemPoint v_sendtelem_; // 二次解析数据向量

    // runtime members
    ModbusTransportMode mode_;
    std::thread worker_thread_;
    std::atomic<bool> running_;
    std::vector<uint8_t> temp_frame_buffer_;
    uint16_t transaction_id_;
};
