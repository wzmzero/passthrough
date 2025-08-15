// common.h
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <ctime>
#include <functional>
#include <unordered_map>
#include <variant>
#include <any>
enum ProtoType {
    MODBUS_M = 1,   // Modbus主站
    MODBUS_S,       // Modbus子站
    IEC101_M,       // 101主站
    IEC101_S,       // 101子站
    IEC104_M,       // 104主站
    IEC104_S        // 104子站
};
 //数据类型
enum Data_Type {
	Data_YX = 1,	//遥信
	Data_YC,			//遥测
	Data_YK,			//遥控
	Data_YT			//遥调
};
// 值类型枚举
enum class ValueType {
    Boolean,      // 布尔值 (0/1)
    Integer,      // 整数
    Float         // 浮点数
};

enum CommInsType {
	Ins_Acquire = 1,	//采集实例
	Ins_Transmit		//转发实例
};
struct InstanceParm {
    int id;
    std::string name;   // RTU/DAS
    CommInsType type;    //采集/转发
	DriverParam driverParam;	//驱动参数
    EndpointConfig channelParam;	//通道参数
    VecDevInfo vecDevInfo;		//设备信息
};
typedef std::vector<InstanceParm> VEC_INSTANCE;

/* 四遥点结构体
    * 用于报文解析的中间数据结构，比如Modbus帧->四遥点,四遥点->Modbus帧。
*/
 struct TelemPoint{
    uint32_t proAddr;                    //规约地址\寄存器地址 (e.g., "0x4001")
    Data_Type data_type;                // 四遥类型 YX, YC, YK, YT
    // uint8_t subAddr;                        // 从站地址 (e.g., "1")
    int value;                     // 存储实际值

};
typedef std::map<int, TelemPoint> MapTelemPoint; // 四遥点表 (ID -> TelemPoint)
typedef std::vector<TelemPoint> VecTelemPoint; // 四遥点向量

/*
    一个规约转换器对应一个DevInfo.映射表 作为TelemPoint -> ComData的桥梁
    不同的通过对应的DevInfo将各自的TemlemPoint映射到同一个ComData。
    DevInfo包含从站地址(为规约转换器提供的公共地址)
*/

// 端点配置结构体
struct EndpointConfig {
    int id = 0;  // 主键
    std::string type;
    uint16_t port = 0;
    std::string ip;
    std::string serial_port;
    uint32_t baud_rate = 0;
	int instance_id;
    // 添加比较运算符
    bool operator==(const EndpointConfig& other) const {
        return type == other.type &&
               port == other.port &&
               ip == other.ip &&
               serial_port == other.serial_port &&
               baud_rate == other.baud_rate;
    }
    bool operator!=(const EndpointConfig& other) const {
        return !(*this == other);
    }
};

struct  DevInfo{
    int dataId;				            //全局唯一ID
    uint16_t slave_addr;					// 
	uint32_t proAddr;                    //规约地址\寄存器地址 (e.g., "0x4001")
   	std::string description;                   // 描述
	Data_Type data_type;    // 四遥类型 YX, YC, YK, YT
    ValueType value_type;   // 值类型 (e.g., Boolean, Integer, Float)
    int value;              // 存储实际值
    std::string unit;       // 单位 (e.g., "V", "A")
	int instance_id;             //数据查询使用
};
typedef std::map<int, DevInfo> MapDevInfo; // 设备信息表 (ID -> DevInfo) 
typedef std::vector<DevInfo> VecDevInfo; // 设备信息向量

//数据表(后续为数据库)，实时变化更新
struct Dataset {
    int dataId;             // 全局唯一ID
    std::string name;       // 名称
    Data_Type data_type;    // 四遥类型 YX, YC, YK, YT
    ValueType value_type;   // 值类型 (e.g., Boolean, Integer, Float)
    std::any value;              // 存储实际值
    std::time_t timestamp;  // 时间戳
    std::string unit;       // 单位 (e.g., "V", "A", "m/s")
};
typedef std::map<int, Dataset> MapDataset; // 数据集表 (ID -> Dataset)
typedef std::vector<Dataset> VecDataset; // 数据集向量


//104主站驱动参数
typedef struct M104Param_tag {
	unsigned short sub_cmn_addr;	//子站公共地址,
	int cyc_All;			//总召唤周期
	int cyc_All_E;		//电度总召唤周期YM
	unsigned char yk_sel;			//遥控选择
	unsigned char yt_sel;			//遥调选择
	unsigned char len_cos;			//传输原因的长度
	unsigned char len_cmn_addr;	//公共地址长度
	unsigned char len_info_addr;	//信息体地址长度
	unsigned char param_K;			//k参数
	unsigned char param_W;			//w参数
	unsigned char timeout1;			//发送或测试APDU超时
	unsigned char timeout2;			//S格式确认超时
	unsigned char timeout3;			//链路上没有任何格式的信息空闲超时
} M104Param,*P_M104Param;
//104子站驱动参数
typedef struct S104Param_tag {
	unsigned short sub_cmn_addr;			//子站公共地址
	unsigned char yx_type;					//遥信类型
	unsigned char yc_type;					//遥测类型
	unsigned char ym_type;					//遥脉类型
	unsigned char len_cos;					//传输原因的长度
	unsigned char len_cmn_addr;			//公共地址长度
	unsigned char len_info_addr;			//信息体地址长度
	int back_cycle;							//背景扫描周期
	unsigned char time_exec;				//是否执行对时
	unsigned char param_K;					//k参数
	unsigned char param_W;					//w参数
	unsigned char timeout1;					//发送或测试APDU超时
	unsigned char timeout2;					//S格式确认超时
	unsigned char timeout3;					//链路上没有任何格式的信息空闲超时
}S104Param,*P_S104Param;
//101主站驱动参数
typedef struct M101Param_tag {
	unsigned short sub_cmn_addr;	//子站公共地址,
	unsigned char link_addr;		//链路地址    子站站址
	int cyc_All;						//总召唤周期
	int cyc_All_E;						//电度总召唤周期YM
	unsigned char yk_sel;			//遥控选择
	unsigned char yt_sel;			//遥调选择
	unsigned char ym_freeze;		//召唤遥脉是否冻结 0：不冻结   1：冻结 默认0
	unsigned char len_cos;			//传输原因的长度
	unsigned char len_cmn_addr;	//公共地址长度
	unsigned char len_info_addr;	//信息体地址长度
	int time_out;						//应答超时
} M101Param,*P_M101Param;
// MODBUS主站驱动参数
typedef struct MModbusParam_tag {
    unsigned short transmit_mode;    // 传输模式 (0:RTU, 1:TCP)
    unsigned short time_out;         // 超时（毫秒）
    unsigned short interval;         // 召测间隔（毫秒）
    unsigned short maxSize;          // 最大应答字节数
} MModbusParam, *P_MModbusParam;
//MODBUS子站驱动参数
typedef struct SModbusParam_tag {
	unsigned short slave_addr;		//从站地址
	unsigned short transmit_mode;	//传输模式
}SModbusParam,*P_SModbusParam;

//驱动参数联合
struct DriverParam {
    ProtoType type; // 添加类型标识字段
	union
	{
	M101Param m101_param;   //主站101
	M104Param m104_param;	//主站104
	S104Param s104_param;	//子站104
	MModbusParam mModbus_param;//modbus主站
	SModbusParam sModbus_param;//modbus子站
	};
};

//功能码枚举
enum FunctionCode {
	VIRTUAL_POINT = 0,		//虚拟测点
	SWITCH_OUT = 1,			//读取开出状态（常用）
	SWITCH_IN = 2,				//读取开入状态（常用）
	ANALOG_OUT = 3,			//读取模出状态（常用）
	ANALOG_IN = 4,				//读取模入状态（常用）
	SNGL_SWITCH_SET = 5,		//设置单路开关状态（常用）
	SNGL_ANALOG_SET = 6,		//设置单路模拟量值（常用）
	EXCEPT_STAT = 7,			//读取异常状态
	RETURN_CRC = 8,			//回送诊断校验
	PROGRAM_1 = 9,				//编程（只用于484）
	INQUIRE_1 = 10,			//探询（用于9之后）
	EVENT_COUNT = 11,		 	//读取事件计数
	COMM_EVENT_COUNT = 12,	//读取通信事件计数
	PROGRAM_2 = 13,			//编程（184/384 484 584）
	INQUIRE_2 = 14,			//探询（用于13之后）
	MUTI_SWITCH_SET = 15,	//设置多路开关状态
	MUTI_ANALOG_SET = 16,	//设置多路模拟量值
	REPORT_FALG = 17,		   //报告从机标识
	PROGRAM_3 = 18,			//编程
	SHGK_W = 19	      		//首航高科此功能码值用于写命令SHGK_W = 19
};

 

