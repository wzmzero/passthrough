#pragma once
#include "shared_structs.h"
#include <vector>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

// 解析器接口
class IConfigParser {
public:
    virtual ~IConfigParser() = default;
    virtual std::vector<ChannelConfig> parse(const std::string& filename) = 0;
};

// JSON 解析器
class JsonConfigParser : public IConfigParser {
public:
    std::vector<ChannelConfig> parse(const std::string& filename) override;
};

// YAML 解析器
class YamlConfigParser : public IConfigParser {
public:
    std::vector<ChannelConfig> parse(const std::string& filename) override;
};

// TOML 解析器 -> TODO

// 解析器工厂
class ConfigParserFactory {
public:
    static std::unique_ptr<IConfigParser> createParser(const std::string& filename);
    
    // 根据文件扩展名检测格式
    enum class Format { JSON, YAML, UNKNOWN };
    static Format detectFormat(const std::string& filename);
    
    // 辅助函数
    static EndpointConfig parseEndpoint(const nlohmann::json& j);
    static std::vector<ChannelConfig> parseJson(const nlohmann::json& j);
};