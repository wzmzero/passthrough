#include "NetworkServer.hpp"
#include <iostream>
#include <algorithm>

NetworkServer::NetworkServer(int tcp_port, int udp_port)
    : tcpPort(tcp_port), udpPort(udp_port) {}

NetworkServer::~NetworkServer() {
    stop();
}

void NetworkServer::stop() {
    running = false;
    udpRunning = false;

    if (tcpSocket != -1) {
        close(tcpSocket);
        tcpSocket = -1;
    }
    if (udpSocket != -1) {
        close(udpSocket);
        udpSocket = -1;
    }
    
    // Close all TCP client sockets
    {
        std::lock_guard<std::mutex> lock(tcpClientsMutex);
        for (auto& client : tcpClients) {
            close(client.first);
        }
        tcpClients.clear();
    }

    // Clear UDP clients
    {
        std::lock_guard<std::mutex> lock(udpClientsMutex);
        udpClients.clear();
    }

    if (udpThread.joinable()) {
        udpThread.join();
    }
}

void NetworkServer::startTcpServer() {
    tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket < 0) {
        std::cerr << "TCP socket creation failed\n";
        return;
    }
    int opt = 1;
    setsockopt(tcpSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(tcpPort);

    if (bind(tcpSocket, (sockaddr*)&addr, sizeof(addr)) < 0 || listen(tcpSocket, 10) < 0) {
        std::cerr << "TCP bind/listen failed\n";
        close(tcpSocket);
        tcpSocket = -1;
        return;
    }

    running = true;
    std::thread(&NetworkServer::acceptTcpClients, this).detach();
    std::cout << "[TCP] Server started on port " << tcpPort << "\n";
}

void NetworkServer::startUdpServer() {
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket < 0) {
        std::cerr << "UDP socket creation failed\n";
        return;
    }

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(udpPort);

    if (bind(udpSocket, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        std::cerr << "UDP bind failed\n";
        close(udpSocket);
        udpSocket = -1;
        return;
    }

    udpRunning = true;
    udpThread = std::thread(&NetworkServer::udpReceiveLoop, this);
    std::cout << "[UDP] Server started on port " << udpPort << "\n";
}

void NetworkServer::acceptTcpClients() {
    while (running) {
        sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);
        int clientSock = accept(tcpSocket, (sockaddr*)&clientAddr, &len);
        if (clientSock >= 0) {
            {
                std::lock_guard<std::mutex> lock(tcpClientsMutex);
                tcpClients[clientSock] = clientAddr;
            }
            std::thread(&NetworkServer::handleTcpClient, this, clientSock, clientAddr).detach();
        }
    }
}

void NetworkServer::handleTcpClient(int clientSocket, sockaddr_in clientAddr) {
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
    int port = ntohs(clientAddr.sin_port);

    {
        std::lock_guard<std::mutex> lock(tcpCoutMutex);
        std::cout << "[TCP] Connected: " << ipStr << ":" << port << "\n";
        tcpClientCount++;
    }

    char buffer[BUFFER_SIZE];
    while (running) {
        int bytes = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        onTcpMessageReceived(std::string(buffer), clientSocket);
    }

    close(clientSocket);
    {
        std::lock_guard<std::mutex> lock(tcpClientsMutex);
        tcpClients.erase(clientSocket);
    }
    {
        std::lock_guard<std::mutex> lock(tcpCoutMutex);
        tcpClientCount--;
        std::cout << "[TCP] Disconnected: " << ipStr << ":" << port << "\n";
    }
}

void NetworkServer::udpReceiveLoop() {
    char buffer[BUFFER_SIZE];
    sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);

    while (udpRunning) {
        int n = recvfrom(udpSocket, buffer, sizeof(buffer) - 1, 0,
                         (sockaddr*)&cliaddr, &len);
        if (n > 0) {
            buffer[n] = '\0';
            
            // 记录UDP客户端地址
            {
                std::lock_guard<std::mutex> lock(udpClientsMutex);
                udpClients.insert(cliaddr);
            }
            
            onUdpMessageReceived(std::string(buffer), cliaddr);
        }
    }
}
void NetworkServer::onTcpMessageReceived(const std::string& msg, int clientSocket) {
    // 调用数据处理器（用于打印）
    if (dataHandler) {
        char ipStr[INET_ADDRSTRLEN];
        sockaddr_in addr;
        {
            std::lock_guard<std::mutex> lock(tcpClientsMutex);
            auto it = tcpClients.find(clientSocket);
            if (it != tcpClients.end()) {
                addr = it->second;
            } else {
                // Client not found, can't get endpoint
                return;
            }
        }
        inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr));
        std::string endpoint = std::string(ipStr) + ":" + std::to_string(ntohs(addr.sin_port));
        dataHandler("TCP", endpoint, msg);
    }
    
    // 调用自定义TCP消息处理器（如果设置）
    bool handled = false;
    if (tcpMessageHandler) {
        handled = tcpMessageHandler(clientSocket, msg);
    }
    
    // 如果自定义处理器没有处理消息，则执行默认行为
    if (!handled) {
        std::string response = "[Echo] " + msg;
        send(clientSocket, response.c_str(), response.size(), 0);
    }
}

void NetworkServer::onUdpMessageReceived(const std::string& msg, const sockaddr_in& clientAddr) {
    // 调用数据处理器（用于打印）
    if (dataHandler) {
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
        std::string endpoint = std::string(ipStr) + ":" + std::to_string(ntohs(clientAddr.sin_port));
        dataHandler("UDP", endpoint, msg);
    }
    
    // 调用自定义UDP消息处理器（如果设置）
    bool handled = false;
    if (udpMessageHandler) {
        handled = udpMessageHandler(clientAddr, msg);
    }
    
    // 如果自定义处理器没有处理消息，则执行默认行为
    if (!handled) {
        std::string response = "[Echo] " + msg;
        sendto(udpSocket, response.c_str(), response.size(), 0,
               (sockaddr*)&clientAddr, sizeof(clientAddr));
    }
}

void NetworkServer::setDataHandler(DataHandler handler) {
    dataHandler = handler;
}

void NetworkServer::setTcpMessageHandler(TcpMessageHandler handler) {
    tcpMessageHandler = handler;
}

void NetworkServer::setUdpMessageHandler(UdpMessageHandler handler) {
    udpMessageHandler = handler;
}

void NetworkServer::sendTcpMessage(int clientSocket, const std::string& msg) {
    send(clientSocket, msg.c_str(), msg.size(), 0);
}

void NetworkServer::sendUdpMessage(const struct sockaddr_in& clientAddr, const std::string& msg) {
    sendto(udpSocket, msg.c_str(), msg.size(), 0,
           (sockaddr*)&clientAddr, sizeof(clientAddr));
}

void NetworkServer::broadcastTcpMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(tcpClientsMutex);
    for (auto& client : tcpClients) {
        send(client.first, msg.c_str(), msg.size(), 0);
    }
}

void NetworkServer::broadcastUdpMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(udpClientsMutex);
    for (const auto& clientAddr : udpClients) {
        sendto(udpSocket, msg.c_str(), msg.size(), 0,
               (sockaddr*)&clientAddr, sizeof(clientAddr));
    }
}

std::vector<int> NetworkServer::getTcpClients() const {
    std::vector<int> clients;
    std::lock_guard<std::mutex> lock(tcpClientsMutex);
    for (const auto& client : tcpClients) {
        clients.push_back(client.first);
    }
    return clients;
}

std::vector<sockaddr_in> NetworkServer::getUdpClients() const {
    std::vector<sockaddr_in> clients;
    std::lock_guard<std::mutex> lock(udpClientsMutex);
    for (const auto& clientAddr : udpClients) {
        clients.push_back(clientAddr);
    }
    return clients;
}