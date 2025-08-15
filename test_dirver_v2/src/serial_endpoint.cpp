#include "serial_endpoint.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <stdexcept>
#include <asm/termbits.h>
#include <sys/ioctl.h>

SerialEndpoint::SerialEndpoint(const std::string& device, int baudrate)
    : _device(device), _baudrate(baudrate) {}

SerialEndpoint::~SerialEndpoint() {
    close();
}

bool SerialEndpoint::open() {
    if (isRunning()) return true;
    
    // 打开串口设备
    _serialFd = ::open(_device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (_serialFd < 0) {
        logError("Open serial failed: " + std::string(strerror(errno)));
        return false;
    }
    
    // 配置串口参数
    if (!configureSerialPort()) {
        ::close(_serialFd);
        _serialFd = -1;
        return false;
    }
    
    // 创建epoll实例
    _epollFd = epoll_create1(0);
    if (_epollFd < 0) {
        logError("Epoll creation failed: " + std::string(strerror(errno)));
        ::close(_serialFd);
        _serialFd = -1;
        return false;
    }
    
    // 添加串口到epoll
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = _serialFd;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, _serialFd, &event) < 0) {
        logError("Epoll_ctl failed: " + std::string(strerror(errno)));
        ::close(_epollFd);
        ::close(_serialFd);
        _epollFd = -1;
        _serialFd = -1;
        return false;
    }
    
    setState(State::CONNECTED);
    startThread();
    return true;
}

void SerialEndpoint::close() {
    stopThread();
    
    if (_epollFd >= 0) {
        ::close(_epollFd);
        _epollFd = -1;
    }
    
    if (_serialFd >= 0) {
        ::close(_serialFd);
        _serialFd = -1;
    }
    
    setState(State::DISCONNECTED);
}

void SerialEndpoint::write(const uint8_t* data, size_t len) {
    if (!isConnected()) return;
    
    std::lock_guard<std::mutex> lock(_mutex);
    if (::write(_serialFd, data, len) < 0) {
        logError("Serial write failed: " + std::string(strerror(errno)));
        setState(State::ERROR);
    }
}

bool SerialEndpoint::configureSerialPort() {
    termios2 tty{};
    if (ioctl(_serialFd, TCGETS2, &tty) != 0) {
        logError("Get serial config failed: " + std::string(strerror(errno)));
        return false;
    }
    
    // 设置波特率
    tty.c_cflag &= ~CBAUD;
    tty.c_cflag |= BOTHER;
    tty.c_ispeed = _baudrate;
    tty.c_ospeed = _baudrate;
    
    // 8位数据位，无校验，1位停止位
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    
    // 禁用硬件流控
    tty.c_cflag &= ~CRTSCTS;
    
    // 开启接收
    tty.c_cflag |= CREAD | CLOCAL;
    
    // 禁用软件流控
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    
    // 原始输入模式
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    
    // 特殊字符处理
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10; // 1秒超时
    
    if (ioctl(_serialFd, TCSETS2, &tty) != 0) {
        logError("Set serial config failed: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

void SerialEndpoint::run() {
    epoll_event events[2];
    while (isRunning()) {
        int numEvents = epoll_wait(_epollFd, events, 2, 100);
        if (numEvents < 0) {
            if (errno != EINTR) {
                logError("Epoll_wait error: " + std::string(strerror(errno)));
            }
            continue;
        }
        
        for (int i = 0; i < numEvents; ++i) {
            if (events[i].events & EPOLLIN) {
                handleSerialData();
            }
        }
    }
}

void SerialEndpoint::handleSerialData() {
    uint8_t buffer[256];
    ssize_t bytesRead = read(_serialFd, buffer, sizeof(buffer));
    
    if (bytesRead > 0) {
        processData(buffer, bytesRead);
    } else if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        logError("Serial read error: " + std::string(strerror(errno)));
        setState(State::ERROR);
    }
}