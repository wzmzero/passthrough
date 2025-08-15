// logrecord.h
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
#include <map>
#include <vector>
#include <cstdarg>
#include <filesystem>
#include <memory>
#include <system_error>

namespace fs = std::filesystem;

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class LogRecord {
public:
    // 获取日志单例实例
    static LogRecord& getInstance() {
        static LogRecord instance;
        return instance;
    }

    // 初始化全局配置
    static void init(bool consoleLog, bool fileLog, const std::string& logDir = "logs") {
        getInstance()._init(consoleLog, fileLog, logDir);
    }

    // 添加格式化日志
    static void addLog(LogLevel level, const char* format, ...) {
        va_list args;
        va_start(args, format);
        getInstance()._addLog("", level, format, args);
        va_end(args);
    }

    // 添加通道特定日志
    static void addChannelLog(const std::string& channel, LogLevel level, const char* format, ...) {
        va_list args;
        va_start(args, format);
        getInstance()._addLog(channel, level, format, args);
        va_end(args);
    }

    // 记录二进制数据日志（十六进制格式）
    static void logBinary(const std::string& channel, const std::string& prefix, const uint8_t* data, size_t len) {
        getInstance()._logBinary(channel, prefix, data, len);
    }

    // 记录二进制数据日志（UTF-8文本格式）
    static void logBinaryAsText(const std::string& channel, const std::string& prefix, const uint8_t* data, size_t len) {
        getInstance()._logBinaryAsText(channel, prefix, data, len);
    }

private:
    struct ChannelLog {
        std::ofstream file;
        std::string filename;
        bool needsReopen = false;  // 标记是否需要重新打开文件
    };

    bool _consoleLog = true;
    bool _fileLog = false;
    fs::path _logDir;
    std::mutex _mutex;
    std::map<std::string, ChannelLog> _channelLogs;
    
    // 私有构造函数（单例模式）
    LogRecord() = default;
    
    // 禁用拷贝和赋值
    LogRecord(const LogRecord&) = delete;
    LogRecord& operator=(const LogRecord&) = delete;
    
    void _init(bool consoleLog, bool fileLog, const std::string& logDir) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        _consoleLog = consoleLog;
        _fileLog = fileLog;
        _logDir = fs::path(logDir);
        
        if (_fileLog) {
            _ensureLogDirectory();
        }
    }

    // 确保日志目录存在
    void _ensureLogDirectory() {
        try {
            if (!fs::exists(_logDir)) {
                fs::create_directories(_logDir);
                // 直接使用标准输出，避免依赖日志宏
                std::cout << "Created log directory: " << _logDir.string() << std::endl;
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
            _fileLog = false;  // 禁用文件日志
        }
    }

    void _addLog(const std::string& channel, LogLevel level, const char* format, va_list args) {
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), format, args);
        
        const auto logLine = _formatLogLine(channel, level, buffer);
        
        std::lock_guard<std::mutex> lock(_mutex);
        
        // 输出到控制台
        if (_consoleLog) {
            if (level >= LogLevel::WARNING) {
                std::cerr << logLine << std::endl;
            } else {
                std::cout << logLine << std::endl;
            }
        }
        
        // 输出到文件
        if (_fileLog) {
            // 确保日志目录存在
            _ensureLogDirectory();
            
            std::string targetChannel = channel.empty() ? "main" : channel;
            
            try {
                auto& clog = _getChannelLog(targetChannel);
                
                // 检查文件状态并尝试重新打开
                if (clog.needsReopen || !clog.file.is_open()) {
                    _reopenChannelLog(clog);
                }
                
                if (clog.file.is_open()) {
                    clog.file << logLine << std::endl;
                    clog.file.flush();  // 确保立即写入
                }
            } catch (const std::exception& e) {
                std::cerr << "Error writing log: " << e.what() << std::endl;
            }
        }
    }

    // 重新打开通道日志文件
    void _reopenChannelLog(ChannelLog& clog) {
        try {
            // 关闭现有文件（如果打开）
            if (clog.file.is_open()) {
                clog.file.close();
            }
            
            // 尝试重新打开文件
            clog.file.open(clog.filename, std::ios::out | std::ios::app | std::ios::binary);
            
            if (clog.file.is_open()) {
                // 检查文件是否为空（新创建的文件）
                clog.file.seekp(0, std::ios::end);
                if (clog.file.tellp() == 0) {
                    // 写入UTF-8 BOM标记
                    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
                    clog.file.write(reinterpret_cast<const char*>(bom), sizeof(bom));
                }
                clog.needsReopen = false;
                // 直接使用标准输出，避免依赖日志宏
                std::cout << "Reopened log file: " << clog.filename << std::endl;
            } else {
                std::cerr << "ERROR: Failed to reopen log file: " << clog.filename << std::endl;
                clog.needsReopen = true;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error reopening log file: " << e.what() << std::endl;
            clog.needsReopen = true;
        }
    }

    void _logBinary(const std::string& channel, const std::string& prefix, const uint8_t* data, size_t len) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (!_fileLog) return;
        
        // 确保日志目录存在
        _ensureLogDirectory();
        
        try {
            auto& clog = _getChannelLog(channel);
            
            // 检查文件状态并尝试重新打开
            if (clog.needsReopen || !clog.file.is_open()) {
                _reopenChannelLog(clog);
            }
            
            if (!clog.file.is_open()) return;
            
            // 写入时间戳
            clog.file << _currentTimeStr() << " " << prefix;
            
            // 写入字节数
            clog.file << std::to_string(len) << " bytes: ";
            
            // 写入十六进制字节
            for (size_t i = 0; i < len; i++) {
                clog.file << std::hex << std::setw(2) << std::setfill('0')
                         << static_cast<int>(data[i]);
                if (i < len - 1) clog.file << " ";
            }
            clog.file << '\n';
            clog.file.flush();  // 确保立即写入
        } catch (const std::exception& e) {
            std::cerr << "Error writing binary log: " << e.what() << std::endl;
        }
    }

    // 二进制转文本日志函数
void _logBinaryAsText(const std::string& channel, const std::string& prefix, const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    if (!_fileLog) return;  
    
    try {
        std::ostringstream oss;
        oss << prefix << " " <<  std::to_string(len) << " bytes: ";
        std::string text = oss.str(); // 获取基础字符串
        
        // 直接写入UTF-8字节序列
        text.append(reinterpret_cast<const char*>(data), len);
        
        // 构建完整日志消息
        const auto logLine = _formatLogLine(channel, LogLevel::INFO, text.c_str());
        
        // 输出到文件
        if (_fileLog) {
            _ensureLogDirectory();
            try {
                auto& clog = _getChannelLog(channel);
                if (clog.needsReopen || !clog.file.is_open()) {
                    _reopenChannelLog(clog);
                }
                if (clog.file.is_open()) {
                    clog.file << logLine << std::endl;
                    clog.file.flush();
                }
            } catch (const std::exception& e) {
                std::cerr << "Error writing binary text log: " << e.what() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in binary text log: " << e.what() << std::endl;
    }
}

    // 在 _getChannelLog 函数中添加文件创建逻辑
ChannelLog& _getChannelLog(const std::string& channel) {
    auto it = _channelLogs.find(channel);
    if (it != _channelLogs.end()) {
        return it->second;
    }
    
    // 创建新的通道日志
    ChannelLog newLog;
    newLog.filename = (_logDir / (channel + ".txt")).string();
    
    try {
        // 确保目录存在
        _ensureLogDirectory();
        
        // 尝试创建并打开文件
        newLog.file.open(newLog.filename, std::ios::out | std::ios::app | std::ios::binary);
        
        if (newLog.file.is_open()) {
            // 检查文件是否为空（新创建的文件）
            newLog.file.seekp(0, std::ios::end);
            if (newLog.file.tellp() == 0) {
                // 写入UTF-8 BOM标记
                const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
                newLog.file.write(reinterpret_cast<const char*>(bom), sizeof(bom));
            }
            newLog.needsReopen = false;
        } else {
            std::cerr << "WARNING: Failed to create log file: " << newLog.filename << std::endl;
            newLog.needsReopen = true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating log file: " << e.what() << std::endl;
        newLog.needsReopen = true;
    }
    
    return _channelLogs.emplace(channel, std::move(newLog)).first->second;
}

    std::string _formatLogLine(const std::string& channel, LogLevel level, const char* message) {
        std::ostringstream oss;
        oss << _currentTimeStr();
        
        // 日志级别标记
        switch (level) {
            case LogLevel::DEBUG:    oss << " [DEBUG] "; break;
            case LogLevel::INFO:     oss << " [INFO ] "; break;
            case LogLevel::WARNING:  oss << " [WARN ] "; break;
            case LogLevel::ERROR:    oss << " [ERROR] "; break;
        }
        
        // 通道标记（如果有）
        if (!channel.empty()) {
            oss << "[" << channel << "] ";
        }
        
        oss << message;
        return oss.str();
    }

    // 获取当前时间字符串
    std::string _currentTimeStr() {
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
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }
};

// 全局日志宏
#define LOG_DEBUG(format, ...)    LogRecord::addLog(LogLevel::DEBUG, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)     LogRecord::addLog(LogLevel::INFO, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...)  LogRecord::addLog(LogLevel::WARNING, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...)    LogRecord::addLog(LogLevel::ERROR, format, ##__VA_ARGS__)

// 通道日志宏
#define CH_LOG_DEBUG(channel, format, ...)    LogRecord::addChannelLog(channel, LogLevel::DEBUG, format, ##__VA_ARGS__)
#define CH_LOG_INFO(channel, format, ...)     LogRecord::addChannelLog(channel, LogLevel::INFO, format, ##__VA_ARGS__)
#define CH_LOG_WARNING(channel, format, ...)  LogRecord::addChannelLog(channel, LogLevel::WARNING, format, ##__VA_ARGS__)
#define CH_LOG_ERROR(channel, format, ...)    LogRecord::addChannelLog(channel, LogLevel::ERROR, format, ##__VA_ARGS__)

// 二进制日志宏（十六进制格式）
#define LOG_BINARY(channel, prefix, data, len) LogRecord::logBinary(channel, prefix, data, len)

// 二进制日志宏（文本格式）
#define LOG_BINARY_TEXT(channel, prefix, data, len) LogRecord::logBinaryAsText(channel, prefix, data, len)

#endif // LOGRECORD_H