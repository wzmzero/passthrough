cmake_minimum_required(VERSION 3.10)
project(ModbusFramework)

set(CMAKE_CXX_STANDARD 17)
include_directories(inc)
add_executable(ModbusFramework
    src/main.cpp
    src/ChannelSerial.cpp
    src/ChannelTcpServer.cpp
    src/ChannelTcpClient.cpp
    src/DriverModbus.cpp
)

target_link_libraries(ModbusFramework pthread)