#include "DriverModbusM.h"
#include "DriverModbusS.h"
#include "ChannelTcpServer.h"
#include "ChannelTcpClient.h"
#include "ChannelSerial.h"
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <thread>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [M|S] [channel_type] [options]\n";
        std::cerr << "M (Master) or S (Slave)\n";
        std::cerr << "channel_type: TCP, SERIAL\n";
        std::cerr << "TCP options: <ip> <port> (for client) or <port> (for server)\n";
        std::cerr << "SERIAL options: <port_name> <baud_rate>\n";
        std::cerr << "\nExamples:\n";
        std::cerr << "  Modbus TCP Master: " << argv[0] << " M TCP 127.0.0.1 1502\n";
        std::cerr << "  Modbus TCP Slave:  " << argv[0] << " S TCP 1502\n";
        std::cerr << "  Modbus RTU Master: " << argv[0] << " M SERIAL COM1 9600\n";
        std::cerr << "  Modbus RTU Slave:  " << argv[0] << " S SERIAL COM1 9600\n";
        return 1;
    }

    std::string role = argv[1];
    std::string channel_type = (argc > 2) ? argv[2] : "";
    boost::asio::io_context io_context;

    try {
        if (role == "M") {
            // 主站模式
            if (channel_type == "TCP" && argc >= 5) {
                std::string ip = argv[3];
                uint16_t port = static_cast<uint16_t>(std::stoi(argv[4]));
                
                auto channel = std::make_shared<ChannelTcpClient>(io_context, ip, port);
                DriverModbusM master(channel);
                
                if (master.start()) {
                    std::cout << "Modbus TCP Master started" << std::endl;
                    
                    // 设置读取请求
                    DriverModbusM::ReadRequest request = {1, 0, 5, std::chrono::milliseconds(3000)};
                    master.setReadRequest(request);
                    
                    // 运行一段时间
                    for (int i = 0; i < 10; ++i) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        auto regs = master.getRegisters();
                        if (!regs.empty()) {
                            std::cout << "Registers: ";
                            for (auto reg : regs) {
                                std::cout << reg << " ";
                            }
                            std::cout << std::endl;
                        }
                    }
                    
                    master.stop();
                }
            } else if (channel_type == "SERIAL" && argc >= 5) {
                std::string port_name = argv[3];
                unsigned int baud_rate = std::stoi(argv[4]);
                
                auto channel = std::make_shared<ChannelSerial>(io_context, port_name, baud_rate);
                DriverModbusM master(channel);
                
                if (master.start()) {
                    std::cout << "Modbus RTU Master started" << std::endl;
                    
                    // 设置读取请求
                    DriverModbusM::ReadRequest request = {1, 0, 5, std::chrono::milliseconds(3000)};
                    master.setReadRequest(request);
                    
                    // 运行一段时间
                    for (int i = 0; i < 10; ++i) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        auto regs = master.getRegisters();
                        if (!regs.empty()) {
                            std::cout << "Registers: ";
                            for (auto reg : regs) {
                                std::cout << reg << " ";
                            }
                            std::cout << std::endl;
                        }
                    }
                    
                    master.stop();
                }
            } else {
                std::cerr << "Invalid arguments for Master mode" << std::endl;
                return 1;
            }
        } else if (role == "S") {
            // 从站模式
            if (channel_type == "TCP" && argc >= 4) {
                uint16_t port = static_cast<uint16_t>(std::stoi(argv[3]));
                
                auto channel = std::make_shared<ChannelTcpServer>(io_context, port);
                DriverModbusS slave(channel);
                
                // 设置寄存器读取回调
                slave.setReadHoldingRegistersCallback([](uint16_t start, uint16_t count) {
                    std::vector<uint16_t> values;
                    for (uint16_t i = 0; i < count; ++i) {
                        values.push_back(1000 + start + i); // 示例值
                    }
                    return values;
                });
                
                if (slave.start()) {
                    std::cout << "Modbus TCP Slave started on port " << port << std::endl;
                    
                    // 运行一段时间
                    std::this_thread::sleep_for(std::chrono::seconds(30));
                    
                    slave.stop();
                }
            } else if (channel_type == "SERIAL" && argc >= 5) {
                std::string port_name = argv[3];
                unsigned int baud_rate = std::stoi(argv[4]);
                
                auto channel = std::make_shared<ChannelSerial>(io_context, port_name, baud_rate);
                DriverModbusS slave(channel);
                
                // 设置寄存器读取回调
                slave.setReadHoldingRegistersCallback([](uint16_t start, uint16_t count) {
                    std::vector<uint16_t> values;
                    for (uint16_t i = 0; i < count; ++i) {
                        values.push_back(2000 + start + i); // 示例值
                    }
                    return values;
                });
                
                if (slave.start()) {
                    std::cout << "Modbus RTU Slave started on " << port_name 
                              << " at " << baud_rate << " baud" << std::endl;
                    
                    // 运行一段时间
                    std::this_thread::sleep_for(std::chrono::seconds(30));
                    
                    slave.stop();
                }
            } else {
                std::cerr << "Invalid arguments for Slave mode" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Invalid role. Use 'M' for Master or 'S' for Slave" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}