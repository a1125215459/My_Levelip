# Level-IP 复刻指南

> 从上至下，从基础设施到核心协议，逐步重建完整的用户态 TCP/IP 协议栈。

---

## 前导知识

复刻前需要掌握：
- **C 语言**：指针、结构体、宏、内存管理
- **Linux 系统编程**：线程(pthread)、信号、TUN/TAP 设备、UNIX domain socket
- **网络协议基础**：Ethernet、ARP、IPv4、ICMPv4、TCP（状态机、三次握手、重传）
- **Makefile**：基本的编译规则

---

## 总的架构

```
┌──────────────────────────────────────────────────────────────┐
│                   用户空间 (Userspace)                        │
│                                                              │
│  ┌─────────┐  ┌──────────────────┐  ┌────────────────────┐  │
│  │  curl /  │  │     IPC 层       │  │    Level-IP 守护进程  │  │
│  │  应用    │◄─┤(UNIX domain sock)├──┤                    │  │
│  └─────────┘  └──────────────────┘  │  ┌──────────────┐   │  │
│       │                             │  │  Socket 层   │   │  │
│       │  ┌──────────────────┐       │  ├──────────────┤   │  │
│       │  │  liblevelip.so   │       │  │  INET/TCP 层 │   │  │
│       │  │ (LD_PRELOAD 劫持) │       │  ├──────────────┤   │  │
│       │  └──────────────────┘       │  │  IP/ICMP     │   │  │
│       │                             │  ├──────────────┤   │  │
│       ▼                             │  │  ARP/Ethernet│   │  │
│  ┌──────────┐                       │  ├──────────────┤   │  │
│  │  Linux   │                       │  │  TUN/TAP 驱动 │   │  │
│  │ 内核网络栈│                      │  └──────────────┘   │  │
│  └──────────┘                       │        ▲            │  │
└─────────────────────────────────────┼────────┼────────────┘  │
                                      │  ┌─────┴─────┐        │
                                      │  │  Timer    │        │
                                      │  └───────────┘        │
                                      └───────────────────────┘
```

---

## 复刻顺序（推荐）

### Phase 0：项目骨架

| 步骤 | 文件 | 内容 | 说明 |
|---|---|---|---|
| 1 | `Makefile` | 编译规则、清理规则 | 从第一步就能编译验证 |
| 2 | `include/syshead.h` | 聚合所有系统头文件 | 所有模块的基础依赖 |
| 3 | `include/basic.h` | `CLEAR(x)` 宏 | 简单的 memset 清零 |

> **验证点：** `make` 至少能跑起来，虽然还没有源文件。

---

### Phase 1：基础设施

**依赖关系：无内部依赖，只依赖系统头文件**

| 步骤 | 文件 | 内容 | 关键导出 |
|---|---|---|---|
| 4 | `include/list.h` | 双向循环链表 | `list_add`、`list_del`、`list_entry`、`list_for_each` |
| 5 | `include/utils.h` + `src/utils.c` | 工具函数 | `checksum()`、`print_debug()`、`run_cmd()` |
| 6 | `include/cli.h` + `src/cli.c` | 命令行解析 | `parse_cli()`、全局 `debug` |
| 7 | `include/wait.h` | 等待/唤醒原语 | `wait_sleep()`、`wait_wakeup()` |
| 8 | `include/timer.h` + `src/timer.c` | 定时器系统 | `timer_oneshot()`、`timer_add()` |
| 9 | `include/list.h` 扩展 | 链表遍历宏 | `list_entry`、`list_for_each_safe` |
| 10 | `include/ipc.h` | IPC 协议定义 | 消息结构体（不急着实现） |

> **验证点：** 编译以上 `.c` 文件生成 `.o`，验证无编译错误

---

### Phase 2：sk_buff 和 TUN/TAP

| 步骤 | 文件 | 内容 | 关键导出 |
|---|---|---|---|
| 11 | `include/skbuff.h` + `src/skbuff.c` | 套接字缓冲区 | `alloc_skb()`、`skb_push()`、`skb_reserve()` |
| 12 | `include/tuntap_if.h` + `src/tuntap_if.c` | TUN/TAP 驱动 | `tun_init()`、`tun_read()`、`tun_write()` |

> **理解：** sk_buff 是整个协议栈中数据包流通的载体，TUN/TAP 是协议栈与内核交换数据包的通道。

---

### Phase 3：链路层 — Ethernet + ARP

**数据结构关系：**
```
Ethernet 帧头 ──► [ dst MAC | src MAC | EtherType | Payload ]
                       ↑                      ↑
                  ARP 请求/应答           IP 数据包
```

| 步骤 | 文件 | 内容 | 说明 |
|---|---|---|---|
| 13 | `include/ethernet.h` | Ethernet 帧头定义 | 帧头结构 + 调试宏 |
| 14 | `include/netdev.h` + `src/netdev.c` | 网络设备抽象 | `netdev_init()`、`netdev_transmit()`、`netdev_rx_loop()` |
| 15 | `include/arp.h` + `src/arp.c` | ARP 协议 | `arp_rcv()`、`arp_request()`、`arp_get_hwaddr()` |
| 16 | `include/dst.h` + `src/dst.c` | 邻居输出 | `dst_neigh_output()` |

**Phase 3 的拓扑：**
```
  TUN/TAP
     │
     ▼
 netdev_rx_loop()    ← 主线程循环读取
     │
     ▼
  eth_type() 解析 EtherType
     ├── 0x0806 → arp_rcv()    ← ARP 处理
     └── 0x0800 → ip_rcv()     ← IP 处理（Phase 4）

 netdev_transmit()    ← 发送函数
     │
     ▼
  tun_write()         ← 写入 TUN 设备
```

> **验证点：** 实现后可以启动 `lvl-ip`，用 `tcpdump` 看到 ARP 请求/应答

---

### Phase 4：网络层 — IPv4 + ICMPv4

| 步骤 | 文件 | 内容 | 说明 |
|---|---|---|---|
| 17 | `include/ip.h` | IPv4 头定义 | `struct iphdr`、`ip_hdr()` |
| 18 | `src/ip_input.c` | IPv4 输入 | `ip_rcv()`：校验、分用 |
| 19 | `src/ip_output.c` | IPv4 输出 | `ip_output()`：路由、构造头 |
| 20 | `include/route.h` + `src/route.c` | 路由表 | `route_init()`、`route_lookup()` |
| 21 | `include/icmpv4.h` + `src/icmpv4.c` | ICMPv4 | `icmpv4_incoming()`、`icmpv4_reply()` |

**IP 处理流程：**
```
ip_rcv() 输入
  │
  ├── 校验 IP 头 checksum
  ├── 检查协议字段
  │     ├── protocol = 1   → icmpv4_incoming()
  │     ├── protocol = 6   → tcp_in()          (Phase 6)
  │     └── protocol = 17  → (UDP 暂不支持)
  └── 投递到上层

ip_output() 输出
  │
  ├── route_lookup() 查找路由
  ├── 构造 IP 头 + 计算 checksum
  └── dst_neigh_output() → ARP → netdev_transmit()
```

> **验证点：** 启动后可以用 `ping 10.0.0.4` 收到 ICMP Echo Reply

---

### Phase 5：套接字抽象层

这是最复杂的中间层，包含三组互相引用的数据结构：

```
socket.c (syscall 实现)
    │
    ├── inet.c (AF_INET 协议族)
    │       │
    │       └── tcp_ops (TCP 协议操作表)
    │
    └── sock.c (传输层套接字)
```

**Circular dependency 处理：**
`socket.h ↔ sock.h` 存在交叉引用，通过前向声明解决。建议一起实现。

| 步骤 | 文件 | 内容 | 关键导出 |
|---|---|---|---|
| 22 | `include/sock.h` | 传输套接字结构 | `struct sock`、`sk_alloc()` |
| 23 | `include/socket.h` | 套接字抽象层 | `struct socket`、`struct sock_ops` |
| 24 | `include/inet.h` | AF_INET 协议族 | `inet_create()`、`inet_socket()` |
| 25 | `src/sock.c` | sock 实现 | `sock_init_data()`、`sock_free()` |
| 26 | `src/socket.c` | socket 实现 | `socket_lookup()`、`_socket()`、`_connect()` |
| 27 | `src/inet.c` | inet 实现 | `inet` (全局) |

**套接字生命周期：**
```
socket() → inet_create() → sk_alloc() → tcp_alloc_sock()
    │
connect() → tcp_v4_connect() → 三次握手
    │
write() → tcp_write() → tcp_send() → 数据分段 → ip_output()
    │
read()  → tcp_read()  → tcp_data_dequeue() → 从接收队列取数据
    │
close() → tcp_close() → FIN 序列
```

---

### Phase 6：TCP 协议（核心）

TCP 是整个协议栈最复杂的部分，分多步实现：

#### 6.1 TCP 基础结构

| 步骤 | 文件 | 内容 |
|---|---|---|
| 28 | `include/tcp.h` | TCP 头、TCB、状态机、函数声明 |
| 29 | `src/tcp.c` | TCP 核心：连接管理、写入/读取、RTT 估算 |

#### 6.2 TCP 数据流

| 步骤 | 文件 | 内容 |
|---|---|---|
| 30 | `include/tcp_data.h` | 队列接口声明 |
| 31 | `src/tcp_data.c` | 接收队列、乱序队列管理 |

#### 6.3 TCP 输入

| 步骤 | 文件 | 内容 |
|---|---|---|
| 32 | `src/tcp_input.c` | RFC 793 Segment Arrives 状态机处理 |

#### 6.4 TCP 输出

| 步骤 | 文件 | 内容 |
|---|---|---|
| 33 | `src/tcp_output.c` | SYN/FIN/ACK/RST 发送、数据分段、RTO 定时器 |

**TCP 核心流程：**
```
CLOSED
   │
   └── connect() → SYN_SENT
                       │
                       └── 收到 SYN+ACK → ESTABLISHED
                                             │
                                ┌────────────┼────────────┐
                                │ write()     │ read()     │ close()
                                ▼             ▼            ▼
                           tcp_write()   tcp_read()   FIN_WAIT1
                           tcp_send()    recv_queue      │
                           ip_output()   dequeue         └── FIN → TIME_WAIT
```

---

### Phase 7：组装与运行

| 步骤 | 文件 | 内容 | 说明 |
|---|---|---|---|
| 34 | `src/ipc.c` | IPC 监听器 | UNIX domain socket 服务，接收用户进程的 socket 调用 |
| 35 | `src/main.c` | 主程序 | 初始化 + 4 个线程 + 清理 |

**4 个线程：**
```
┌─ THREAD_CORE  (netdev_rx_loop)   ← 主循环：读 TUN → 协议分用
├─ THREAD_TIMERS (timers_start)    ← 定时器：RTO、延迟 ACK
├─ THREAD_IPC   (start_ipc_listener) ← IPC：处理用户进程请求
└─ THREAD_SIGNAL(stop_stack_handler)  ← 信号：Ctrl+C 优雅退出
```

---

### Phase 8：用户空间工具（可选）

| 步骤 | 文件 | 内容 |
|---|---|---|
| 36 | `tools/liblevelip.h` | LD_PRELOAD 劫持库头文件 |
| 37 | `tools/liblevelip.c` | 劫持 libc socket 调用 ← 转发给 IPC |
| 38 | `tools/level-ip` | 启动脚本 |
| 39 | `apps/curl/curl.c` | 测试应用 |

---

## 依赖关系图速览

```
main.c ─────────────────────────────────────────────────────────────┐
  │                                                                  │
  ├── cli.c ─── syshead.h                                           │
  ├── tuntap_if.c ─── syshead.h, utils.h, basic.h                   │
  ├── utils.c ─── syshead.h, utils.h                                │
  │                                                                  │
  ├── ipc.c ─── syshead.h, utils.h, ipc.h, socket.h                 │
  │                        │                                        │
  ├── socket.c ─── syshead.h, utils.h, socket.h, inet.h, wait.h, timer.h
  │    │                  │                                           │
  │    ├── inet.c ─── syshead.h, inet.h, socket.h, sock.h, tcp.h, wait.h
  │    │                  │                                           │
  │    ├── sock.c ─── syshead.h, sock.h, socket.h                    │
  │    │                  │                                           │
  │    └── tcp.c ◄─── tcp.h, sock.h, ip.h, timer.h, wait.h          │
  │         │            │                                           │
  │         ├── tcp_input.c ◄─── tcp.h, tcp_data.h, skbuff.h, sock.h│
  │         ├── tcp_output.c ◄─── tcp.h, ip.h, skbuff.h, timer.h    │
  │         └── tcp_data.c ◄─── tcp.h, list.h                       │
  │                                                                  │
  ├── ip_input.c ─── ip.h, icmpv4.h, tcp.h, skbuff.h                │
  ├── ip_output.c ─── ip.h, dst.h, route.h, skbuff.h                │
  │     │                                                             │
  ├── route.c ─── route.h, dst.h, netdev.h, ip.h, list.h            │
  │     │                                                             │
  ├── dst.c ─── dst.h, ip.h, arp.h                                  │
  │     │                                                             │
  ├── arp.c ─── arp.h, netdev.h, skbuff.h, list.h                   │
  │     │                                                             │
  ├── icmpv4.c ─── icmpv4.h, ip.h, ethernet.h, utils.h              │
  │     │                                                             │
  ├── netdev.c ─── netdev.h, ethernet.h, arp.h, ip.h, skbuff.h, tuntap_if.h
  │     │                                                             │
  ├── timer.c ─── timer.h, socket.h                                 │
  │                                                                  │
  └── skbuff.c ─── skbuff.h, list.h                                 │
```

**`◄───` 表示"依赖箭头方向"**（箭头指向被依赖者）

---

## 建议的开发节奏

### 每个 Phase 的流程

1. **写头文件**（`.h`）：定义结构体、声明函数
2. **写实现**（`.c`）：逐个函数实现
3. **写 Makefile 规则**：把新的 `.o` 加入链接
4. **编译验证**：`make` 通过
5. **功能测试**：
   - Phase 3：启动后用 `tcpdump` 看 ARP 是否正常
   - Phase 4：`ping 10.0.0.4` 看 ICMP 是否响应
   - Phase 6：`curl` 经过 LD_PRELOAD 能否发起 TCP 连接

### 调试手段

| 工具 | 用途 |
|---|---|
| `make debug` | 启用 AddressSanitizer + ThreadSanitizer |
| `tcpdump -i any host 10.0.0.4 -n` | 抓包观察网络交互 |
| `gdb` | 单步调试（注意多线程） |
| 各模块的 `DEBUG_xxx` 宏 | 打开特定模块的日志输出 |

---

## 参考资源

- [项目 README](Readme.md)
- 原始博客系列：
  - [Part 1: Ethernet & ARP](http://www.saminiir.com/lets-code-tcp-ip-stack-1-ethernet-arp)
  - [Part 2: IPv4 & ICMPv4](http://www.saminiir.com/lets-code-tcp-ip-stack-2-ipv4-icmpv4)
  - [Part 3: TCP Handshake](http://www.saminiir.com/lets-code-tcp-ip-stack-3-tcp-handshake)
  - [Part 4: TCP Data & Socket API](http://www.saminiir.com/lets-code-tcp-ip-stack-4-tcp-data-flow-socket-api)
  - [Part 5: TCP Retransmission](http://www.saminiir.com/lets-code-tcp-ip-stack-5-tcp-retransmission)
- RFC 793 (TCP), RFC 826 (ARP), RFC 791 (IP), RFC 792 (ICMP)
