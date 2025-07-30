#pragma once

#include "ChannelBase.h"
#include <boost/asio/ip/tcp.hpp>

class ChannelTcpServer : public ChannelBase {
public:
    ChannelTcpServer(boost::asio::io_context& io_context, uint16_t port);
    ~ChannelTcpServer();
    
    bool start() override;
    void stop() override;
    bool send(const std::vector<uint8_t>& data) override;

private:
    void run_receive() override;
    void do_accept();
    void handle_connection(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    std::atomic<bool> connected_{false};
};