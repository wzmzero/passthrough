#pragma once

#include "ChannelBase.h"
#include <boost/asio/ip/tcp.hpp>

class ChannelTcpClient : public ChannelBase {
public:
    ChannelTcpClient(boost::asio::io_context& io_context, 
                     const std::string& ip, 
                     uint16_t port);
    ~ChannelTcpClient();
    
    bool start() override;
    void stop() override;
    bool send(const std::vector<uint8_t>& data) override;

private:
    void run_receive() override;
    void do_connect();
    void handle_connect(const boost::system::error_code& ec);
    void handle_read(const boost::system::error_code& ec, size_t bytes_transferred);

    std::string ip_;
    uint16_t port_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::ip::tcp::resolver resolver_;
    std::array<uint8_t, 128> read_buffer_;
    std::atomic<bool> connected_{false};
};