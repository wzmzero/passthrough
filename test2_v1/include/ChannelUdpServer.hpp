// include/ChannelUdpServer.hpp
#pragma once
#include "ChannelBase.hpp"
#include <boost/asio.hpp>
#include <set>
#include <mutex>

class ChannelUdpServer : public ChannelBase {
public:
    ChannelUdpServer(boost::asio::io_context& io, uint16_t port);
    ~ChannelUdpServer() override;
    
    void start() override;
    void stop() override;
    void send(const std::string& data) override;
    bool isRunning() const override { return m_isRunning; }
    
private:
    void startReceive();
    void addClientEndpoint(const boost::asio::ip::udp::endpoint& endpoint);
    
    boost::asio::ip::udp::socket m_socket;
    boost::asio::ip::udp::endpoint m_remoteEndpoint;
    bool m_isRunning = false;
    std::set<boost::asio::ip::udp::endpoint> m_clientEndpoints;
    std::mutex m_endpointsMutex;
    std::array<char, 1024> m_recvBuffer;
};