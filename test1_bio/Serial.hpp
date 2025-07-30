#pragma once
#include <string>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <functional>

class Serial {
public:
    using DataCallback = std::function<void(const std::string&)>;
    
    Serial();
    ~Serial();
    
    bool open(const std::string& port, int baud_rate);
    void close();
    bool isOpen() const;
    bool write(const std::string& data);
    
    void setDataCallback(DataCallback callback);
    void startReading();
    void stopReading();

private:
    void readThreadFunc();

    int fd_ = -1;
    std::string port_;
    int baud_rate_;
    std::thread read_thread_;
    std::atomic<bool> running_{false};
    DataCallback data_callback_;
};