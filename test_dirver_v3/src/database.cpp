#include "database.h"
#include <iostream>
#include <algorithm>
 

// 创建存储的具体实现
Database::StorageType Database::createStorage(const std::string& filename) {
    return makeStorageSchema(filename);
}

// 构造函数
Database::Database(const std::string& filename)
    : storage(createStorage(filename))  // 初始化存储
{
    storage.sync_schema();

    // 设置数据库连接
    storage.on_open = [this](sqlite3* db_) {
        db = db_;
        sqlite3_update_hook(db, &Database::updateHook, this);
    };
    storage.open_forever();
}

// 执行SQL语句
void Database::execute(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL 执行错误: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}

// 注册回调函数
void Database::registerCallback(DbChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    callbacks.push_back(std::move(callback));
}

// 更新钩子函数
void Database::updateHook(void* self, int op, const char* dbName, const char* tableName, sqlite3_int64 rowid) {
    Database* db = static_cast<Database*>(self);
    const std::string table(tableName);
    const int rid = static_cast<int>(rowid);

    std::any data;
    db->handleTableUpdate(table, rid, data);

    std::lock_guard<std::mutex> lock(db->callbackMutex);
    for (const auto& cb : db->callbacks) {
        cb(table, op, rid, data);
    }
}

// 转换函数：将DriverParam_Mid转换为DriverParam
DriverParam Database::convertToDriverParam(const std::vector<DriverParam_Mid>& params) {
    if (params.empty()) {
        return DriverParam{};
    }
    
    ProtoType protoType = params[0].proto_type;
    DriverParam driverParam;
    driverParam.type = protoType; // 设置协议类型
    
    // 根据协议类型解析参数
    for (const auto& param : params) {
        const std::string& name = param.param_name;
        const std::any& value = param.param_value;
        try {
            switch (protoType) {
                case ProtoType::MODBUS_M:
                    if (name == "transmit_mode") {
                        driverParam.mModbus_param.transmit_mode = std::any_cast<unsigned short>(value);
                    } else if (name == "time_out") {
                        driverParam.mModbus_param.time_out = std::any_cast<unsigned short>(value);
                    } else if (name == "interval") {
                        driverParam.mModbus_param.interval = std::any_cast<unsigned short>(value);
                    } else if (name == "maxSize") {
                        driverParam.mModbus_param.maxSize = std::any_cast<unsigned short>(value);
                    }
                    break;
                case ProtoType::MODBUS_S:
                    if (name == "slave_addr") {
                        driverParam.sModbus_param.slave_addr = std::any_cast<unsigned short>(value);
                    } else if (name == "transmit_mode") {
                        driverParam.sModbus_param.transmit_mode = std::any_cast<unsigned short>(value);
                    }
                    break;
                case ProtoType::IEC101_M:
                    if (name == "sub_cmn_addr") {
                        driverParam.m101_param.sub_cmn_addr = std::any_cast<unsigned short>(value);
                    } else if (name == "link_addr") {
                        driverParam.m101_param.link_addr = std::any_cast<unsigned char>(value);
                    } else if (name == "cyc_All") {
                        driverParam.m101_param.cyc_All = std::any_cast<int>(value);
                    }
                    break;
                case ProtoType::IEC104_M:
                    if (name == "sub_cmn_addr") {
                        driverParam.m104_param.sub_cmn_addr = std::any_cast<unsigned short>(value);
                    } else if (name == "cyc_All") {
                        driverParam.m104_param.cyc_All = std::any_cast<int>(value);
                    } else if (name == "param_K") {
                        driverParam.m104_param.param_K = std::any_cast<unsigned char>(value);
                    }
                    // 其他参数类似...
                    break;
                default:
                    std::cerr << "未知协议类型: " << static_cast<int>(protoType) << std::endl;
                    break;
            }
        } catch (const std::bad_any_cast& e) {
            std::cerr << "参数类型转换错误: " << name << " - " << e.what() << std::endl;
        }
    }
    
    return driverParam;
}

// 转换函数：将DriverParam转换为std::vector<DriverParam_Mid>
std::vector<DriverParam_Mid> Database::convertFromDriverParam(const DriverParam& driverParam, int instanceId) {
    std::vector<DriverParam_Mid> params;
    ProtoType protoType = driverParam.type;
    
    switch (protoType) {
        case ProtoType::MODBUS_M: {
            const MModbusParam& modbusParam = driverParam.mModbus_param;
            params.push_back(DriverParam_Mid{0, protoType, "传输模式", "transmit_mode", modbusParam.transmit_mode, instanceId});
            params.push_back(DriverParam_Mid{0, protoType, "超时时间", "time_out", modbusParam.time_out, instanceId});
            params.push_back(DriverParam_Mid{0, protoType, "轮询间隔", "interval", modbusParam.interval, instanceId});
            params.push_back(DriverParam_Mid{0, protoType, "最大字节数", "maxSize", modbusParam.maxSize, instanceId});
            break;
        }
        case ProtoType::MODBUS_S: {
            const SModbusParam& modbusParam = driverParam.sModbus_param;
            params.push_back(DriverParam_Mid{0, protoType, "从站地址", "slave_addr", modbusParam.slave_addr, instanceId});
            params.push_back(DriverParam_Mid{0, protoType, "传输模式", "transmit_mode", modbusParam.transmit_mode, instanceId});
            break;
        }
        case ProtoType::IEC101_M: {
            const M101Param& param = driverParam.m101_param;
            params.push_back(DriverParam_Mid{0, protoType, "子站公共地址", "sub_cmn_addr", param.sub_cmn_addr, instanceId});
            params.push_back(DriverParam_Mid{0, protoType, "链路地址", "link_addr", param.link_addr, instanceId});
            params.push_back(DriverParam_Mid{0, protoType, "总召唤周期", "cyc_All", param.cyc_All, instanceId});
            // 其他参数类似...
            break;
        }
        case ProtoType::IEC104_M: {
            const M104Param& param = driverParam.m104_param;
            params.push_back(DriverParam_Mid{0, protoType, "子站公共地址", "sub_cmn_addr", param.sub_cmn_addr, instanceId});
            params.push_back(DriverParam_Mid{0, protoType, "总召唤周期", "cyc_All", param.cyc_All, instanceId});
            params.push_back(DriverParam_Mid{0, protoType, "K参数", "param_K", param.param_K, instanceId});
            // 其他参数类似...
            break;
        }
        default:
            std::cerr << "未知协议类型: " << static_cast<int>(protoType) << std::endl;
            break;
    }
    
    return params;
}

// 加载所有实例配置
std::vector<InstanceParm> Database::loadInstances() {
    std::vector<InstanceParm> instances;
    storage.transaction([&] {
        // 加载所有实例
        instances = storage.get_all<InstanceParm>();
        
        // 为每个实例加载关联数据
        for (auto& instance : instances) {
            int instanceId = instance.id;
            
            // 加载通道参数
            auto channels = storage.get_all<EndpointConfig>(
                where(c(&EndpointConfig::instance_id) == instanceId)
            );
            if (!channels.empty()) {
                instance.channelParam = channels[0];
            }
            
            // 加载驱动参数
            auto driverParams = storage.get_all<DriverParam_Mid>(
                where(c(&DriverParam_Mid::instance_id) == instanceId)
            );
            instance.driverParam = convertToDriverParam(driverParams);
            
            // 加载设备信息
            instance.vecDevInfo = storage.get_all<DevInfo>(
                where(c(&DevInfo::instance_id) == instanceId)
            );
        }
        return true;
    });
    
    return instances;
}

// 替换所有实例配置
void Database::replaceInstances(std::vector<InstanceParm>& instances) {
    storage.transaction([&] {
        // 先删除所有相关表的数据（注意外键约束）
        storage.remove_all<DevInfo>();
        storage.remove_all<DriverParam_Mid>();
        storage.remove_all<EndpointConfig>();
        storage.remove_all<InstanceParm>();
        
        for (auto& instance : instances) {
            // 保存实例基本信息
            instance.id = storage.insert(instance);
            
            // 保存通道参数
            if (instance.channelParam != EndpointConfig{}) {
                instance.channelParam.instance_id = instance.id;
                instance.channelParam.id = storage.insert(instance.channelParam);
            }
            
            // 保存驱动参数
            auto driverParams = convertFromDriverParam(instance.driverParam, instance.id);
            for (auto& param : driverParams) {
                param.id = storage.insert(param);
            }
            
            // 保存设备信息
            for (auto& devInfo : instance.vecDevInfo) {
                devInfo.instance_id = instance.id;
                storage.insert(devInfo);
            }
        }
        return true;
    });
}

// 初始化示例数据
void Database::initSampleData() {
    // 创建Modbus主站实例
    InstanceParm modbusMaster;
    modbusMaster.id = 0; // 插入时会自动生成ID
    modbusMaster.name = "Modbus Master";
    modbusMaster.type = CommInsType::Ins_Acquire;
    // 设置通道参数
    modbusMaster.channelParam.type = "tcp_client";
    modbusMaster.channelParam.port = 502;
    modbusMaster.channelParam.ip = "192.168.1.100";
    // 设置驱动参数
    modbusMaster.driverParam.type = ProtoType::MODBUS_M;
    modbusMaster.driverParam.mModbus_param = MModbusParam{0, 5000, 1000, 256};
    // 添加设备信息
    DevInfo dev1;
    dev1.dataId = 1001;
    dev1.description = "温度传感器";
    dev1.slave_addr = 1;
    dev1.proAddr = 0x4001;
    dev1.data_type = Data_Type::Data_YC;
    dev1.value_type = ValueType::Float;
    dev1.value = 25; // 示例值
    dev1.unit = "°C";
    DevInfo dev2;
    dev2.dataId = 1002;
    dev2.description = "压力传感器";
    dev2.slave_addr = 1;
    dev2.proAddr = 0x4002;
    dev2.data_type = Data_Type::Data_YC;
    dev2.value_type = ValueType::Float;
    dev2.value = 1.2; // 示例值
    dev2.unit = "MPa";
    modbusMaster.vecDevInfo = {dev1, dev2};
    
    // 创建IEC104主站实例
    InstanceParm iec104Master;
    iec104Master.id = 0;
    iec104Master.name = "IEC104 Master";
    iec104Master.type = CommInsType::Ins_Acquire;
    // 设置通道参数
    iec104Master.channelParam.type = "Serial";
    iec104Master.channelParam.serial_port = "COM1";
    iec104Master.channelParam.baud_rate = 9600;
    
    // 设置驱动参数
    iec104Master.driverParam.type = ProtoType::IEC104_M;
    iec104Master.driverParam.m104_param = M104Param{0x01, 3000, 6000, 1, 1, 2, 3, 12, 8, 15, 10, 20};
    
    // 添加设备信息
    DevInfo dev3;
    dev3.dataId = 2001;
    dev3.description = "开关状态";
    dev3.slave_addr = 2;
    dev3.proAddr = 0x6001;
    dev3.data_type = Data_Type::Data_YX;
    dev3.value_type = ValueType::Boolean;
    dev3.value = 1; // 示例值
    dev3.unit = "";
    
    iec104Master.vecDevInfo = {dev3};
    
    // 替换数据库中的所有实例
    std::vector<InstanceParm> instances = {modbusMaster, iec104Master};
    replaceInstances(instances);
    
    std::cout << "示例数据初始化完成" << std::endl;
}



// 加载所有通道配置（包含端点信息）
std::vector<ChannelConfig> Database::loadChannels() {
    std::vector<ChannelConfig> channels;
    storage.transaction([&] {
        // 加载所有通道
        channels = storage.get_all<ChannelConfig>();
        
        // 为每个通道加载端点信息
        for (auto& channel : channels) {
            if (channel.input_id > 0) {
                if (auto input = storage.get_pointer<EndpointConfig>(channel.input_id)) {
                    channel.input = *input;
                }
            }
            if (channel.output_id > 0) {
                if (auto output = storage.get_pointer<EndpointConfig>(channel.output_id)) {
                    channel.output = *output;
                }
            }
        }
        return true;
    });
    
    return channels;
}


// 简化版替换所有通道配置
void Database::replaceChannels(std::vector<ChannelConfig>& channels) {
    storage.transaction([&] {
        // 删除所有现有配置
        storage.remove_all<ChannelConfig>();
        storage.remove_all<EndpointConfig>();
        
        // 直接插入新配置
        for (auto& channel : channels) {
            // 插入端点
            channel.input.id = storage.insert(channel.input);
            channel.input_id = channel.input.id;
            
            channel.output.id = storage.insert(channel.output);
            channel.output_id = channel.output.id;
            
            // 插入通道
            channel.id = storage.insert(channel);
        }
        
        return true;
    });
}