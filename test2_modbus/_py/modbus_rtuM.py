from pymodbus.client import ModbusSerialClient as ModbusClient
import time

def read_registers():
    # 创建Modbus RTU客户端
    client = ModbusClient(
        method='rtu',
        port='COM1',               # 串口名称 (Windows)
        # port='/dev/ttyUSB0',     # Linux串口设备
        baudrate=9600,
        bytesize=8,
        parity='N',
        stopbits=1,
        timeout=1
    )
    
    # 连接串口
    connection = client.connect()
    print("连接状态:", connection)

    if connection:
        try:
            # 读取保持寄存器 (地址0开始的10个寄存器)
            result = client.read_holding_registers(address=0, count=10, unit=1)
            if not result.isError():
                print("寄存器值:", result.registers)
            else:
                print("读取错误:", result)
                
            # 写入单个保持寄存器 (地址0)
            client.write_register(address=0, value=123, unit=1)
            
            # 写入线圈 (地址0置为ON)
            client.write_coil(address=0, value=True, unit=1)
            
        finally:
            # 关闭连接
            client.close()
    else:
        print("无法连接到串口设备")

if __name__ == "__main__":
    read_registers()