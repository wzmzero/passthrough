struct Proto_Type_Mapping {
    static constexpr std::array<std::pair<ProtoType, const char*>, 6> value = {{
        {ProtoType::MODBUS_M, "MODBUS_M"},
        {ProtoType::MODBUS_S, "MODBUS_S"},
        {ProtoType::IEC101_M, "IEC101_M"},
        {ProtoType::IEC101_S, "IEC101_S"},
        {ProtoType::IEC104_M, "IEC104_M"},
        {ProtoType::IEC104_S, "IEC104_S"},
    }};
};

 std::this_thread::sleep_for(std::chrono::seconds(1));