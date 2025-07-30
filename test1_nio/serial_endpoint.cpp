#include "serial_endpoint.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

SerialEndpoint::SerialEndpoint(const std::string& port, uint32_t baudRate)
    : port_(port), baudRate_(baudRate) {}

SerialEndpoint::~SerialEndpoint() {
    close();
}

void SerialEndpoint::send(const uint8_t* data, size_t len) {
    if (fd_ < 0) {
        log("Serial port not open, data dropped");
        return;
    }
    
    ssize_t sent = write(fd_, data, len);
    if (sent < 0) {
        log("Write failed: " + std::string(strerror(errno)));
    } else if (static_cast<size_t>(sent) != len) {
        log("Incomplete write: " + std::to_string(sent) + "/" + 
            std::to_string(len) + " bytes");
    }
}

std::string SerialEndpoint::getInfo() const {
    return "Serial:" + port_ + ":" + std::to_string(baudRate_);
}

bool SerialEndpoint::connect() {
    fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        log("Open failed: " + std::string(strerror(errno)));
        return false;
    }
    
    struct termios tty;
    if (tcgetattr(fd_, &tty)) {
        log("Get attributes failed: " + std::string(strerror(errno)));
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    speed_t speed = getBaudRate(baudRate_);
    if (speed == B0) {
        log("Unsupported baud rate: " + std::to_string(baudRate_));
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);
    
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
    
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;
    
    tty.c_cc[VTIME] = 10;
    tty.c_cc[VMIN] = 0;
    
    if (tcsetattr(fd_, TCSANOW, &tty)) {
        log("Set attributes failed: " + std::string(strerror(errno)));
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    tcflush(fd_, TCIOFLUSH);
    
    log("Serial port opened: " + port_ + " at " + 
        std::to_string(baudRate_) + " baud");
    return true;
}

void SerialEndpoint::disconnect() {
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
}

void SerialEndpoint::run() {
    FD_ZERO(&readFds_);
    FD_SET(fd_, &readFds_);
    
    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    int activity = select(fd_ + 1, &readFds_, nullptr, nullptr, &timeout);
    
    if (activity < 0 && errno != EINTR) {
        log("Select error: " + std::string(strerror(errno)));
        status_ = DISCONNECTED;
        return;
    }
    
    if (activity == 0) {
        // Timeout, nothing to do
        return;
    }
    
    if (FD_ISSET(fd_, &readFds_)) {
        uint8_t buffer[4096];
        ssize_t bytesRead = read(fd_, buffer, sizeof(buffer));
        
        if (bytesRead < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                log("Read error: " + std::string(strerror(errno)));
                status_ = DISCONNECTED;
            }
            return;
        }
        
        if (bytesRead > 0 && dataCallback_) {
            dataCallback_(buffer, bytesRead);
        }
    }
}

speed_t SerialEndpoint::getBaudRate(uint32_t baud) {
    switch (baud) {
        case 50: return B50;
        case 75: return B75;
        case 110: return B110;
        case 134: return B134;
        case 150: return B150;
        case 200: return B200;
        case 300: return B300;
        case 600: return B600;
        case 1200: return B1200;
        case 1800: return B1800;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 500000: return B500000;
        case 576000: return B576000;
        case 921600: return B921600;
        case 1000000: return B1000000;
        case 1152000: return B1152000;
        case 1500000: return B1500000;
        case 2000000: return B2000000;
        case 2500000: return B2500000;
        case 3000000: return B3000000;
        case 3500000: return B3500000;
        case 4000000: return B4000000;
        default: return B0;
    }
}