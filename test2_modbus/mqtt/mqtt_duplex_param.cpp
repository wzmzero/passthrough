#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <mqtt/async_client.h>
#include <mqtt/callback.h>
#include <CLI/CLI.hpp>

using namespace std::chrono;

std::atomic<bool> running{true};

void signal_handler(int) {
    running = false;
}

// 修改1：继承自mqtt::callback的类
class client_callback : public virtual mqtt::callback {
public:
    explicit client_callback(std::atomic<bool>& running_flag) : running_(running_flag) {}

    void connection_lost(const std::string& cause) override {
        std::cerr << "Connection lost: " << cause << std::endl;
        running_ = false;
    }

private:
    std::atomic<bool>& running_;
};

int main(int argc, char** argv) {
    CLI::App app{"MQTT Async Publisher/Subscriber"};

    std::string mode;
    std::string topic;
    int interval = 0;
    std::string message;
    std::string server_address = "tcp://localhost:1883";

    app.add_option("mode", mode, "Operation mode (P for publish, S for subscribe)")
        ->required()
        ->check(CLI::IsMember({"P", "S"}));
    
    app.add_option("topic", topic, "MQTT topic name")
        ->required();
    
    app.add_option("interval", interval, "Publish interval in milliseconds (for publish mode)")
        ->needs("mode,P");
    
    app.add_option("message", message, "Message to publish (for publish mode)")
        ->needs("mode,P");
    
    // 修改2：修正CLI11参数添加方式
    app.add_option("-s,--server", server_address, "MQTT broker address")
        ->default_val("tcp://localhost:1883");

    CLI11_PARSE(app, argc, argv);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string client_id = "mqtt_client_" + std::to_string(time(nullptr));

    try {
        mqtt::async_client client(server_address, client_id);
        
        // 修改3：正确设置回调对象
        client_callback cb(running);
        client.set_callback(cb);

        mqtt::connect_options connOpts;
        connOpts.set_clean_session(true);
        connOpts.set_automatic_reconnect(true);

        std::cout << "Connecting to " << server_address << "..." << std::endl;
        client.connect(connOpts)->wait();
        std::cout << "Connected!" << std::endl;

        if (mode == "P") {
            std::cout << "Publishing to topic: " << topic 
                      << " every " << interval << "ms" << std::endl;
            
            while (running) {
                auto pubmsg = mqtt::make_message(topic, message);
                client.publish(pubmsg)->wait();
                std::this_thread::sleep_for(milliseconds(interval));
            }
        } else if (mode == "S") {
            client.subscribe(topic, 1)->wait();
            std::cout << "Subscribed to topic: " << topic << std::endl;
            
            // 修改4：使用成员函数设置消息回调
            client.set_message_callback([&](mqtt::const_message_ptr msg) {
                std::cout << "[" << msg->get_topic() << "]: " 
                          << msg->to_string() << std::endl;
            });
            
            while (running) {
                std::this_thread::sleep_for(milliseconds(500));
            }
        }
        
        std::cout << "Disconnecting..." << std::endl;
        client.disconnect()->wait();
        std::cout << "Disconnected" << std::endl;
    }
    catch (const mqtt::exception& exc) {
        std::cerr << "MQTT Error: " << exc.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}