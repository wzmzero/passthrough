// ChannelTcpServer.cpp
#include "ChannelTcpServer.hpp"
#include <iostream>

// ------------------- Session Implementation -------------------
Session::Session(boost::asio::io_context& io, ChannelTcpServer& server)
    : ChannelBase(io), m_socket(std::make_shared<boost::asio::ip::tcp::socket>(io)),
      m_server(server){}
Session::~Session() {
    stop();  
}
void Session::start() {
    try {
        log(LogLevel::INFO, "Client connected: " + 
            m_socket->remote_endpoint().address().to_string());
        startReceive();
    } catch (const boost::system::system_error& e) {
       log(LogLevel::ERROR, "Session error: " + std::string(e.what()));
    }
}
void Session::stop() {
    if (!m_isRunning) return;
    m_isRunning = false;

    boost::system::error_code ec;
    // 安全关闭套接字
    if (m_socket->is_open()) {
        m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::asio::error::not_connected) {
            log(LogLevel::WARNING, "Socket shutdown warning: " + ec.message());
        }
        m_socket->close(ec);
        if (ec) {
            log(LogLevel::ERROR, "Socket close error: " + ec.message());
        }
    }
}
void Session::send(const std::string& data) {
    if (!m_socket->is_open()) return;
    
    auto self = shared_from_this();
    auto buffer = std::make_shared<std::string>(data);
    boost::asio::async_write(*m_socket, boost::asio::buffer(*buffer),
        [self, buffer](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                self->log(LogLevel::ERROR, "Send failed: " + ec.message());
            }
        });
}

void Session::startReceive() {
    auto self = shared_from_this();
    m_socket->async_read_some(boost::asio::buffer(m_recvBuffer),
        [self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            self->handleReceive(ec, bytes_transferred);
        });
}

void Session::handleReceive(const boost::system::error_code& ec, 
                            std::size_t bytes_transferred) {
    if (ec) {
        if (ec == boost::asio::error::eof) {
           log(LogLevel::INFO, "Client disconnected");
        } else if (ec != boost::asio::error::operation_aborted) {
            log(LogLevel::ERROR, "Receive error: " + ec.message());
        }
        stop(); // 停止当前会话
        m_server.removeSession(this);
        return;
    }
        if (receiveCallback) {
            receiveCallback(std::string(m_recvBuffer.data(), bytes_transferred));
        startReceive();
    }
}

// ------------------- Server Implementation -------------------
ChannelTcpServer::ChannelTcpServer(boost::asio::io_context& io, 
                                   const std::string& host, uint16_t port)
    : ChannelBase(io),
      m_acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(host), port)) {}

ChannelTcpServer::~ChannelTcpServer() {
    stop();
}

void ChannelTcpServer::start() {
    if (m_isRunning) return;
    m_isRunning = true;
    startAccept();
    log(LogLevel::INFO, "TCP server started");
}

void ChannelTcpServer::stop() {
    if (!m_isRunning) return;
    m_isRunning = false;
    
    boost::system::error_code ec;
    m_acceptor.close(ec);
    if (ec) {
        log(LogLevel::ERROR, "Acceptor close error: " + ec.message());
    }
    
    // 创建临时副本避免迭代器失效
    auto sessions = m_sessions;
    for (auto& session : sessions) {
        if (session->socket().is_open()) {
            session->socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            session->socket().close();
        }
    }
    m_sessions.clear();
    log(LogLevel::INFO, "TCP server stopped");
}

void ChannelTcpServer::send(const std::string& data) {
    if (m_sessions.empty()) {
        log(LogLevel::WARNING, "No clients connected, cannot send data");
        return;
    }
    
    // 创建临时副本避免迭代器失效
    auto sessions = m_sessions;
    for (auto& session : sessions) {
        session->send(data);
    }
}

void ChannelTcpServer::removeSession(Session* session) {
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
        [session](const Session::SessionPtr& ptr) {
            return ptr.get() == session;
        });
    
    if (it != m_sessions.end()) {
        m_sessions.erase(it);
        log(LogLevel::INFO, "Session removed, active sessions: " + std::to_string(m_sessions.size()));
    }
}

void ChannelTcpServer::startAccept() {
    if (!m_isRunning) return;
    
    auto new_session = std::make_shared<Session>(ioContext, *this);
    m_acceptor.async_accept(new_session->socket(),
        [this, new_session](const boost::system::error_code& ec) {
            handleAccept(ec, new_session);
        });
}

void ChannelTcpServer::handleAccept(const boost::system::error_code& ec, 
                                    Session::SessionPtr session) {
    if (!m_isRunning) return;
    
    if (!ec) {
        m_sessions.insert(session);
        session->start();
        log(LogLevel::INFO, "New connection accepted, active sessions: " + 
            std::to_string(m_sessions.size()));
    } else if (ec != boost::asio::error::operation_aborted) {
        log(LogLevel::ERROR, "Accept error: " + ec.message());
    }
    
    if (m_isRunning) {
        startAccept();
    }
}