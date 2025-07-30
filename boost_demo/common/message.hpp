#pragma once
#include <cstdint>
#include <chrono>
#include <array>

struct TimestampedMsg {
    uint64_t counter;
    std::chrono::system_clock::time_point timestamp;
};

// 网络字节序转换
template <typename T>
void to_network_order(T& value) {
    static_assert(std::is_arithmetic_v<T>, "Only arithmetic types supported");
    if constexpr (sizeof(T) > 1) {
        uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
        for (size_t i = 0; i < sizeof(T)/2; ++i) {
            std::swap(bytes[i], bytes[sizeof(T)-1-i]);
        }
    }
}