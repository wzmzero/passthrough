 
#pragma once
#include "Endpoint.hpp"
#include "SerialEndpoint.hpp"
#include "TcpClientEndpoint.hpp"
#include "TcpServerEndpoint.hpp"
#include "UdpClientEndpoint.hpp"
#include "UdpServerEndpoint.hpp"
#include <nlohmann/json.hpp>
#include <memory>

class EndpointFactory {
public:
    static std::unique_ptr<Endpoint> createEndpoint(Endpoint::Type type, const nlohmann::json& config) {
        switch (type) {
            case Endpoint::SERIAL:
                return createSerialEndpoint(config);
            case Endpoint::TCP_CLIENT:
                return createTcpClientEndpoint(config);
            case Endpoint::TCP_SERVER:
                return createTcpServerEndpoint(config);
            case Endpoint::UDP_CLIENT:
                return createUdpClientEndpoint(config);
            case Endpoint::UDP_SERVER:
                return createUdpServerEndpoint(config);
            default:
                return nullptr;
        }
    }

private:
    static std::unique_ptr<Endpoint> createSerialEndpoint(const nlohmann::json& config) {
        try {
            return std::make_unique<SerialEndpoint>(
                config["port"].get<std::string>(),
                config["baud_rate"].get<int>()
            );
        } catch (const std::exception& e) {
            std::cerr << "Error creating serial endpoint: " << e.what() << std::endl;
            return nullptr;
        }
    }
    
    static std::unique_ptr<Endpoint> createTcpClientEndpoint(const nlohmann::json& config) {
        try {
            return std::make_unique<TcpClientEndpoint>(
                config["server_ip"].get<std::string>(),
                config["server_port"].get<int>()
            );
        } catch (const std::exception& e) {
            std::cerr << "Error creating TCP client endpoint: " << e.what() << std::endl;
            return nullptr;
        }
    }
    
    static std::unique_ptr<Endpoint> createTcpServerEndpoint(const nlohmann::json& config) {
        try {
            return std::make_unique<TcpServerEndpoint>(
                config["listen_port"].get<int>()
            );
        } catch (const std::exception& e) {
            std::cerr << "Error creating TCP server endpoint: " << e.what() << std::endl;
            return nullptr;
        }
    }
    
    static std::unique_ptr<Endpoint> createUdpClientEndpoint(const nlohmann::json& config) {
        try {
            return std::make_unique<UdpClientEndpoint>(
                config["server_ip"].get<std::string>(),
                config["server_port"].get<int>()
            );
        } catch (const std::exception& e) {
            std::cerr << "Error creating UDP client endpoint: " << e.what() << std::endl;
            return nullptr;
        }
    }
    
    static std::unique_ptr<Endpoint> createUdpServerEndpoint(const nlohmann::json& config) {
        try {
            return std::make_unique<UdpServerEndpoint>(
                config["listen_port"].get<int>()
            );
        } catch (const std::exception& e) {
            std::cerr << "Error creating UDP server endpoint: " << e.what() << std::endl;
            return nullptr;
        }
    }
};  