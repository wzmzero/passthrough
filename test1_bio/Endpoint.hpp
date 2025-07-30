#pragma once
#include <string>
#include <functional>
#include <memory>

class Endpoint {
public:
    using DataCallback = std::function<void(const std::string&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    
    enum Type {
        SERIAL,
        TCP_CLIENT,
        TCP_SERVER,
        UDP_CLIENT,
        UDP_SERVER
    };
    
    virtual ~Endpoint() = default;
    
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool write(const std::string& data) = 0;
    
    virtual void setDataCallback(DataCallback callback) = 0;
    virtual void setErrorCallback(ErrorCallback callback) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    
    virtual Type getType() const = 0;
    virtual std::string getConfigString() const = 0;
};