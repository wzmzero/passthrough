from pymodbus.server import StartSerialServer
from pymodbus.datastore import ModbusSequentialDataBlock
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext

def run_server():
    # 初始化数据存储
    store = ModbusSlaveContext(
        di=ModbusSequentialDataBlock(0, [0]*100),  # 离散输入
        co=ModbusSequentialDataBlock(0, [0]*100),  # 线圈
        hr=ModbusSequentialDataBlock(0, [0]*100),  # 保持寄存器
        ir=ModbusSequentialDataBlock(0, [0]*100)   # 输入寄存器
    )
    context = ModbusServerContext(slaves=store, single=True)

    # 启动串行 Modbus 服务器
    StartSerialServer(
        context,
        port='COM1',               # 串口名称 (Windows)
        # port='/dev/ttyUSB0',     # Linux串口设备
        baudrate=9600,             # 波特率
        bytesize=8,                # 数据位
        parity='N',                # 校验位 (N无校验/E偶校验/O奇校验)
        stopbits=1,                # 停止位
        timeout=0.05               # 超时时间(秒)
    )

if __name__ == "__main__":
    print("启动Modbus RTU服务器...")
    run_server()