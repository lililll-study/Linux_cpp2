# 一 用户态协议栈实现

简历可写内容：

自我评价|专业技能：使用dpdk实现过一个发包工具，能够做到多少多少

## 1 基础介绍

### （1）虚拟机网卡状态

桥接：虚拟机网卡和Windows网卡平级的。两者在同一个局域网内。

复制网卡状态。

nat：相当于主机作为路由器，虚拟机做子网

host only：仅主机模式，只有Windows的机器能互相通信，不能对外通信。

![948cd271-7f64-4187-8b82-251aedc7f7cd](U:\linux_first\02\pictures\948cd271-7f64-4187-8b82-251aedc7f7cd.png)

 1.多队列网卡

rte_eth_rx_buff

 2.hugepage 大页

### （2）用DPDK实现接收数据

（常规的数据接收流程）网卡 -> driver -> tcp/ip -> posix api（recv send）-> redis nginx 

（DPDK-- 旁路结构）网卡接收数据有中断，告诉CPU数据在哪，然后去取数据。每个中断都有一个消息队列rbuffer。

```bash
            dpdk ——> 网卡

                uio

                vfio

            dpdk ——> tcp/ip

                kni
```

**sk_buff（socket buffer）** 是Linux内核中用于**管理网络数据包的核心数据结构**。通常使用下面的数据结构保存

1 **哈希表（Hash Table）**

* **五元组连接跟踪**（最常用）

* 根据IP/端口快速查找连接
  
  

2. **无锁队列（Lock-free Queue）**

* 生产者-消费者模型

* 多核之间的数据包传递
  
  

3. **红黑树（RB-Tree）**

* 需要**有序遍历**

* 定时器管理（按超时时间排序）

* IP路由表查找
  
  

### 对于海量sk_buff场景：

| 应用场景      | 推荐数据结构            | 原因          |
| --------- | ----------------- | ----------- |
| **连接跟踪表** | 哈希表 + LRU         | O(1)查找，自动淘汰 |
| **数据包队列** | 无锁环形队列            | 高并发，无锁      |
| **定时器管理** | 时间轮（Timing Wheel） | O(1)插入/删除   |
| **IP路由表** | 压缩前缀树（LC-Trie）    | 高效前缀匹配      |
| **流量统计**  | Per-CPU计数器        | 避免缓存行伪共享    |
| **包缓存**   | 内存池（Mempool）      | 快速分配/回收     |

### （3） dpdk的作用？

为什么dpdk要加上巨页，为了提升网卡的处理能力。网卡是把电信号转化为数字信号，速度很快。限制网络吞吐量的是，协议层本身。通过巨页就不需要频繁更换内存。

巨页通过减少**TLB（页表缓存）缺失**，来提升内存访问效率。当处理大量数据包时，使用常规4KB页会导致TLB频繁缺失，引发性能瓶颈。而使用2MB甚至1GB的巨页，能让TLB的覆盖范围大大增加，从而提升性能。

应用场景

1 大量的数据备份-dpdk-rdma（提升网络吞吐量1s处理的总量）

2 网关 防火墙

#### 1 dpdk能不能提升redis qps？不明显

直接改造Redis**成本高、收益不明确**，但通过**DPDK透明代理**的方式可以**极大提升**QPS。

#### 2 dpdk能不能减少Nginx延迟？不明显

DPDK对单次请求的延迟**改善有限**，但能**大幅提升并发处理能力**和**稳定性**。

#### 3 dpdk能不能提升网络的吞吐量？能

DPDK对网络吞吐量的提升效果最为显著，这是它最核心的优势。

* **原理**：DPDK通过“**内核旁路（Kernel Bypass）**”技术，让应用程序直接与网卡交互，绕过了Linux内核协议栈这个性能瓶颈。这避免了传统方式中的**频繁中断、上下文切换和数据多次拷贝**开销。
  
  

### （4） 实现协议栈推荐

a. ntytcp

b. 4.4BSD---f-stack

c. mtcp

d. lwip

### （5）环境配置

```c
1.查看是否支持多队列
cat /proc/interrupts | grep eth0

2.多开vm网络适配器，并修改vmx文件，添加参数


3.配置hugepage
vim /etc/default/grub
sudo update-grub
sudo reboot

4.下载dpdk源码后，进入下面
cd share/linux_first/02/2.4_dpdk/

5. ./usertools/dpdk-setup.sh 
39，43

6. 43步骤如下
Unloading any existing DPDK UIO module
Loading uio module
Loading DPDK UIO module
插入一个网卡驱动UIO

44 vfio
Unloading any existing VFIO module
Loading VFIO module
chmod /dev/vfio
OK

Option: 45 kni
Unloading any existing DPDK KNI module
Loading DPDK KNI module

绑定dpdk设备到网卡Option: 49
Network devices using kernel driver
===================================
0000:02:01.0 '82545EM Gigabit Ethernet Controller (Copper) 100f' if=eth3 drv=e1000 unused=igb_uio,vfio-pci 
0000:03:00.0 'VMXNET3 Ethernet Controller 07b0' if=eth0 drv=vmxnet3 unused=igb_uio,vfio-pci *Active*
0000:0b:00.0 'VMXNET3 Ethernet Controller 07b0' if=eth1 drv=vmxnet3 unused=igb_uio,vfio-pci 
0000:13:00.0 'VMXNET3 Ethernet Controller 07b0' if=eth2 drv=vmxnet3 unused=igb_uio,vfio-pci 

No 'Baseband' devices detected
==============================

No 'Crypto' devices detected
============================

No 'Eventdev' devices detected
==============================

No 'Mempool' devices detected
=============================

No 'Compress' devices detected
==============================

No 'Misc (rawdev)' devices detected
===================================



```



编译问题

```bash
FATAL: Cannot use IOVA as 'PA' since physical addresses are not available
修改为sudo 运行即可（缺少root权限）


No available hugepages reported in hugepages-1048576kB
```



## 2 实现UDP数据包接收

### （1）DPDK的工作原理：

1. **绕过内核**：DPDK直接控制网卡硬件，数据包不经过Linux内核协议栈

2. **零拷贝**：数据直接从网卡DMA到用户空间内存(mbuf)

3. **轮询模式**：不使用中断，而是主动轮询接收数据包（提高性能）

4. **大页内存**：使用hugepages减少TLB miss
   
   

### （2）和socket编程区别

网卡 → 内核协议栈 → socket缓冲区 → 用户程序
（多次拷贝，中断处理，上下文切换）

网卡 → DMA → mbuf内存池 → 用户程序
（零拷贝，无中断，直接访问）



```bash
mbuf 结构:
┌─────────────────┐
│ mbuf 元数据      │  (包长度、端口等)
├─────────────────┤ ← rte_pktmbuf_mtod 返回这里
│ 以太网头(14字节) │  ← ethdr 指向这里
├─────────────────┤
│ IP头(20字节)     │
├─────────────────┤
│ UDP头(8字节)     │
├─────────────────┤
│ 应用数据         │
└─────────────────┘


内存布局:
┌────────────────┐ ← mbuf数据起始
│  以太网头(14)  │
├────────────────┤ ← iphdr 指向这里 (偏移14字节)
│  IP头(20)      │
├────────────────┤
│  UDP头(8)      │
├────────────────┤
│  数据 "1111"   │
└────────────────┘
```

### （3）完整的数据包结构（实际收到的包）

```bash
内存地址    内容                 指针
──────────────────────────────────────────
0x1000     目标MAC (00:50:56:c0:00:08)
0x1006     源MAC   (00:0c:29:d1:54:b7)  ← ethdr
0x100C     类型 (0x0800 = IPv4)
──────────────────────────────────────────
0x100E     版本/IHL, TOS, 总长度
0x1012     标识, 分片, TTL
0x1016     协议 (17 = UDP)              ← iphdr
0x1018     校验和
0x101A     源IP (192.168.137.1)
0x101E     目标IP (192.168.137.201)
──────────────────────────────────────────
0x1022     源端口 (8080)
0x1024     目标端口 (2000)              ← udphdr
0x1026     UDP长度 (12)
0x1028     UDP校验和
──────────────────────────────────────────
0x102A     '1' '1' '1' '1' '\0'       ← (char *)(udphdr + 1)
0x102F     这就是 "1111"
```

数据接收流程

```bash
1. 网卡收到数据包
        ↓
2. DMA传输到mbuf内存
        ↓
3. rte_eth_rx_burst() 返回mbuf指针
        ↓
4. rte_pktmbuf_mtod() 获取数据起始 → 以太网头
        ↓
5. 偏移14字节 → IP头
        ↓
6. 偏移20字节 → UDP头  
        ↓
7. 偏移8字节  → 应用数据 "1111"
        ↓
8. printf 打印数据
        ↓
9. rte_pktmbuf_free() 释放mbuf
```





## 3 TCP

为什么TCP包不带长度，而UDP带

通过ack 和 seq来确认长度

![ff0a66c4-8a80-4926-87a8-59f0e0478b8f](file:///U:/linux_first/02/pictures/ff0a66c4-8a80-4926-87a8-59f0e0478b8f.png)



代码逻辑：

初始化网卡端口

while主循环

rx_burst从dpdk环形队列中接收num_recv个数据包出来

for循环遍历所有的包进行解析

先用pktmbuf_mtod解析以太网头，判断是不是IPV4的ip包

如果是ip包，拿出ip hdr，ip头。并判断是不是UDP包

如果是UDP，那么解析UDP头，并取出源mac目的mac，源ip目的ip，源port目的port

然后重新打一个包并回发回去，这里需要实现一个encode_udp_pkt

然后调用tx_burst把数据包发回去



作业：实现一个TCP发包工具 ./pktgen -t -dmac xxxxx  -sip xxxxxx -dipxx.xx.xx.xx -sport xxx -dport xxx

用于后面做测试

### （1）TCP三次握手的实现

```bash
client        server
         →
        syn(seqnum=1234)
         ←
       syn/ack(acknum=1235,seqnum=4567)
         →
        ack
```

注意：客户端发给服务器的syn和服务器回的ack/syn的seqnum是不同的，二者没有必然联系。

seqnum：代表我方发包的序号

acknum：代表我方接受的序号



问题：ACK最后一次包一直在发，原因是什么？

通常意味着**对端没有正确收到你的数据**，或者**对端的接收窗口（Receive Window）为 0，导致你不断重传**。



TCP状态迁移



seqnum的单位是什么？

TCP序列号（seqnum）的单位是**字节（byte）**。

* TCP序列号以**字节**为单位进行计数

* 每个字节的数据都有一个唯一的序列号

* 序列号表示的是数据流中的位置

// 假设当前 seqnum = 1000
// 发送 100 字节数据
// 下一个 seqnum = 1000 + 100 = 1100

作用是保证数据的完整性，以及对端发送的ack确认收到了多少数据。

ack在有数据传输的过程中，是随着数据包一起发送回去的。但是如果没有数据了，会单独发送一个ack确认信息。









### （2）TCP传输技术



#### 1 滑动窗口

三种数据状态：

1. 已发送已确认

2. 已发送未确认

3. 未发送未确认
   
   

延迟确认：**不立即发送ACK确认包，而是等待一段时间（通常最多200ms）**，期望在这段时间内如果有数据要发送，可以将ACK与数据一起发送，减少网络包数量。----延迟确认是用来做接收的。

滑动窗口：因此在发送端会有一段内存，包含有两个指针分别位于三种状态之间，如果1部分的数据来了，那么两个指针都后移，形成了一种滑动确认机制，<mark>这就是滑动窗口的概念</mark>。--用来形容发送方的。





#### 2 拥塞控制

拥塞控制和慢启动是TCP用来**防止网络过载**的机制，它们共同决定了发送数据的速度。
一、核心概念

1. **拥塞窗口（cwnd - congestion window）**

* 发送端**根据网络状况动态调整**的窗口大小

* 决定了**在不引起网络拥塞的前提下**，可以发送多少数据

2. **接收窗口（rwnd - receiver window）**

* 接收端**缓冲区大小**，告诉发送端能接收多少数据

* 你的代码中的 `TCP_INITIAL_WINDOW = 14600`

3. **实际发送窗口**

```c
实际发送窗口 = min(cwnd, rwnd)
```



#### 3 慢启动（Slow Start）

1. **原理**

* 开始时**以很小的速率发送**（通常1个MSS）

* 每收到一个ACK，cwnd就**翻倍增长**

* 直到达到慢启动阈值（ssthresh）

```c
cwnd
  ^
  |              / 
  |            /
  |          /        ← 指数增长
  |        /
  |      /
  |    /
  |  /
  |/
  +------------------------→ 时间
  慢启动阶段

第1个RTT: cwnd = 1  (发送1个包)
第2个RTT: cwnd = 2  (发送2个包)
第3个RTT: cwnd = 4  (发送4个包)
第4个RTT: cwnd = 8  (发送8个包)
...
```

拥塞状态通过往返时间RTT确认

通过seq（发送）和ack（对方）来确认



#### 4 超时重传



## 4 EPOLL实现DPDK

### 1 架构图



![78c577de-ba94-4636-b8fa-764b1f791f99](file:///U:/linux_first/02/pictures/78c577de-ba94-4636-b8fa-764b1f791f99.png)

所有revbuf和sndbuf是单独一个tcp连接的。

实现 思路

```c
网卡
  ↓
main线程：rte_eth_rx_burst() 接收
  ↓
ring->in (输入环形队列)
  ↓
pkt_process线程：rte_ring_mc_dequeue_burst() 取出
  ↓
┌─────────────────────────────────────┐
│ 1. 解析以太网头                     │
│ 2. 学习ARP (IP→MAC映射)            │
│ 3. 协议分发: UDP/TCP/其他          │
│ 4. 处理应用层数据                   │
│ 5. 处理KNI请求                      │
└─────────────────────────────────────┘
  ↓
UDP/TCP应用处理，构造响应
  ↓
ring->out (输出环形队列)
  ↓
main线程：rte_eth_tx_burst() 发送
  ↓
网卡
```



TCP应用处理：




```c
接收TCP包 → 校验和验证 → 查找连接 → 状态机处理 → 释放包
```



### 2 用户态TCP协议 如何实现并发？

为什么使用红黑树来实现应用层的epoll？

因为epfd是每一次来数据都需要查找的，rbtree是强查找的，但是它为O(n)，哈希为O(1)，那为什么不用hash呢？因为hash需要开辟很大的连续内存。b树查找，查找性能没有红黑树高，内存是用块来管理的。因此用红黑树比较好，内存增长没有副作用，✅运行不连续的内存存在。

1. 查找性能不低

2. 内存线性增长

hash用在数据量很大，而且业务简单时，效率高。



3 面试问题

红黑树的数据结构？



epoll的三个接口的功能分别是什么？怎么实现的？



协议栈有数据来，如何通知epoll模块的？



从整集到就绪的条件是什么？




