#include "NetworkServer.hpp"
#include "ConfigReader.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <sstream>
#include <vector>
#include <arpa/inet.h>

std::atomic<bool> running(true);

void signalHandler(int signal) {
    running = false;
}

void printUdpClient(const sockaddr_in& clientAddr) {
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), ipStr, INET_ADDRSTRLEN);
    std::cout << "  " << ipStr << ":" << ntohs(clientAddr.sin_port) << "\n";
}

void serverSender(NetworkServer& server) {
    std::string input;
    while (running) {
        std::cout << "[Input] Enter message to send (or 'list' to show clients): ";
        if (!std::getline(std::cin, input)) break;
        
        if (input == "list") {
            // 显示TCP客户端
            auto tcpClients = server.getTcpClients();
            std::cout << "Connected TCP clients (" << tcpClients.size() << "):\n";
            for (int client : tcpClients) {
                std::cout << "  Socket: " << client << "\n";
            }
            
            // 显示UDP客户端
            auto udpClients = server.getUdpClients();
            std::cout << "Known UDP clients (" << udpClients.size() << "):\n";
            for (const auto& client : udpClients) {
                printUdpClient(client);
            }
            continue;
        }
        
        // 广播到所有TCP客户端
        server.broadcastTcpMessage(input);
        
        // 广播到所有UDP客户端
        server.broadcastUdpMessage(input);
    }
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    int tcp_port = 0;
    int udp_port = 0;
    bool update_database = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --tcp_port <port>    Set TCP port and update database\n"
                      << "  --udp_port <port>    Set UDP port and update database\n"
                      << "  -h, --help           Show this help message\n";
            return 0;
        } 
        else if (strcmp(argv[i], "--tcp_port") == 0 && i + 1 < argc) {
            tcp_port = std::atoi(argv[++i]);
            update_database = true;
        }
        else if (strcmp(argv[i], "--udp_port") == 0 && i + 1 < argc) {
            udp_port = std::atoi(argv[++i]);
            update_database = true;
        }
        else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            std::cerr << "Use --help for usage information" << std::endl;
            return 1;
        }
    }
    
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    ConfigReader configReader("config.db");
    
    // 读取TCP服务器配置
    ConfigReader::ServerConfig tcpConfig;
    bool tcp_config_ok = configReader.readServerConfig(ConfigReader::TCP, tcpConfig);
    
    // 读取UDP服务器配置
    ConfigReader::ServerConfig udpConfig;
    bool udp_config_ok = configReader.readServerConfig(ConfigReader::UDP, udpConfig);
    // 应用命令行参数
    if (tcp_port > 0) {
        tcpConfig.listen_port = tcp_port;
        if (update_database) {
            configReader.writeServerConfig(ConfigReader::TCP, tcpConfig);
            std::cout << "Updated TCP port in database: " << tcp_port << std::endl;
        }
    } else if (!tcp_config_ok) {
        std::cerr << "Using fallback configuration for TCP: port 8080\n";
        tcpConfig.listen_port = 8080;
    }
    
    if (udp_port > 0) {
        udpConfig.listen_port = udp_port;
        if (update_database) {
            configReader.writeServerConfig(ConfigReader::UDP, udpConfig);
            std::cout << "Updated UDP port in database: " << udp_port << std::endl;
        }
    } else if (!udp_config_ok) {
        std::cerr << "Using fallback configuration for UDP: port 8080\n";
        udpConfig.listen_port = 8080;
    }
    
    std::cout << "Starting server with configuration:\n"
              << "  TCP port: " << tcpConfig.listen_port << "\n"
              << "  UDP port: " << udpConfig.listen_port << std::endl;
    
    // 启动网络服务器
    NetworkServer server(tcpConfig.listen_port, udpConfig.listen_port);

    server.setDataHandler([](const std::string& protocol,
                             const std::string& endpoint,
                             const std::string& data) {
        std::cout << "[" << protocol << "] From " << endpoint << ": " << data << std::endl;
    });

    server.startTcpServer();
    server.startUdpServer();

    std::thread senderThread(serverSender, std::ref(server));

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server.stop();
    if (senderThread.joinable()) senderThread.join();
    std::cout << "Server stopped.\n";
    return 0;
}