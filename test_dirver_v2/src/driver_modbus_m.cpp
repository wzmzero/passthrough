// driver_modbus_m.cpp
#include "driver_modbus_m.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <ctime>

using namespace std::chrono_literals;

// 辅助函数：获取当前时间（毫秒）
static time_t getCurrentTimeMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

DriverModbusM::DriverModbusM(VecDevInfo &v_devinfo, MModbusParam param) 
    : v_devinfo_(v_devinfo), param_(param), running_(false), transaction_id_(0),
      sendTime_(0), reqTime_(0), sendFlag_(false) {
    // 初始化设备信息
}

DriverModbusM::~DriverModbusM() {
    close();
}

bool DriverModbusM::open() {
    if (running_) return true;
    running_ = true;
    // 初始化请求时间为当前时间
    reqTime_ = getCurrentTimeMs();
    worker_thread_ = std::thread(&DriverModbusM::workThread, this);
    return true;
}

void DriverModbusM::close() {
    running_ = false;
    cv_.notify_one();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

size_t DriverModbusM::write(const uint8_t* data, size_t len) {
    std::vector<uint8_t> frame(data, data + len);
    {
        std::lock_guard<std::mutex> lock(recvMutex_);
        recvqueue_.push(std::move(frame));
    }
    cv_.notify_one();
    return len;
}

void DriverModbusM::workThread() {
    while (running_) {
        time_t current_time = getCurrentTimeMs();
        
        // 检查是否达到召测时间间隔
        if (!sendFlag_ && (current_time - reqTime_) >= param_.interval) {
            // 满足召测间隔，发送请求
            sendMessage();
            sendTime_ = current_time; // 记录发送时间
            sendFlag_ = true;         // 标记为已发送等待响应
        }
        
        // 检查是否超时
        if (sendFlag_ && (current_time - sendTime_) >= param_.time_out) {
            std::cerr << "Modbus request timeout!" << std::endl;
            sendFlag_ = false; // 超时，重置发送标志
            reqTime_ = current_time; // 重置请求时间为当前时间
        }
        
        // 处理接收队列
        {
            std::unique_lock<std::mutex> lock(recvMutex_);
            // 等待100ms或直到有数据
            cv_.wait_for(lock, 100ms, [this] { 
                return !recvqueue_.empty() || !running_; 
            });
            parseMessage();
        }
    }
}

void DriverModbusM::parseMessage() {
    while (!recvqueue_.empty()) {
        auto frame = recvqueue_.front();
        recvqueue_.pop();
        
        // 检查帧长度是否超过最大应答字节数
        if (frame.size() > param_.maxSize) {
            std::cerr << "Frame size exceeds maxSize: " << frame.size() 
                      << " > " << param_.maxSize << std::endl;
            continue;
        }
        
        VecTelemPoint v_parsetelem;
        int result = parseFrame(frame, v_parsetelem);
        if (result > 0) {
            // 收到有效响应，重置发送标志
            sendFlag_ = false;
            // 重置请求时间为当前时间
            reqTime_ = getCurrentTimeMs();
            
            for (auto& point : v_parsetelem) {
                // 在实际应用中，这里会将解析的点更新到数据库
                std::cout << "Parsed telem point: type=" << static_cast<int>(point.data_type)
                          << ", addr=" << point.proAddr
                          << ", value=" << point.value << std::endl;
            }
        }
    }
}

int DriverModbusM::parseFrame(const std::vector<uint8_t>& frame, VecTelemPoint& v_parsetelem) {
    uint8_t unit_id = 0;
    uint8_t func_code = 0;
    const uint8_t* data_start = nullptr;
    size_t data_length = 0;

    // 根据传输模式区分处理
    if (param_.transmit_mode == 1) { // TCP模式
        // TCP模式解析
        if (frame.size() < 8) return -1; // 最小长度检查
        
        // 提取Modbus TCP头部
        uint16_t trans_id = (frame[0] << 8) | frame[1];
        uint16_t proto_id = (frame[2] << 8) | frame[3];
        uint16_t length = (frame[4] << 8) | frame[5];
        unit_id = frame[6];
        func_code = frame[7];
        
        // 检查长度是否匹配
        if (frame.size() < 6 + length) return -1;
        data_start = frame.data() + 8;
        data_length = length - 2; // 减去单元ID和功能码长度
    } else { // RTU模式
        // RTU模式解析
        if (frame.size() < 4) return -1; // 最小长度检查（地址+功能码+CRC）
        
        // 验证CRC
        size_t data_len = frame.size() - 2;
        uint16_t crc_calc = crc16_modbus(frame.data(), data_len);
        uint16_t crc_recv = frame[data_len] | (frame[data_len+1] << 8);
        if (crc_calc != crc_recv) {
            std::cerr << "CRC check failed: calculated=0x" << std::hex << crc_calc
                      << ", received=0x" << crc_recv << std::dec << std::endl;
            return -4;
        }
        
        unit_id = frame[0];
        func_code = frame[1];
        data_start = frame.data() + 2;
        data_length = frame.size() - 4; // 减去地址、功能码和CRC
    }

    // 异常响应检查（功能码高位为1）
    if (func_code & 0x80) {
        if (data_length < 1) return -2;
        std::cerr << "Modbus exception: code=" << static_cast<int>(data_start[0]) << std::endl;
        return -2;
    }

    // 根据功能码解析数据
    switch (func_code) {
        case SWITCH_OUT: // 0x01 - 读开出状态
            if (data_length < 1) return -1;
            {
                uint8_t byte_count = data_start[0];
                if (data_length < 1 + byte_count) return -1;
                // 解析每个位状态（最多256个点）
                for (int byte_idx = 0; byte_idx < byte_count; ++byte_idx) {
                    uint8_t byte_val = data_start[1 + byte_idx];
                    for (int bit_idx = 0; bit_idx < 8; ++bit_idx) {
                        TelemPoint point;
                        point.proAddr = (byte_idx * 8) + bit_idx; // 位地址
                        point.data_type = Data_YX; // 开出状态属于遥信
                        point.value = (byte_val >> bit_idx) & 0x01;
                        v_parsetelem.push_back(point);
                    }
                }
            }
            break;

        case SWITCH_IN:  // 0x02 - 读开入状态
            if (data_length < 1) return -1;
            {
                uint8_t byte_count = data_start[0];
                if (data_length < 1 + byte_count) return -1;
                // 解析每个位状态（最多256个点）
                for (int byte_idx = 0; byte_idx < byte_count; ++byte_idx) {
                    uint8_t byte_val = data_start[1 + byte_idx];
                    for (int bit_idx = 0; bit_idx < 8; ++bit_idx) {
                        TelemPoint point;
                        point.proAddr = (byte_idx * 8) + bit_idx; // 位地址
                        point.data_type = Data_YX; // 开入状态属于遥信
                        point.value = (byte_val >> bit_idx) & 0x01;
                        v_parsetelem.push_back(point);
                    }
                }
            }
            break;

        case ANALOG_OUT: // 0x03 - 读模出状态
        case ANALOG_IN:  // 0x04 - 读模入状态
            if (data_length < 1) return -1;
            {
                uint8_t byte_count = data_start[0];
                int reg_count = byte_count / 2;
                if (data_length < 1 + byte_count) return -1;
                // 解析每个寄存器值（16位）
                for (int i = 0; i < reg_count; ++i) {
                    uint16_t reg_val = (data_start[1 + 2*i] << 8) | data_start[2 + 2*i];
                    TelemPoint point;
                    point.proAddr = i; // 寄存器索引
                    point.data_type = Data_YC; // 模入/模出状态属于遥测
                    point.value = reg_val;
                    v_parsetelem.push_back(point);
                }
            }
            break;

        case SNGL_SWITCH_SET: // 0x05 - 设置单路开关状态
            if (data_length < 4) return -1;
            {
                TelemPoint point;
                point.proAddr = (data_start[0] << 8) | data_start[1]; // 操作地址
                point.data_type = Data_YK; // 遥控
                point.value = (data_start[2] << 8) | data_start[3]; // 开关状态
                v_parsetelem.push_back(point);
            }
            break;

        case SNGL_ANALOG_SET: // 0x06 - 设置单路模拟量值
            if (data_length < 4) return -1;
            {
                TelemPoint point;
                point.proAddr = (data_start[0] << 8) | data_start[1]; // 操作地址
                point.data_type = Data_YT; // 遥调
                point.value = (data_start[2] << 8) | data_start[3]; // 模拟量值
                v_parsetelem.push_back(point);
            }
            break;

        default:
            std::cerr << "Unsupported function code: " << static_cast<int>(func_code) << std::endl;
            return -3;
    }
    return v_parsetelem.size();
}

void DriverModbusM::sendMessage() {
    // 将设备信息映射到四遥点
    v_sendtelem_.clear();
    for (const auto& dev : v_devinfo_) {
        TelemPoint point;
        point.proAddr = dev.proAddr;        // 规约地址
        point.data_type = dev.data_type;    // 四遥类型
        point.value = dev.value;            // 当前值
        v_sendtelem_.push_back(point);
    }
    
    // 为每个四遥点生成帧
    for (auto& telem : v_sendtelem_) {
        std::vector<uint8_t> frame = MakeFrame(telem);
        if (frame.empty()) continue;

        // 检查帧长度是否超过最大限制
        if (frame.size() > param_.maxSize) {
            std::cerr << "Frame size exceeds maxSize: " << frame.size() 
                      << " > " << param_.maxSize << std::endl;
            continue;
        }

        // 添加到发送队列
        {
            std::lock_guard<std::mutex> lock(sendMutex_);
            sendqueue_.push(frame);
        }
        
        // 仅TCP模式递增事务ID
        if (param_.transmit_mode == 1) { // TCP模式
            transaction_id_++;
        }
    }
}

std::vector<uint8_t> DriverModbusM::MakeFrame(TelemPoint &telem) {
    std::vector<uint8_t> frame;
    std::vector<uint8_t> pdu;
    uint16_t start_addr = static_cast<uint16_t>(telem.proAddr);
    uint16_t value = static_cast<uint16_t>(telem.value);

    // 单元地址
    uint8_t unit_addr = 1; // 默认值
    if (!v_devinfo_.empty()) {
        unit_addr = static_cast<uint8_t>(v_devinfo_[0].slave_addr);
    }

    // 根据四遥类型选择功能码
    uint8_t func_code = 0;
    switch (telem.data_type) {
        case Data_YX: 
            // 遥信：读开入状态（功能码02）
            func_code = SWITCH_IN; 
            break;
        case Data_YC: 
            // 遥测：读模入状态（功能码04）
            func_code = ANALOG_IN; 
            break;
        case Data_YK: 
            // 遥控：设置单路开关状态（功能码05）
            func_code = SNGL_SWITCH_SET; 
            break;
        case Data_YT: 
            // 遥调：设置单路模拟量值（功能码06）
            func_code = SNGL_ANALOG_SET; 
            break;
        default: 
            std::cerr << "Unsupported data type in MakeFrame: " 
                      << static_cast<int>(telem.data_type) << std::endl;
            return {};
    }

    // 构建PDU（协议数据单元）
    pdu.push_back(unit_addr);
    pdu.push_back(func_code);
    
    if (func_code == SNGL_SWITCH_SET || func_code == SNGL_ANALOG_SET) {
        // 单点写入命令（功能码5/6）
        pdu.push_back(start_addr >> 8);
        pdu.push_back(start_addr & 0xFF);
        if (func_code == SNGL_SWITCH_SET) {
            pdu.push_back(value ? 0xFF : 0x00);
            pdu.push_back(0x00);
        } else {
            pdu.push_back(value >> 8);
            pdu.push_back(value & 0xFF);
        }
    } else {
        // 批量读取命令（功能码1-4）
        pdu.push_back(start_addr >> 8);
        pdu.push_back(start_addr & 0xFF);
        pdu.push_back(0x00); // 寄存器数量高字节（固定1个）
        pdu.push_back(0x01); // 寄存器数量低字节
    }

    // 根据传输模式构建完整帧
    if (param_.transmit_mode == 1) { // TCP模式
        // TCP模式：添加MBAP头部
        frame.push_back(transaction_id_ >> 8);    // 事务ID高字节
        frame.push_back(transaction_id_ & 0xFF);  // 事务ID低字节
        frame.push_back(0x00); // 协议ID高字节
        frame.push_back(0x00); // 协议ID低字节
        
        // 设置长度字段（PDU长度）
        uint16_t pdu_length = pdu.size();
        frame.push_back(pdu_length >> 8);
        frame.push_back(pdu_length & 0xFF);
        
        // 合并PDU到帧
        frame.insert(frame.end(), pdu.begin(), pdu.end());
    } else { // RTU模式
        // RTU模式：直接使用PDU+CRC
        frame = pdu;
        uint16_t crc = crc16_modbus(frame.data(), frame.size());
        frame.push_back(crc & 0xFF); // CRC低字节在前
        frame.push_back(crc >> 8);   // CRC高字节在后
    }

    return frame;
}