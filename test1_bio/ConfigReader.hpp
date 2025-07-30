#ifndef CONFIG_READER_HPP
#define CONFIG_READER_HPP

#include <string>
#include <sqlite3.h>
#include <vector>
struct EndpointConfig {
    int id;
    std::string type;
    std::string name;
    std::string config_json; // JSON格式的配置参数
};

struct ChannelConfig {
    int id;
    std::string name;
    EndpointConfig input;
    EndpointConfig output;
};  
class ConfigReader {
public:
    enum Protocol { TCP, UDP, SERIAL };
    
    ConfigReader(const std::string& db_path);
    ~ConfigReader();
    
    // 禁止拷贝
    ConfigReader(const ConfigReader&) = delete;
    ConfigReader& operator=(const ConfigReader&) = delete;
    
    struct ClientConfig {
        std::string server_ip;
        int server_port;
        // 串口配置
        std::string serial_port;
        int baud_rate;
    };
    
    struct ServerConfig {
        int listen_port;
    };
    
    // 读取客户端配置（根据协议类型）
    bool readClientConfig(Protocol protocol, ClientConfig& config);
    // 写入客户端配置（根据协议类型）
    bool writeClientConfig(Protocol protocol, const ClientConfig& config);
    // 读取服务器配置（根据协议类型）
    bool readServerConfig(Protocol protocol, ServerConfig& config);
    // 写入服务器配置（根据协议类型）
    bool writeServerConfig(Protocol protocol, const ServerConfig& config);

        // 新增方法
    std::vector<ChannelConfig> loadChannels();
    bool saveChannel(const ChannelConfig& channel);
    bool deleteChannel(int channel_id);
private:
    sqlite3* db_ = nullptr;
    std::string db_path_;
    
    bool openDatabase();
    void closeDatabase();
    bool executeQuery(const std::string& sql, int (*callback)(void*, int, char**, char**) = nullptr, void* data = nullptr);
    bool executeUpdate(const std::string& sql); //  用于执行更新操作
    static std::string protocolToString(Protocol protocol);
};

#endif // CONFIG_READER_HPP