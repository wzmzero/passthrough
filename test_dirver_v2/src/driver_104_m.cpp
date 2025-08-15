#include "driver_104_m.h"
#include <cstring>
#include <chrono>
#include <algorithm>
#include <iostream>

using namespace IEC104;
using namespace std::chrono;

// 启动符
const uint8_t START_BYTE = 0x68;

Driver104M::Driver104M(uint16_t commonAddr) 
    : commonAddr_(commonAddr) {}

Driver104M::~Driver104M() {
    close();
}

bool Driver104M::open() {
    if (running_) return true;
    
    running_ = true;
    resetLink();
    workerThread_ = std::thread(&Driver104M::processThread, this);
    return true;
}

void Driver104M::close() {
    if (!running_) return;
    
    running_ = false;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

size_t Driver104M::write(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    recvQueue_.push(std::vector<uint8_t>(data, data + len));
    return len;
}

void Driver104M::processThread() {
    while (running_) {
        // 处理接收队列
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            while (!recvQueue_.empty()) {
                auto& data = recvQueue_.front();
                APDU apdu;
                if (parseAPDU(data.data(), data.size(), apdu)) {
                    switch (apdu.control.format) {
                        case I_FORMAT: handleIFrame(apdu); break;
                        case S_FORMAT: handleSFrame(apdu); break;
                        case U_FORMAT: handleUFrame(apdu); break;
                    }
                }
                recvQueue_.pop();
            }
        }
        
        // 状态机处理
        if (!linkActive_) {
            sendStartActivation();
        } 
        else if (startDtConfirmed_) {
            // 周期总召唤
            static time_t lastCallTime = 0;
            time_t now = time(nullptr);
            if (now - lastCallTime >= 60) { // 每分钟召唤一次
                sendGeneralCall();
                lastCallTime = now;
            }
        }
        
        // 发送队列处理
        if (!sendQueue_.empty()) {
            std::lock_guard<std::mutex> lock(queueMutex_);
            auto apdu = sendQueue_.front();
            auto frame = buildAPDU(apdu);
            // 实际发送操作 (伪代码)
            // physicalLayerSend(frame.data(), frame.size());
            sendQueue_.pop();
        }
        
        // 超时检测
        checkTimeouts();
        
        // 限速10ms循环
        std::this_thread::sleep_for(milliseconds(10));
    }
}

bool Driver104M::parseAPDU(const uint8_t* data, size_t len, APDU& apdu) {
    // 基本长度检查
    if (len < 6 || data[0] != START_BYTE) return false;
    
    size_t idx = 1;
    uint8_t apduLen = data[idx++];
    
    // 控制域解析
    uint8_t ctrl1 = data[idx++];
    uint8_t ctrl2 = data[idx++];
    
    apdu.control.format = static_cast<CtrlFormat>(ctrl1 & 0x03);
    switch (apdu.control.format) {
        case I_FORMAT:
            apdu.control.sendSeq = ((ctrl1 >> 2) & 0x3F) | (ctrl2 << 6);
            apdu.control.recvSeq = (data[idx++] << 8) | data[idx++];
            break;
            
        case S_FORMAT:
            apdu.control.ackSeq = ((ctrl1 >> 2) & 0x3F) | (ctrl2 << 6);
            break;
            
        case U_FORMAT:
            apdu.control.function = static_cast<UFunction>(ctrl2);
            break;
    }
    
    // ASDU解析 (仅处理I帧)
    if (apdu.control.format == I_FORMAT && len > idx) {
        apdu.asdu.type = static_cast<TypeIdentify>(data[idx++]);
        apdu.asdu.isSequence = (data[idx] & 0x80) != 0;
        apdu.asdu.numElements = data[idx++] & 0x7F;
        apdu.asdu.cause = static_cast<TransCause>(data[idx++]);
        
        // 公共地址 (2字节)
        apdu.asdu.commonAddr = (data[idx++] << 8) | data[idx++];
        
        // 信息对象解析
        for (int i = 0; i < apdu.asdu.numElements; i++) {
            InfoObjectAddr addr;
            addr.bytes[0] = data[idx++];
            addr.bytes[1] = data[idx++];
            addr.bytes[2] = data[idx++];
            apdu.asdu.ioAddrs.push_back(addr);
            
            // 值解析 (4字节浮点)
            uint32_t valBits = (data[idx++] << 24) | (data[idx++] << 16) 
                             | (data[idx++] << 8) | data[idx++];
            apdu.asdu.values.push_back(*reinterpret_cast<float*>(&valBits));
        }
    }
    
    return true;
}

std::vector<uint8_t> Driver104M::buildAPDU(const APDU& apdu) {
    std::vector<uint8_t> frame;
    frame.push_back(START_BYTE);
    frame.push_back(0); // 长度占位
    
    // 控制域构建
    switch (apdu.control.format) {
        case I_FORMAT: {
            uint16_t sendSeq = apdu.control.sendSeq;
            uint8_t ctrl1 = (I_FORMAT) | ((sendSeq & 0x3F) << 2);
            uint8_t ctrl2 = (sendSeq >> 6) & 0xFF;
            frame.push_back(ctrl1);
            frame.push_back(ctrl2);
            frame.push_back(apdu.control.recvSeq >> 8);
            frame.push_back(apdu.control.recvSeq & 0xFF);
            break;
        }
            
        case S_FORMAT: {
            uint16_t ackSeq = apdu.control.ackSeq;
            uint8_t ctrl1 = (S_FORMAT) | ((ackSeq & 0x3F) << 2);
            uint8_t ctrl2 = (ackSeq >> 6) & 0xFF;
            frame.push_back(ctrl1);
            frame.push_back(ctrl2);
            frame.push_back(0);
            frame.push_back(0);
            break;
        }
            
        case U_FORMAT: {
            frame.push_back(U_FORMAT);
            frame.push_back(apdu.control.function);
            frame.push_back(0);
            frame.push_back(0);
            break;
        }
    }
    
    // ASDU构建 (仅I帧)
    if (apdu.control.format == I_FORMAT) {
        frame.push_back(apdu.asdu.type);
        frame.push_back(apdu.asdu.isSequence ? 0x80 : 0 | apdu.asdu.numElements);
        frame.push_back(apdu.asdu.cause);
        
        // 公共地址
        frame.push_back(apdu.asdu.commonAddr >> 8);
        frame.push_back(apdu.asdu.commonAddr & 0xFF);
        
        // 信息对象
        for (int i = 0; i < apdu.asdu.numElements; i++) {
            uint32_t addr = apdu.asdu.ioAddrs[i];
            frame.push_back(addr & 0xFF);         // LSB
            frame.push_back((addr >> 8) & 0xFF);
            frame.push_back((addr >> 16) & 0xFF);  // MSB
            
            // 值 (4字节浮点)
            float val = apdu.asdu.values[i];
            uint32_t valBits = *reinterpret_cast<uint32_t*>(&val);
            frame.push_back((valBits >> 24) & 0xFF);
            frame.push_back((valBits >> 16) & 0xFF);
            frame.push_back((valBits >> 8) & 0xFF);
            frame.push_back(valBits & 0xFF);
        }
    }
    
    // 更新长度字段
    frame[1] = frame.size() - 2;
    return frame;
}

void Driver104M::handleIFrame(const APDU& apdu) {
    // 更新T3计时器
    lastT3Time_ = time(nullptr);
    
    // 验证序列号
    if (apdu.control.recvSeq != recvSeq_) {
        resetLink();
        return;
    }
    
    // 更新接收序列号
    recvSeq_ = (recvSeq_ + 1) % 0x8000;
    
    // 处理不同类型
    switch (apdu.asdu.type) {
        case M_SP_NA: // 单点信息
        case M_ME_NA: // 测量值
            if (dataCallback_) {
                for (int i = 0; i < apdu.asdu.numElements; i++) {
                    dataCallback_(apdu.asdu.ioAddrs[i], apdu.asdu.values[i]);
                }
            }
            break;
            
        case C_IC_NA: // 总召唤响应
            // 处理数据...
            break;
            
        case C_SC_NA: // 遥控确认
            // 处理确认...
            break;
    }
    
    // 检查是否需要发送S帧
    if ((recvSeq_ - ackSeq_) >= w_factor_) {
        sendSFrame();
    }
}

void Driver104M::handleSFrame(const APDU& apdu) {
    // 处理确认帧
    uint16_t ackSeq = apdu.control.ackSeq;
    
    // 清除已确认的I帧
    auto it = unackIFrames_.begin();
    while (it != unackIFrames_.end()) {
        if (it->first <= ackSeq) {
            it = unackIFrames_.erase(it);
        } else {
            ++it;
        }
    }
    
    // 关闭T1计时器
    t1Active_ = false;
}

void Driver104M::handleUFrame(const APDU& apdu) {
    switch (apdu.control.function) {
        case START_CON:
            startDtConfirmed_ = true;
            break;
            
        case TEST_CON:
            // 重置T1计时器
            t1Active_ = false;
            break;
            
        default:
            // 忽略其他U帧
            break;
    }
}

void Driver104M::sendSFrame() {
    APDU apdu;
    apdu.control.format = S_FORMAT;
    apdu.control.ackSeq = recvSeq_;
    
    std::lock_guard<std::mutex> lock(queueMutex_);
    sendQueue_.push(apdu);
    ackSeq_ = recvSeq_;
}

bool Driver104M::sendStartActivation() {
    APDU apdu;
    apdu.control.format = U_FORMAT;
    apdu.control.function = START_ACT;
    
    std::lock_guard<std::mutex> lock(queueMutex_);
    sendQueue_.push(apdu);
    updateT1Timer();
    return true;
}

bool Driver104M::sendGeneralCall() {
    APDU apdu;
    apdu.control.format = I_FORMAT;
    apdu.control.sendSeq = sendSeq_;
    apdu.control.recvSeq = recvSeq_;
    
    apdu.asdu.type = C_IC_NA;
    apdu.asdu.numElements = 1;
    apdu.asdu.cause = ACT;
    apdu.asdu.commonAddr = commonAddr_;
    apdu.asdu.ioAddrs.push_back(0); // 全局地址
    apdu.asdu.values.push_back(0);  // 无数据
    
    std::lock_guard<std::mutex> lock(queueMutex_);
    sendQueue_.push(apdu);
    unackIFrames_[sendSeq_] = apdu;
    sendSeq_ = (sendSeq_ + 1) % 0x8000;
    updateT1Timer();
    return true;
}

bool Driver104M::sendCommand(uint32_t ioAddr, bool value) {
    APDU apdu;
    apdu.control.format = I_FORMAT;
    apdu.control.sendSeq = sendSeq_;
    apdu.control.recvSeq = recvSeq_;
    
    apdu.asdu.type = C_SC_NA;
    apdu.asdu.numElements = 1;
    apdu.asdu.cause = ACT;
    apdu.asdu.commonAddr = commonAddr_;
    apdu.asdu.ioAddrs.push_back(ioAddr);
    apdu.asdu.values.push_back(value ? 1.0f : 0.0f);
    
    std::lock_guard<std::mutex> lock(queueMutex_);
    sendQueue_.push(apdu);
    unackIFrames_[sendSeq_] = apdu;
    sendSeq_ = (sendSeq_ + 1) % 0x8000;
    updateT1Timer();
    return true;
}

void Driver104M::resetLink() {
    sendSeq_ = 0;
    recvSeq_ = 0;
    ackSeq_ = 0;
    startDtConfirmed_ = false;
    t1Active_ = false;
    
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!sendQueue_.empty()) sendQueue_.pop();
    unackIFrames_.clear();
}

void Driver104M::updateT1Timer() {
    lastT1Time_ = time(nullptr);
    t1Active_ = true;
}

void Driver104M::checkTimeouts() {
    time_t now = time(nullptr);
    
    // T1超时处理 (发送超时)
    if (t1Active_ && (now - lastT1Time_ >= t1_timeout_)) {
        resetLink();
        return;
    }
    
    // T2超时处理 (确认超时)
    if (!unackIFrames_.empty() && (now - lastT2Time_ >= t2_timeout_)) {
        sendSFrame();
        lastT2Time_ = now;
    }
    
    // T3超时处理 (链路空闲)
    if ((now - lastT3Time_ >= t3_timeout_)) {
        APDU apdu;
        apdu.control.format = U_FORMAT;
        apdu.control.function = TEST_ACT;
        
        std::lock_guard<std::mutex> lock(queueMutex_);
        sendQueue_.push(apdu);
        updateT1Timer();
    }
}