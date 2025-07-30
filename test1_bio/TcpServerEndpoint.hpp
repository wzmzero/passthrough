#pragma once
#include "BaseEndpoint.hpp"
#include "NetworkServer.hpp"
#include <thread>
#include <atomic>
#include <mutex>
#include <map>

class TcpServerEndpoint : public BaseEndpoint {
public:
    TcpServerEndpoint(int port);
    ~TcpServerEndpoint();
    
    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool write(const std::string& data) override;
    void start() override;
    void stop() override;
    
    Type getType() const override { return TCP_SERVER; }
    std::string getConfigString() const override;
    
    void broadcast(const std::string& data);
    bool sendToClient(int client_id, const std::string& data);
    
private:
    void serverThreadFunc();
    void clientHandler(int client_socket);
    void onClientMessage(int client_socket, const std::string& data);
    
    int port_;
    std::unique_ptr<NetworkServer> server_;
    std::thread server_thread_;
    std::map<int, std::thread> client_threads_;
    std::mutex clients_mutex_;
    std::atomic<int> next_client_id_{1};
};