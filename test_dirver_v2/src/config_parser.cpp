#include "config_parser.h"
#include <fstream>
#include <stdexcept>
#include <cstdint>
#include <yaml-cpp/yaml.h>    // YAML 支持
 

using json = nlohmann::json;

// 检测文件格式
ConfigParserFactory::Format ConfigParserFactory::detectFormat(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos == std::string::npos) return Format::UNKNOWN;
    
    std::string ext = filename.substr(pos);
    if (ext == ".json") return Format::JSON;
    if (ext == ".yaml" || ext == ".yml") return Format::YAML;
 
    return Format::UNKNOWN;
}

// 创建解析器
std::unique_ptr<IConfigParser> ConfigParserFactory::createParser(const std::string& filename) {
    Format format = detectFormat(filename);
    switch (format) {
        case Format::JSON:
            return std::make_unique<JsonConfigParser>();
        case Format::YAML:
            return std::make_unique<YamlConfigParser>();
        default:
            throw std::runtime_error("Unsupported config format: " + filename);
    }
}

// 解析端点配置
EndpointConfig ConfigParserFactory::parseEndpoint(const nlohmann::json& j) {
    EndpointConfig config;
    config.type = j["type"].get<std::string>();
    
    if (j.contains("port")) {
        config.port = j["port"].get<uint16_t>();
    }
    if (j.contains("ip")) {
        config.ip = j["ip"].get<std::string>();
    }
    if (j.contains("serial_port")) {
        config.serial_port = j["serial_port"].get<std::string>();
    }
    if (j.contains("baud_rate")) {
        config.baud_rate = j["baud_rate"].get<uint32_t>();
    }
    
    return config;
}

// 解析通道配置
std::vector<ChannelConfig> ConfigParserFactory::parseJson(const nlohmann::json& j) {
    std::vector<ChannelConfig> channels;
    
    if (!j.contains("channels")) {
        throw std::runtime_error("Invalid config format: missing 'channels' key");
    }
    
    for (const auto& channel : j["channels"]) {
        ChannelConfig config;
        config.name = channel["name"].get<std::string>();
        config.input = parseEndpoint(channel["input"]);
        config.output = parseEndpoint(channel["output"]);
        channels.push_back(config);
    }
    
    return channels;
}

// JSON 解析器实现
std::vector<ChannelConfig> JsonConfigParser::parse(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filename);
    }
    
    json config;
    file >> config;
    return ConfigParserFactory::parseJson(config);
}

// YAML 解析器实现
std::vector<ChannelConfig> YamlConfigParser::parse(const std::string& filename) {
    YAML::Node config = YAML::LoadFile(filename);
    std::vector<ChannelConfig> channels;
    
    if (!config["channels"]) {
        throw std::runtime_error("Invalid config format: missing 'channels' key");
    }
    
    for (const auto& channel : config["channels"]) {
        ChannelConfig chConfig;
        chConfig.name = channel["name"].as<std::string>();
        
        // 解析输入端点
        YAML::Node input = channel["input"];
        chConfig.input.type = input["type"].as<std::string>();
        if (input["port"]) chConfig.input.port = input["port"].as<uint16_t>();
        if (input["ip"]) chConfig.input.ip = input["ip"].as<std::string>();
        if (input["serial_port"]) chConfig.input.serial_port = input["serial_port"].as<std::string>();
        if (input["baud_rate"]) chConfig.input.baud_rate = input["baud_rate"].as<uint32_t>();
        
        // 解析输出端点
        YAML::Node output = channel["output"];
        chConfig.output.type = output["type"].as<std::string>();
        if (output["port"]) chConfig.output.port = output["port"].as<uint16_t>();
        if (output["ip"]) chConfig.output.ip = output["ip"].as<std::string>();
        if (output["serial_port"]) chConfig.output.serial_port = output["serial_port"].as<std::string>();
        if (output["baud_rate"]) chConfig.output.baud_rate = output["baud_rate"].as<uint32_t>();
        
        channels.push_back(chConfig);
    }
    
    return channels;
}
 