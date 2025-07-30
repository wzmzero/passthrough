// ChannelTcpServer.hpp
#pragma once
#include "ChannelBase.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <unordered_set>

// 前向声明
class ChannelTcpServer;

class Session : public std::enable_shared_from_this<Session> {
public:
    using SessionPtr = std::shared_ptr<Session>;
    using SessionSocketPtr = std::shared_ptr<boost::asio::ip::tcp::socket>;

    Session(boost::asio::io_context& io, ChannelTcpServer& server);
    void start();
    void send(const std::string& data);
    boost::asio::ip::tcp::socket& socket() { return *m_socket; }

private:
    void startReceive();
    void handleReceive(const boost::system::error_code& ec, std::size_t bytes_transferred);

    SessionSocketPtr m_socket;
    ChannelTcpServer& m_server;
    std::vector<char> m_recvBuffer;
};

class ChannelTcpServer : public ChannelBase {
public:
    ChannelTcpServer(boost::asio::io_context& io, const std::string& host, uint16_t port);
    ~ChannelTcpServer();

    void start() override;
    void stop() override;
    void send(const std::string& data) override;
    bool isRunning() const override { return m_isRunning; }  // 实现纯虚函数
    
    void removeSession(Session* session);

private:
    void startAccept();
    void handleAccept(const boost::system::error_code& ec, Session::SessionPtr session);

    boost::asio::ip::tcp::acceptor m_acceptor;
    std::unordered_set<Session::SessionPtr> m_sessions;
    bool m_isRunning = false;
};