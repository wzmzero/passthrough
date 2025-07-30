#ifndef LOGRECORD_H
#define LOGRECORD_H

#include <fstream>
#include <mutex>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <iostream>

class LogRecord {
public:
    // 获取日志单例实例
    static LogRecord& getInstance() {
        static LogRecord instance;
        return instance;
    }

    // 记录二进制数据日志
    void logBinary(const std::string& prefix, const uint8_t* data, size_t len) {
        if (!loggingEnabled_) return;
        
        std::lock_guard<std::mutex> lock(fileMutex_);
        
        // 确保文件已打开
        if (!logFile_.is_open()) {
            openLogFile();
            if (!logFile_.is_open()) return; // 文件打开失败
        }
        
        // 写入时间戳
        logFile_ << currentTimeStr() << " " << prefix;
        
        // 写入字节数
        logFile_ << len << " bytes: ";
        
        // 尝试写入UTF-8内容或原始字节
        if (isValidUtf8(data, len)) {
            logFile_.write(reinterpret_cast<const char*>(data), len);
        } else {
            // 写入十六进制字节
            for (size_t i = 0; i < len; i++) {
                logFile_ << std::hex << std::setw(2) << std::setfill('0')
                         << static_cast<int>(data[i]);
                if (i < len - 1) logFile_ << " ";
            }
        }
        
        logFile_ << '\n';
        logFile_.flush(); // 确保立即写入
    }

    // 启用/禁用日志
    void setLoggingEnabled(bool enabled) {
        loggingEnabled_ = enabled;
    }

private:
    std::ofstream logFile_;
    std::mutex fileMutex_;
    std::atomic<bool> loggingEnabled_{true};
    bool fileOpened_ = false;

    // 私有构造函数（单例模式）
    LogRecord() {
        openLogFile();
    }
    
    // 禁用拷贝和赋值
    LogRecord(const LogRecord&) = delete;
    LogRecord& operator=(const LogRecord&) = delete;
    
    // 打开日志文件
    void openLogFile() {
        if (fileOpened_) return;
        
        logFile_.open("log.txt", std::ios::out | std::ios::app | std::ios::binary);
        if (logFile_.is_open()) {
            // 写入UTF-8 BOM标记
            const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
            logFile_.write(reinterpret_cast<const char*>(bom), sizeof(bom));
            fileOpened_ = true;
        } else {
            std::cerr << "ERROR: Failed to open log file: log.txt" << std::endl;
        }
    }

    // 获取当前时间字符串
    std::string currentTimeStr() {
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::tm tm;
        #ifdef _WIN32
            localtime_s(&tm, &now_time);
        #else
            localtime_r(&now_time, &tm);
        #endif
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count() << "]";
        return oss.str();
    }

    // 验证UTF-8有效性
    bool isValidUtf8(const uint8_t* data, size_t len) {
        size_t i = 0;
        while (i < len) {
            // 检查ASCII字符 (0-127)
            if (data[i] <= 0x7F) {
                i++;
            }
            // 检查2字节字符
            else if ((data[i] & 0xE0) == 0xC0) {
                if (i + 1 >= len || (data[i+1] & 0xC0) != 0x80) return false;
                i += 2;
            }
            // 检查3字节字符
            else if ((data[i] & 0xF0) == 0xE0) {
                if (i + 2 >= len || (data[i+1] & 0xC0) != 0x80 || 
                    (data[i+2] & 0xC0) != 0x80) return false;
                i += 3;
            }
            // 检查4字节字符
            else if ((data[i] & 0xF8) == 0xF0) {
                if (i + 3 >= len || (data[i+1] & 0xC0) != 0x80 || 
                    (data[i+2] & 0xC0) != 0x80 || 
                    (data[i+3] & 0xC0) != 0x80) return false;
                i += 4;
            }
            else {
                return false; // 无效的UTF-8起始字节
            }
        }
        return true;
    }
};

// 简化的日志宏
#define LOG_BINARY(prefix, data, len) LogRecord::getInstance().logBinary(prefix, data, len)

#endif // LOGRECORD_H