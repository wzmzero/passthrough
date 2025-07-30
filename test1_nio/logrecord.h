#ifndef LOGRECORD_H
#define LOGRECORD_H

#include <fstream>
#include <mutex>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>
#include <vector>
#include <codecvt>
#include <locale>

class LogRecord {
public:
    // 日志显示模式
    enum DisplayMode {
        COMPACT,    // 简洁模式 (只显示文本)
        VERBOSE,    // 详细模式 (显示文本和编码细节)
        DEBUG       // 调试模式 (显示所有细节)
    };

    // 获取日志单例实例
    static LogRecord& getInstance() {
        static LogRecord instance;
        return instance;
    }

    // 记录文本日志（线程安全）
    void log(const std::string& message) {
        if (!loggingEnabled_) return;
        
        std::string formatted = formatLogEntry(message);
        writeToLog(formatted);
    }

    // 记录二进制数据日志（线程安全，假设数据是UTF-8编码）
    void logBinary(const std::string& prefix, const uint8_t* data, size_t len) {
        if (!loggingEnabled_) return;
        
        std::string formatted = formatLogEntry(prefix + formatUtf8Data(data, len));
        writeToLog(formatted);
    }

    // 设置日志显示模式
    void setDisplayMode(DisplayMode mode) {
        displayMode_ = mode;
        log("Display mode set to: " + std::to_string(mode));
    }

private:
    std::ofstream logFile_;
    std::mutex fileMutex_;
    std::atomic<bool> loggingEnabled_{true};
    std::atomic<DisplayMode> displayMode_{COMPACT};

    // 私有构造函数（单例模式）
    LogRecord() {
        // 设置locale以支持UTF-8
        // try {
        //     std::locale::global(std::locale("en_US.UTF-8"));
        //     logFile_.imbue(std::locale("en_US.UTF-8"));
        // } catch (...) {
        //     std::cerr << "Warning: Failed to set UTF-8 locale" << std::endl;
        // }

        logFile_.open("log.txt", std::ios::out | std::ios::app);
        if (!logFile_.is_open()) {
            std::cerr << "FATAL: Failed to open log file: log.txt" << std::endl;
        } else {
            // 写入UTF-8 BOM标记
            logFile_ << "\xEF\xBB\xBF";
            logFile_ << "========================================\n"
                     << "Log session started (UTF-8 encoding)\n"
                     << "========================================\n";
            logFile_.flush();
        }
    }
    
    // 禁用拷贝和赋值
    LogRecord(const LogRecord&) = delete;
    LogRecord& operator=(const LogRecord&) = delete;

    // 格式化日志条目
    std::string formatLogEntry(const std::string& message) {
        if (!loggingEnabled_) return "";
        
        // 获取当前时间（毫秒级精度）
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
            << '.' << std::setfill('0') << std::setw(3) << ms.count() << "] "
            << message;
        return oss.str();
    }

    // 验证UTF-8有效性
    bool isValidUtf8(const uint8_t* data, size_t len) {
        size_t i = 0;
        while (i < len) {
            if ((data[i] & 0x80) == 0x00) { // 1字节字符
                i++;
            } else if ((data[i] & 0xE0) == 0xC0) { // 2字节字符
                if (i+1 >= len || (data[i+1] & 0xC0) != 0x80) return false;
                i += 2;
            } else if ((data[i] & 0xF0) == 0xE0) { // 3字节字符
                if (i+2 >= len || (data[i+1] & 0xC0) != 0x80 || 
                    (data[i+2] & 0xC0) != 0x80) return false;
                i += 3;
            } else if ((data[i] & 0xF8) == 0xF0) { // 4字节字符
                if (i+3 >= len || (data[i+1] & 0xC0) != 0x80 || 
                    (data[i+2] & 0xC0) != 0x80 || (data[i+3] & 0xC0) != 0x80) return false;
                i += 4;
            } else {
                return false;
            }
        }
        return true;
    }

    // 格式化UTF-8数据
    std::string formatUtf8Data(const uint8_t* data, size_t len) {
        if (len == 0) return "0 bytes";
        
        bool validUtf8 = isValidUtf8(data, len);
        std::string utf8Str(reinterpret_cast<const char*>(data), len);
        
        std::ostringstream oss;
        oss << len << " bytes: ";
        
        switch (displayMode_) {
            case COMPACT:
                if (validUtf8) {
                    oss << utf8Str;
                } else {
                    oss << "[INVALID UTF-8 DATA]";
                }
                break;
                
            case VERBOSE:
                if (validUtf8) {
                    oss << utf8Str << " (";
                    for (size_t i = 0; i < len; i++) {
                        oss << std::hex << std::setw(2) << std::setfill('0') 
                            << static_cast<int>(data[i]);
                        if (i < len-1) oss << " ";
                    }
                    oss << ")";
                } else {
                    oss << "[INVALID UTF-8 DATA: ";
                    for (size_t i = 0; i < len; i++) {
                        oss << std::hex << std::setw(2) << std::setfill('0') 
                            << static_cast<int>(data[i]);
                        if (i < len-1) oss << " ";
                    }
                    oss << "]";
                }
                break;
                
            case DEBUG:
                oss << "\n  Text: ";
                if (validUtf8) {
                    oss << utf8Str;
                } else {
                    oss << "[INVALID UTF-8 DATA]";
                }
                oss << "\n  Hex: ";
                for (size_t i = 0; i < len; i++) {
                    oss << std::hex << std::setw(2) << std::setfill('0') 
                        << static_cast<int>(data[i]);
                    if (i < len-1) oss << " ";
                }
                oss << "\n  Binary: ";
                for (size_t i = 0; i < len; i++) {
                    for (int j = 7; j >= 0; j--) {
                        oss << ((data[i] >> j) & 1);
                    }
                    if (i < len-1) oss << " ";
                }
                break;
        }
        
        return oss.str();
    }

    // 写入日志（带锁）
    void writeToLog(const std::string& formatted) {
        if (!loggingEnabled_ || formatted.empty()) return;
        
        std::lock_guard<std::mutex> lock(fileMutex_);
        
        if (logFile_.is_open()) {
            logFile_ << formatted << std::endl;
            logFile_.flush();
        } else {
            // 回退到标准输出
            std::cout << formatted << std::endl;
        }
    }
};

// 简化的日志宏
#define LOG(message) LogRecord::getInstance().log(message)
#define LOG_BINARY(prefix, data, len) LogRecord::getInstance().logBinary(prefix, data, len)
#define SET_LOG_MODE(mode) LogRecord::getInstance().setDisplayMode(mode)

#endif // LOGRECORD_H