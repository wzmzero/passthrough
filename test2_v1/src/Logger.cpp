#include "Logger.h"
#include <filesystem>
#include <sstream>
#include <iostream>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_logFiles) {
        if (pair.second.is_open()) {
            pair.second.close();
        }
    }
}

std::ofstream& Logger::getLogFile(const std::string& prefix) {
    auto it = m_logFiles.find(prefix);
    if (it != m_logFiles.end()) {
        return it->second;
    }
    
    // 创建新日志文件
    std::string filename = prefix + ".log";
    std::ofstream file(filename, std::ios::out | std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开日志文件: " + filename);
    }
    
    auto result = m_logFiles.emplace(prefix, std::move(file));
    return result.first->second;
}

void Logger::log(const std::string& prefix, LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& file = getLogFile(prefix);
    
    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    char time_str[20];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&now_time));
    
    // 日志级别字符串
    std::string levelStr;
    switch(level) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO: levelStr = "INFO"; break;
        case LogLevel::WARNING: levelStr = "WARNING"; break;
        case LogLevel::ERROR: levelStr = "ERROR"; break;
        default: levelStr = "UNKNOWN";
    }
    
    file << "[" << time_str << "] [" << levelStr << "] " << message << std::endl;
}

void Logger::logData(const std::string& prefix, const std::string& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& file = getLogFile(prefix);
    
    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    char time_str[20];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&now_time));
    
    // 写入数据日志
    file << "[" << time_str << "] [DATA] Hex:";
    for (unsigned char c : data) {
        file << " " << std::hex << std::setw(2) << std::setfill('0') 
             << static_cast<unsigned int>(c);
    }
    file << " | String: \"" << data << "\"" << std::endl;
}