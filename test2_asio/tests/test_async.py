import asyncio
import telnetlib3

async def telnet_client(host, port, client_id):
    try:
        reader, writer = await telnetlib3.open_connection(host, port)
        # 模拟登录流程
        writer.write("username\r\n")
        await asyncio.sleep(0.1)
        writer.write("password\r\n")
        
        # 持续交互
        for _ in range(100):
            writer.write("show status\r\n")
            await reader.readuntil(b"#")  # 等待提示符
            await asyncio.sleep(0.5)
            
    except Exception as e:
        print(f"Client {client_id} failed: {str(e)}")

async def main(host, port, num_clients):
    tasks = []
    for i in range(num_clients):
        task = asyncio.create_task(telnet_client(host, port, i))
        tasks.append(task)
        await asyncio.sleep(0.01)  # 控制新建连接速率
    
    await asyncio.gather(*tasks)

if __name__ == "__main__":
    asyncio.run(main("127.0.0.1", 8080, 5000))  # 并发5000连接