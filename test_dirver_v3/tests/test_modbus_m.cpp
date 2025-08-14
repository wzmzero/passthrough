// test_modbus_m.cpp
#include "common.h"
#include "driver_modbus_m.h"
#include <iostream>
#include <thread>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <iomanip>
#include  "database.h"
// 重定义hexStr函数用于测试输出
static std::string hexStr(const std::vector<uint8_t>& v) {
    std::ostringstream oss;
    for (auto b : v) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
    }
    return oss.str();
}

int main() {
    Database db("config/config.db");
    // 创建设备信息 (覆盖所有数据类型)
    VecDevInfo vecDevInfo;
    // 遥信设备 (YX)
    vecDevInfo.push_back({1000000, 1, 0x1000, "YX Device", Data_YX, ValueType::Boolean, 0, ""});
    // 遥测设备 (YC)
    vecDevInfo.push_back({2000000, 1, 0x2000, "YC Device", Data_YC, ValueType::Integer, 0, "A"});
    // 遥控设备 (YK)
    vecDevInfo.push_back({3000000, 1, 0x3000, "YK Device", Data_YK, ValueType::Boolean, 0, ""});
    // 遥调设备 (YT)
    vecDevInfo.push_back({4000000, 1, 0x4000, "YT Device", Data_YT, ValueType::Integer, 0, "V"});
    
    // 打印设备信息
    for (auto& d : vecDevInfo) {
        std::cout << "DevInfo: ID=" << d.dataId << ", SlaveAddr=" << d.slave_addr 
                  << ", RegAddr=0x" << std::hex << d.proAddr << std::dec
                  << ", Desc=" << d.description 
                  << ", Type=" << static_cast<int>(d.data_type)
                  << ", ValueType=" << static_cast<int>(d.value_type)
                  << ", Value=" << d.value << ", Unit=" << d.unit << "\n";
    }   
    
    // 创建驱动并打开 (TCP模式)
    DriverModbusM drv(vecDevInfo, MModbusParam{1, 2000, 5000, 256});
    drv.open();
    
    // 发送测试帧的线程
    auto sender = [&] {
   
        while (true) {
            // 每2秒发送一组测试帧
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 功能码01: 读开关量输出 (线圈状态)
            std::vector<uint8_t> fc01_frame = {
                0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x01, 0x01, 0x02, 0x03, 0x01
            };
            drv.write(fc01_frame.data(), fc01_frame.size());
            std::cout << "Sent FC01 frame: " << hexStr(fc01_frame) << "\n";
            
            // 功能码02: 读开关量输入
            std::vector<uint8_t> fc02_frame = {
                0x00, 0x02, 0x00, 0x00, 0x00, 0x05, 0x01, 0x02, 0x01, 0x05
            };
            drv.write(fc02_frame.data(), fc02_frame.size());
            std::cout << "Sent FC02 frame: " << hexStr(fc02_frame) << "\n";
            
            // 功能码03: 读模拟量输出 (保持寄存器)
            std::vector<uint8_t> fc03_frame = {
                0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x01, 0x03, 0x04, 
                0x12, 0x34, 0x56, 0x78
            };
            drv.write(fc03_frame.data(), fc03_frame.size());
            std::cout << "Sent FC03 frame: " << hexStr(fc03_frame) << "\n";
            
            // 功能码04: 读模拟量输入
            std::vector<uint8_t> fc04_frame = {
                0x00, 0x04, 0x00, 0x00, 0x00, 0x07, 0x01, 0x04, 0x04, 
                0x55, 0xAA, 0x11, 0x22
            };
            drv.write(fc04_frame.data(), fc04_frame.size());
            std::cout << "Sent FC04 frame: " << hexStr(fc04_frame) << "\n";
            
            // 功能码05: 设置单路开关状态 (写线圈)
            std::vector<uint8_t> fc05_frame = {
                0x00, 0x05, 0x00, 0x00, 0x00, 0x06, 0x01, 0x05, 
                0x30, 0x00, 0xFF, 0x00
            };
            drv.write(fc05_frame.data(), fc05_frame.size());
            std::cout << "Sent FC05 frame: " << hexStr(fc05_frame) << "\n";
            
            // 功能码06: 设置单路模拟量值 (写寄存器)
            std::vector<uint8_t> fc06_frame = {
                0x00, 0x06, 0x00, 0x00, 0x00, 0x06, 0x01, 0x06, 
                0x40, 0x00, 0x12, 0x34
            };
            drv.write(fc06_frame.data(), fc06_frame.size());
            std::cout << "Sent FC06 frame: " << hexStr(fc06_frame) << "\n";
        }
    };
    
    // 启动发送线程
    std::thread sender_thread(sender);
    
    // 主线程处理发送队列
    while (true) {
        // 检查发送队列
        {
            std::lock_guard<std::mutex> lock(drv.sendMutex_);
            while (!drv.sendqueue_.empty()) {
                auto f = drv.sendqueue_.front();
                drv.sendqueue_.pop();
                std::cout << "Generated request frame: " << hexStr(f) << "\n";
            }
        }
        
        // 检查是否结束
        if (!sender_thread.joinable()) break;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    if (sender_thread.joinable()) {
        sender_thread.join();
    }
    
    drv.close();
    return 0;
}