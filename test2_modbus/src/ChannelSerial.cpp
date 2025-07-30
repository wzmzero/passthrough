#include "ChannelSerial.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <stdexcept>
#include <iostream>

ChannelSerial::ChannelSerial(const std::string& port, int baudrate)
    : port(port), baudrate(baudrate) {}

ChannelSerial::~ChannelSerial() {
    stop();
}

bool ChannelSerial::start() {
    if (running) return true;
    
    serialFd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serialFd < 0) {
        perror("Error opening serial port");
        return false;
    }
    
    if (!configureSerialPort()) {
        close(serialFd);
        return false;
    }
    
    running = true;
    workerThread = std::thread(&ChannelSerial::serialThreadFunc, this);
    return true;
}

bool ChannelSerial::configureSerialPort() {
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serialFd, &tty) != 0) {
        perror("Error getting termios attributes");
        return false;
    }
    
    // 设置波特率
    speed_t speed;
    switch (baudrate) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        default: speed = B9600;
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);
    
    // 8N1配置
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;
    
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;
    
    tty.c_cc[VTIME] = 1; // 100ms超时
    tty.c_cc[VMIN] = 0;
    
    if (tcsetattr(serialFd, TCSANOW, &tty) != 0) {
        perror("Error setting termios attributes");
        return false;
    }
    
    return true;
}

void ChannelSerial::stop() {
    if (!running) return;
    
    running = false;
    if (workerThread.joinable()) {
        workerThread.join();
    }
    
    if (serialFd != -1) {
        close(serialFd);
        serialFd = -1;
    }
}

bool ChannelSerial::send(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(writeMutex);
    if (serialFd < 0) return false;
    
    ssize_t written = write(serialFd, data.data(), data.size());
    tcdrain(serialFd); // 等待所有数据发送完成
    
    return written == static_cast<ssize_t>(data.size());
}

void ChannelSerial::serialThreadFunc() {
    std::vector<uint8_t> buffer(256);
    
    while (running) {
        ssize_t n = read(serialFd, buffer.data(), buffer.size());
        if (n > 0) {
            if (receiveCallback) {
                receiveCallback(std::vector<uint8_t>(buffer.begin(), buffer.begin() + n));
            }
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Serial read error");
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
} 