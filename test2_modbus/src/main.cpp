#include "ChannelSerial.h"
#include "ChannelTcpServer.h"
#include "ChannelTcpClient.h"
#include "DriverModbus.h"
#include <iostream>
#include <memory>
#include <thread>

int main() {
    // 创建Modbus主站驱动
    auto masterDriver = std::make_shared<DriverModbus>(true);
    
    // 创建TCP客户端通道 (主站作为TCP客户端)
    auto tcpClient = std::make_shared<ChannelTcpClient>("127.0.0.1", 502);
    masterDriver->setChannel(tcpClient);
    
    // 设置响应回调
    masterDriver->setResponseCallback([](const ModbusRequest& request, const std::vector<uint8_t>& data) {
        std::cout << "Received Modbus response: ";
        for (auto byte : data) {
            printf("%02X ", byte);
        }
        std::cout << std::endl;
    });
    
    // 启动通道
    if (!tcpClient->start()) {
        std::cerr << "Failed to start TCP client" << std::endl;
        return 1;
    }
    
    // 创建Modbus从站驱动
    auto slaveDriver = std::make_shared<DriverModbus>(false);
    
    // 创建TCP服务器通道 (从站)
    auto tcpServer = std::make_shared<ChannelTcpServer>(502);
    slaveDriver->setChannel(tcpServer);
    
    // 启动TCP服务器
    if (!tcpServer->start()) {
        std::cerr << "Failed to start TCP server" << std::endl;
        return 1;
    }
    
    // 等待连接建立
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 主站发送请求
    ModbusRequest request;
    request.deviceId = 1;
    request.functionCode = 0x03; // 读保持寄存器
    request.startingAddress = 0;
    request.quantity = 10;
    masterDriver->sendRequest(request);
    
    // 保持运行
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // 清理
    tcpClient->stop();
    tcpServer->stop();
    
    return 0;
}