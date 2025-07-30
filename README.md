Makefile语法笔记
=====================

变量
-------------------
$@：目标文件，$^：所有的依赖文件，$<：第一个依赖文件,$?：所有更新过的依赖文件

通配符 (Wildcards)
-------------------
*：匹配任意数量字符（不含路径分隔符）

?：匹配单个字符

[...]：匹配字符集（如 [abc] 匹配 a/b/c）

[^...]：排除字符集


工具链
-------------------
### arm32    -> `arm-linux-gnueabihf-g++`
### arm64    -> `aarch64-linux-gnu-g++`
### x86_win  -> `x86_64-w64-mingw32-g++`

-----
## 主程序流程图

```mermaid
graph TD
  %% 管理层
  A[Main] --> B[ChannelManager]
  
  %% 通道层
  B --> C[ProtocolChannel1]
  B --> D[ProtocolChannel2]
  B --> E[ProtocolChannel3]
  %% 共享线程池
  subgraph "线程池ThreadPool"
    F[Thread1]
    G[Thread2]
    H[Thread3]
  end

  %% 双向消息队列
  subgraph "ProtocolChannel1 消息队列"
    Q1["消息队列1"]
    Q1P["消息队列2"]
  end
  subgraph "ProtocolChannel2 消息队列"
    Q2["消息队列1'"]
    Q2P["消息队列2'"]
  end
  subgraph "ProtocolChannel3 消息队列"
    Q3["消息队列1'"]
    Q3P["消息队列2'"]
  end
  %% 节点层
  C --> N1[Node1]
  C --> N2[Node2]
  D --> N1P[Node1']
  D --> N2P[Node2']
  E --> N1Q[Node1'']
  E --> N2Q[Node2'']

  %% 通信流程
  N1 -->|push| Q1
  N2 -->|push| Q1P
  N1P -->|push| Q2
  N2P -->|push| Q2P
  N1Q -->|push| Q3
  N2Q -->|push| Q3P
  F .->|pop| Q1
  G .->|pop| Q2
  H .->|pop| Q3P
```