#pragma once

#include <fstream>
#include <mutex>
#include <unordered_map>
#include <string>
#include <iomanip>
#include <chrono>
#include <ctime>
#include "Common.h"

class Logger {
public:
    static Logger& instance();
    
    void log(const std::string& prefix, LogLevel level, const std::string& message);
    void logData(const std::string& prefix, const std::string& data);
    
    // 禁用复制构造和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
    ~Logger();
    
    std::ofstream& getLogFile(const std::string& prefix);
    
    std::mutex m_mutex;
    std::unordered_map<std::string, std::ofstream> m_logFiles;
};