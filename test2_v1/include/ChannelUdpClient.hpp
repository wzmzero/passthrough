// include/ChannelUdpClient.hpp
#pragma once
#include "ChannelBase.hpp"
#include <boost/asio.hpp>
#include <optional>

class ChannelUdpClient : public ChannelBase {
public:
    ChannelUdpClient(boost::asio::io_context& io,
                    const std::string& host, uint16_t port);
    ~ChannelUdpClient() override;
    
    void start() override;
    void stop() override;
    void send(const std::string& data) override;
    bool isRunning() const override { return m_isRunning; }
    
private:
    void startReceive();
    void resolveEndpoint();
    
    std::string m_host;
    uint16_t m_port;
    bool m_isRunning = false;
    boost::asio::ip::udp::socket m_socket;
    std::optional<boost::asio::ip::udp::endpoint> m_serverEndpoint;
    boost::asio::ip::udp::endpoint m_remoteEndpoint;
 
};