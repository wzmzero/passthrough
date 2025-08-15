// driver_modbus_m.h
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


static std::string hexStr(const std::vector<uint8_t>& v);

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
    explicit DriverModbusM(VecDevInfo &v_devinfo, MModbusParam param);
    ~DriverModbusM();
    
    // 核心接口
    bool open() override;
    void close() override;
    size_t write(const uint8_t* data, size_t len) override;
    
    // 添加访问方法用于测试
    const VecTelemPoint& getParsedPoints() const { return v_parsetelem_; }
    std::queue<std::vector<uint8_t>> sendqueue_; 
    std::mutex sendMutex_;

private:
    void workThread();
    void parseMessage(); 
    int parseFrame(const std::vector<uint8_t>& frame, VecTelemPoint& v_parsetelem);
    void sendMessage();
    std::vector<uint8_t> MakeFrame(TelemPoint &sendtlem);
    std::queue<std::vector<uint8_t>> recvqueue_;
    std::mutex recvMutex_;

    VecDevInfo v_devinfo_;
    VecTelemPoint v_parsetelem_;
    VecTelemPoint v_sendtelem_;
 
    std::thread worker_thread_;
    std::atomic<bool> running_;
    uint16_t transaction_id_; // 事务ID，用于Modbus TCP
    std::condition_variable cv_;
    time_t sendTime_;					//发送时间
	time_t reqTime_;				//召测时间
	bool sendFlag_;				//命令发送标识
    MModbusParam param_;			//规约参数
};