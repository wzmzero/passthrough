cmake_minimum_required(VERSION 3.12)
project(ModbusAsioExample)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Boost 库
find_package(Boost 1.66 REQUIRED COMPONENTS system)

# 添加可执行文件
add_executable(modbus-asio-example
    src/main.cpp
    src/ModbusMaster.cpp
    src/ModbusSlave.cpp
    include/ModbusMaster.h
    include/ModbusSlave.h
)

# 包含头文件目录
target_include_directories(modbus-asio-example PRIVATE 
    ${Boost_INCLUDE_DIRS}
    include/
)

# 链接库
target_link_libraries(modbus-asio-example PRIVATE
    ${Boost_LIBRARIES}
)

# 安装目标
install(TARGETS modbus-asio-example DESTINATION bin)