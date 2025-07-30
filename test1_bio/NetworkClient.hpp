#pragma once
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
class NetworkClient {
public:
    enum Protocol { TCP, UDP };
    
    NetworkClient(Protocol protocol, const std::string& server_ip, int port);
    ~NetworkClient();
    
    bool connect();
 
    bool sendMessage(const std::string& message);
    std::string receiveMessage();
    void closeConnection();

    Protocol getProtocol() const { return protocol; }

private:
    Protocol protocol;
    std::string serverIP;
    int serverPort;
    int sock = -1;
    struct sockaddr_in serverAddr;
    static const int BUFFER_SIZE = 1024;
};