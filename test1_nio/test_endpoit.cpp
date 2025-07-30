#include <iostream>
#include <memory>
#include <string>
#include "tcp_server_endpoint.h"
#include "tcp_client_endpoint.h"
#include "udp_server_endpoint.h"
#include "udp_client_endpoint.h"
#include "serial_endpoint.h"

void dataCallback(const uint8_t* data, size_t len) {
    std::cout << "Received " << len << " bytes: ";
    for (size_t i = 0; i < len && i < 20; ++i) {
        printf("%02X ", data[i]);
    }
    if (len > 20) std::cout << "...";
    std::cout << std::endl;
}

void logCallback(const std::string& msg) {
    std::cout << msg << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <type> [options]\n"
                  << "Types:\n"
                  << "  tcp_server <port>\n"
                  << "  tcp_client <ip> <port>\n"
                  << "  udp_server <port>\n"
                  << "  udp_client <ip> <port>\n"
                  << "  serial <device> <baud>\n";
        return 1;
    }

    std::unique_ptr<Endpoint> endpoint;
    std::string type = argv[1];
    
    try {
        if (type == "tcp_server" && argc == 3) {
            endpoint = std::make_unique<TcpServerEndpoint>(std::stoi(argv[2]));
        } else if (type == "tcp_client" && argc == 4) {
            endpoint = std::make_unique<TcpClientEndpoint>(argv[2], std::stoi(argv[3]));
        } else if (type == "udp_server" && argc == 3) {
            endpoint = std::make_unique<UdpServerEndpoint>(std::stoi(argv[2]));
        } else if (type == "udp_client" && argc == 4) {
            endpoint = std::make_unique<UdpClientEndpoint>(argv[2], std::stoi(argv[3]));
        } else if (type == "serial" && argc == 4) {
            endpoint = std::make_unique<SerialEndpoint>(argv[2], std::stoi(argv[3]));
        } else {
            std::cerr << "Invalid arguments\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    endpoint->setReceiveCallback(dataCallback);
    endpoint->setLogCallback(logCallback);
    
    // Start the endpoint
    if (!endpoint->open()) {
        std::cerr << "Failed to start endpoint" << std::endl;
        return 1;
    }

    std::cout << "Endpoint running. Type 'exit' to quit, 'restart' to restart connection.\n";
    
    // User input handling
    std::string input;
    while (true) {
        std::getline(std::cin, input);
        
        if (input == "exit") {
            break;
        } else if (input == "restart") {
            endpoint->restart();
        } else {
            // Send the input text
            endpoint->send(reinterpret_cast<const uint8_t*>(input.data()), input.size());
        }
    }

    endpoint->close();
    return 0;
}