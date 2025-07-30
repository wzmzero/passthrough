// include/ChannelTcpClient.hpp
#pragma once
#include "ChannelBase.hpp"
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

class ChannelTcpClient : public ChannelBase {
public:
    ChannelTcpClient(boost::asio::io_context& io,
                    const std::string& host, uint16_t port);
    ~ChannelTcpClient() override;

    void start() override;
    void stop() override;
    void send(const std::string& data) override;
    bool isRunning() const override { return m_isRunning; }
    bool isConnected() const { return m_isConnected; }

private:
    void startConnect();
    void startReceive();
    void reconnect();
    void resetConnection();
    
    std::string m_host;
    uint16_t m_port;
    bool m_isConnected = false;
    bool m_isRunning = false;
    bool m_isReconnecting = false;
    int m_reconnectAttempts = 0;
    std::chrono::seconds m_reconnectInterval = std::chrono::seconds(1);
    
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::steady_timer m_reconnectTimer;
    std::array<char, 1024> m_recvBuffer;
};