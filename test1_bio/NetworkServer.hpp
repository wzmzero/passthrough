#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <arpa/inet.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#define BUFFER_SIZE 1024

// 用于比较sockaddr_in的结构体
struct sockaddr_in_compare {
    bool operator()(const sockaddr_in& a, const sockaddr_in& b) const {
        if (a.sin_port != b.sin_port) return a.sin_port < b.sin_port;
        return a.sin_addr.s_addr < b.sin_addr.s_addr;
    }
};

class NetworkServer
{
public:
    using DataHandler = std::function<void(const std::string &protocol, const std::string &endpoint, const std::string &data)>;
    using TcpMessageHandler = std::function<bool(int clientSocket, const std::string& msg)>;
    using UdpMessageHandler = std::function<bool(const struct sockaddr_in& clientAddr, const std::string& msg)>;

    NetworkServer(int tcp_port, int udp_port);
    ~NetworkServer();

    void startTcpServer();
    void startUdpServer();
    void stop();

    void setDataHandler(DataHandler handler);
    void setTcpMessageHandler(TcpMessageHandler handler);
    void setUdpMessageHandler(UdpMessageHandler handler);
    
    void sendTcpMessage(int clientSocket, const std::string &msg);
    void sendUdpMessage(const struct sockaddr_in &clientAddr, const std::string &msg);
    void broadcastTcpMessage(const std::string &msg);
    void broadcastUdpMessage(const std::string &msg);
    std::vector<int> getTcpClients() const;
    std::vector<sockaddr_in> getUdpClients() const;

private:
    void acceptTcpClients();
    void handleTcpClient(int clientSocket, struct sockaddr_in clientAddr);
    void udpReceiveLoop();

    void onTcpMessageReceived(const std::string &msg, int clientSocket);
    void onUdpMessageReceived(const std::string &msg, const struct sockaddr_in &clientAddr);

    int tcpPort, udpPort;
    int tcpSocket = -1, udpSocket = -1;

    std::atomic<bool> running{false};
    std::atomic<bool> udpRunning{false};
    std::thread udpThread;

    DataHandler dataHandler = nullptr;
    TcpMessageHandler tcpMessageHandler = nullptr;
    UdpMessageHandler udpMessageHandler = nullptr;

    mutable std::mutex tcpClientsMutex;
    std::unordered_map<int, sockaddr_in> tcpClients;

    mutable std::mutex udpClientsMutex;
    std::set<sockaddr_in, sockaddr_in_compare> udpClients;

    std::mutex tcpCoutMutex;
    int tcpClientCount = 0;
};