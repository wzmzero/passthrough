#pragma once
#include <iostream>

void log(LogLevel level, const std::string &message)
{
    // 实际项目中应使用日志系统
    if (level == LogLevel::ERROR)
    {
        std::cerr << "\033[1;31m[ERROR] " << message << "\033[0m\n";
    }
    else if (level == LogLevel::WARNING)
    {
        std::cerr << "\033[1;33m[WARNING] " << message << "\033[0m\n";
    }
    else if (level == LogLevel::INFO)
    {
        std::cout << "\033[1;32m[INFO] " << message << "\033[0m\n";
    }
    else
    {
        std::cout << "[LOG] " << message << "\n";
    }
}