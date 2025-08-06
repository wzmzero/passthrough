#include <sqlite_orm/sqlite_orm.h>
#include <iostream>
#include <string>
#include <memory>
 

// 端点配置结构体
struct EndpointConfig {
    int id = 0;  // 主键
    std::string type;
    uint16_t port = 0;
    std::string ip;
    std::string serial_port;
    uint32_t baud_rate = 0;

    bool operator==(const EndpointConfig& other) const {
        return id == other.id &&
               type == other.type &&
               port == other.port &&
               ip == other.ip &&
               serial_port == other.serial_port &&
               baud_rate == other.baud_rate;
    }
};

// 通道配置结构体
struct ChannelConfig {
    int id = 0;  // 主键
    std::string name;
    int input_id;  // 外键 -> EndpointConfig.id
    int output_id; // 外键 -> EndpointConfig.id
    // 嵌套对象（程序内使用）
    EndpointConfig input;  
    EndpointConfig output; 

    bool operator==(const ChannelConfig& other) const {
        return id == other.id &&
               name == other.name &&
               input_id == other.input_id &&
               output_id == other.output_id &&
               input == other.input &&
               output == other.output;
    }
};

// 创建数据库存储（两个表）
auto createStorage(const std::string& filename) {
    using namespace sqlite_orm;
    return make_storage(
        filename,
        make_table("endpoints",
            make_column("id", &EndpointConfig::id, primary_key().autoincrement()),
            make_column("type", &EndpointConfig::type),
            make_column("port", &EndpointConfig::port),
            make_column("ip", &EndpointConfig::ip),
            make_column("serial_port", &EndpointConfig::serial_port),
            make_column("baud_rate", &EndpointConfig::baud_rate)
        ),
        make_table("channels",
            make_column("id", &ChannelConfig::id, primary_key().autoincrement()),
            make_column("name", &ChannelConfig::name),
            make_column("input_id", &ChannelConfig::input_id),
            make_column("output_id", &ChannelConfig::output_id),
            foreign_key(&ChannelConfig::input_id)
                .references(&EndpointConfig::id)
                .on_delete.cascade()
                .on_update.cascade(),
            foreign_key(&ChannelConfig::output_id)
                .references(&EndpointConfig::id)
                .on_delete.cascade()
                .on_update.cascade()
        )
    );
}

using Storage = decltype(createStorage(""));

int main() {
    auto storage = createStorage("cascade_delete.db");
    storage.sync_schema();

    // 1. 创建端点配置
    EndpointConfig inputEndpoint{0, "tcp", 8080, "192.168.1.100", "", 0};
    EndpointConfig outputEndpoint{0, "serial", 0, "", "/dev/ttyUSB0", 9600};

    // 2. 插入端点到数据库
    auto inputId = storage.insert(inputEndpoint);
    auto outputId = storage.insert(outputEndpoint);

    // 3. 创建通道配置（使用智能指针管理嵌套对象）
   
    // 4. 插入通道到数据库  
    storage.get<EndpointConfig>(inputId);
    storage.get<EndpointConfig>(outputId);

    storage.remove<EndpointConfig>(inputId);
    storage.remove<EndpointConfig>(outputId);
    try{
 
    }catch(std::system_error e) {
        std::cout << e.what() << std::endl;
    }catch(...){
        std::cout << "unknown exeption" << std::endl;
    }
 
    std::cout << "删除后端点数量: " << storage.count<EndpointConfig>() << "\n"; // 应输出0

    return 0;
}