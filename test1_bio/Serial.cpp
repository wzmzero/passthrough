#include "Serial.hpp"
#include <iostream>
#include <cstring>
#include <system_error>

Serial::Serial() {}

Serial::~Serial() {
    stopReading();
    close();
}

bool Serial::open(const std::string& port, int baud_rate) {
    if (isOpen()) close();
    
    fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        std::cerr << "Error opening serial port " << port << ": " 
                  << std::strerror(errno) << std::endl;
        return false;
    }
    
    port_ = port;
    baud_rate_ = baud_rate;
    
    struct termios tty;
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "Error getting serial attributes: " 
                  << std::strerror(errno) << std::endl;
        close();
        return false;
    }
    
    // 设置波特率
    speed_t speed;
    switch (baud_rate) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
        case 921600: speed = B921600; break;
        default: 
            std::cerr << "Unsupported baud rate: " << baud_rate << std::endl;
            close();
            return false;
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);
    
    // 配置串口参数
    tty.c_cflag &= ~PARENB; // 无奇偶校验
    tty.c_cflag &= ~CSTOPB; // 1位停止位
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;     // 8位数据位
    
    tty.c_cflag &= ~CRTSCTS; // 禁用硬件流控
    tty.c_cflag |= CREAD | CLOCAL; // 启用接收，忽略modem控制线
    
    tty.c_lflag &= ~ICANON; // 非规范模式
    tty.c_lflag &= ~ECHO;   // 禁用回显
    tty.c_lflag &= ~ECHOE;  // 禁用擦除
    tty.c_lflag &= ~ECHONL; // 禁用换行回显
    tty.c_lflag &= ~ISIG;   // 禁用中断信号
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // 禁用软件流控
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    
    tty.c_oflag &= ~OPOST; // 原始输出
    tty.c_oflag &= ~ONLCR; // 不转换换行符
    
    // 设置超时 - 等待数据，最多等待0.1秒
    tty.c_cc[VTIME] = 1; // 0.1秒超时
    tty.c_cc[VMIN] = 0;
    
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "Error setting serial attributes: " 
                  << std::strerror(errno) << std::endl;
        close();
        return false;
    }
    
    std::cout << "Serial port " << port << " opened at " << baud_rate << " baud" << std::endl;
    return true;
}

void Serial::close() {
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
        std::cout << "Serial port " << port_ << " closed" << std::endl;
    }
}

bool Serial::isOpen() const {
    return fd_ != -1;
}

bool Serial::write(const std::string& data) {
    if (!isOpen()) return false;
    
    ssize_t n = ::write(fd_, data.c_str(), data.size());
    if (n < 0) {
        std::cerr << "Error writing to serial port: " 
                  << std::strerror(errno) << std::endl;
        return false;
    }
    return n == static_cast<ssize_t>(data.size());
}

void Serial::setDataCallback(DataCallback callback) {
    data_callback_ = callback;
}

void Serial::startReading() {
    if (!isOpen() || running_) return;
    
    running_ = true;
    read_thread_ = std::thread(&Serial::readThreadFunc, this);
}

void Serial::stopReading() {
    if (running_) {
        running_ = false;
        if (read_thread_.joinable()) {
            read_thread_.join();
        }
    }
}

void Serial::readThreadFunc() {
    char buffer[1024];
    
    while (running_) {
        ssize_t n = ::read(fd_, buffer, sizeof(buffer));
        if (n > 0) {
            if (data_callback_) {
                data_callback_(std::string(buffer, n));
            }
        } else if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "Serial read error: " << std::strerror(errno) << std::endl;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}