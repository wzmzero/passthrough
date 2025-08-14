// driver_base.h
#pragma once
 
#include <functional>
#include <vector>
#include <cstdint>
class DriverBase {
public:
    virtual ~DriverBase() = default;
    
    // 核心接口
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual size_t write(const uint8_t* data, size_t len) = 0;   
    
 

};