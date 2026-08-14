# 一 用户态协议栈实现

简历可写内容：

自我评价|专业技能：使用dpdk实现过一个发包工具，能够做到多少多少

## （1） 为什么要用到巨页？

操作系统将物理内存划分为固定大小的**页（Page）**，通过**页表**将虚拟地址映射到物理地址。

TLB是CPU内部的一个**小而快的缓存**，用于存储最近使用的**虚拟地址到物理地址的映射关系**。CPU访问内存的流程是，CPU产生虚拟地址，然后查询TLB（缓存），如果命中则直接得到物理地址，如果没有命中则需要查询页表 ❌ 慢！。

常规的CPUTLB大小只有4KB左右（标准页），这会导致缓存频繁未命中，那么CPU进行内存访问的开销就会增大。

而DPDK则是总网卡直接DMA（直接访问内存）到内存，并且使用Linux支持的巨页（2M），能大大提高缓存命中。这使得访问内存的速度有着极大的提升 ——> 提高了网络吞吐量QPS。

总结：

* **绕过内核**：DPDK直接控制网卡硬件，数据包不经过Linux内核协议栈

* **零拷贝**：数据直接从网卡DMA到用户空间内存(mbuf)

* **轮询模式**：不使用中断，而是主动轮询接收数据包（提高性能）

* **大页内存**：使用hugepages减少TLB miss
  
  
  
  

虚拟机网卡状态

桥接：虚拟机网卡和Windows网卡平级的。两者在同一个局域网内。复制网卡状态。

nat：相当于主机作为路由器，虚拟机做子网

host only：仅主机模式，只有Windows的机器能互相通信，不能对外通信。



dpdk实现架构图

![948cd271-7f64-4187-8b82-251aedc7f7cd](U:\linux_first\02\pictures\948cd271-7f64-4187-8b82-251aedc7f7cd.png)

 1.多队列网卡：rte_eth_rx_buff

 2.hugepage



## （2）用DPDK实现接收数据

（常规的数据接收流程）

网卡 -> driver -> tcp/ip -> posix api（recv send）-> redis nginx 

（DPDK-- 旁路结构）

网卡接收数据有中断，告诉CPU数据在哪，然后去取数据。每个中断都有一个消息队列rbuffer。

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
  
  

**对于海量sk_buff场景：**

| 应用场景      | 推荐数据结构            | 原因          |
| --------- | ----------------- | ----------- |
| **连接跟踪表** | 哈希表 + LRU         | O(1)查找，自动淘汰 |
| **数据包队列** | 无锁环形队列            | 高并发，无锁      |
| **定时器管理** | 时间轮（Timing Wheel） | O(1)插入/删除   |
| **IP路由表** | 压缩前缀树（LC-Trie）    | 高效前缀匹配      |
| **流量统计**  | Per-CPU计数器        | 避免缓存行伪共享    |
| **包缓存**   | 内存池（Mempool）      | 快速分配/回收     |

## （3） dpdk的作用？

为什么dpdk要加上巨页，为了提升网卡的处理能力。网卡是把电信号转化为数字信号，速度很快。限制网络吞吐量的是，协议层本身。通过巨页就不需要频繁更换内存。

巨页通过减少**TLB（页表缓存）缺失**，来提升内存访问效率。当处理大量数据包时，使用常规4KB页会导致TLB频繁缺失，引发性能瓶颈。而使用2MB甚至1GB的巨页，能让TLB的覆盖范围大大增加，从而提升性能。

应用场景

1 大量的数据备份-dpdk-rdma（提升网络吞吐量1s处理的总量）

2 网关 防火墙

**1 dpdk能不能提升redis qps？不明显**

直接改造Redis**成本高、收益不明确**，但通过**DPDK透明代理**的方式可以**极大提升**QPS。

**2 dpdk能不能减少Nginx延迟？不明显**

DPDK对单次请求的延迟**改善有限**，但能**大幅提升并发处理能力**和**稳定性**。

**3 dpdk能不能提升网络的吞吐量？能**

DPDK对网络吞吐量的提升效果最为显著，这是它最核心的优势。

* **原理**：DPDK通过“**内核旁路（Kernel Bypass）**”技术，让应用程序直接与网卡交互，绕过了Linux内核协议栈这个性能瓶颈。这避免了传统方式中的**频繁中断、上下文切换和数据多次拷贝**开销。
  
  

<mark>实现协议栈推荐</mark>

a. ntytcp

b. 4.4BSD---f-stack

c. mtcp

d. lwip



## 2 实现UDP数据包接收

### （1）环境配置

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

### （3）数据接收流程

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

### （4）代码逻辑

初始化网卡端口

while主循环

rx_burst从dpdk环形队列中接收num_recv个数据包出来

for循环遍历所有的包进行解析

先用pktmbuf_mtod解析以太网头，判断是不是IPV4的ip包

如果是ip包，拿出ip hdr，ip头。并判断是不是UDP包

如果是UDP，那么解析UDP头，并取出源mac目的mac，源ip目的ip，源port目的port

然后重新打一个包并回发回去，这里需要实现一个encode_udp_pkt

然后调用tx_burst把数据包发回去

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





# 二 TCP协议栈

## 八股

### （1）为什么TCP包不带长度，而UDP带？

UDP头部是8字节{ s/d port， dgram_len， cksum}

TCP头部是20字节

因为TCP是流式协议（stream），将数据视为连续的字节流，没有边界

所以通过总长度-头长度来计算数据长度。



![ff0a66c4-8a80-4926-87a8-59f0e0478b8f](file:///U:/linux_first/02/pictures/ff0a66c4-8a80-4926-87a8-59f0e0478b8f.png)

### （2）ack 和 seq

seq发送端to接收端，表示我这次发了多少数据(len)，以及数据的标识号(seq)是多少。接收端to发送端发送ack(len + seq)，表示接收端确认收到了这些数据。



### （3）TCP三次握手的实现

```bash
client        server
         →
        syn(seqnum=x)
         ←
       syn/ack(acknum=x+1,seqnum=y)
         →
        ack(seqnum=x+1,ack=y+1)
```

第一次发送端SYN：        seq=x，首次握手没有ack

第二次接收端SYN+ACK：ack=x+1, seq=y

第三次客户端ACK：seq=x+1，ack=y+1

注意：客户端发给服务器的syn和服务器回的ack/syn的seqnum是不同的，二者没有必然联系。

seqnum：代表我方发包的序号

acknum：代表我方接受的序号



**问题：ACK最后一次包一直在发，原因是什么？**

通常意味着**对端没有正确收到你的数据**，或者**对端的接收窗口（Receive Window）为 0，导致你不断重传**。



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





### （4）TCP传输技术

#### 1 滑动窗口

三种数据状态：

1. 已发送已确认

2. 已发送未确认

3. 未发送未确认

滑动窗口：在发送端会有一段内存，包含有两个指针分别位于三种状态之间，如果1部分的数据来了，那么两个指针都后移，形成了一种滑动确认机制，<mark>这就是滑动窗口的概念</mark>。--用来形容发送方的。

延迟确认：**不立即发送ACK确认包，而是等待一段时间（通常最多200ms）**，期望在这段时间内如果有数据要发送，可以将ACK与数据一起发送，减少网络包数量。----延迟确认是用来做接收的。



#### 2 拥塞控制

拥塞控制和慢启动是TCP用来**防止网络过载**的机制，它们共同决定了发送数据的速度。

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





### （5）面试问题

#### 0 用户态TCP协议 如何实现并发？

#### 1 为什么使用红黑树来实现应用层的epoll？

因为epfd是每一次来数据都需要查找的，rbtree是强查找的，但是它为O(n)，哈希为O(1)，那为什么不用hash呢？因为hash需要开辟很大的连续内存。b树查找，查找性能没有红黑树高，内存是用块来管理的。因此用红黑树比较好，内存增长没有副作用，✅运行不连续的内存存在。

1. 查找性能不低

2. 内存线性增长

hash用在数据量很大，而且业务简单时，效率高。



#### 2 红黑树的数据结构？



#### 3 epoll的三个接口的功能分别是什么？怎么实现的？



#### 4 协议栈有数据来，如何通知epoll模块的？



#### 5 从整集到就绪的条件是什么？



# 二 TCP协议栈代码实现✅️

![78c577de-ba94-4636-b8fa-764b1f791f99](file:///U:/linux_first/02/pictures/78c577de-ba94-4636-b8fa-764b1f791f99.png)

<mark>三个loop，两个ring队列，</mark>代码主要分为下面的层级：

**1 硬件层 dpdk**

dpdk：通过旁路转发，零拷贝收发包，提高批量处理的性能（rte_eth_rx_burst  rte_eth_tx_burst）

**2 数据链路层**

以太网层：s/d mac，进行帧封装为了交换机转发（封装/解封装）

ARP模块：ip，mac，链表。实现ip与mac的对应（缓存/请求）

KNI：内核通信

**3 网络层**

ip协议层：s/d ip，加上协议号（TCP），路由器路由（校验和计算 路由查找）

**4 传输层**

TCP协议层：s/d port，建立连接（TCP状态机）

UDP协议层

**5 套接字层（POSIX API）**

实现nsocket()  nbind()  nlisten()  naccept()nsend()  nrecv()  nsendto()  nrecvfrom()  nclose()

**6 应用层**

UDP server 和 TCP server

```c
                  应用层
                    |
          --------------------
          |                  |
       TCP API             UDP API
          |                  |
          --------------------
                    |
              Socket抽象层
                    |
              TCP状态机
                    |
          --------------------
          |                  |
        IPv4               ARP                
          |
       Ethernet
          |
        DPDK
          |
        NIC
```



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

TCP应用处理： 接收TCP包 → 校验和验证 → 查找连接 → 状态机处理 → 释放包

```

## 1 主函数实现 （pthread-1）

（1）环形缓冲区

使用单例模式创建ring缓冲区，后续直接调用实例创建。

使用生产者消费者模式创建in和out ring，前者主线程收包后放入，后者工作线程处理完成后放入

```c
static struct inout_ring *ringInstance(void) {
    // 1. 检查全局指针是否为NULL
    if (rInst == NULL) {
        // 2. 为NULL则分配内存
        rInst = rte_malloc("in/out ring", sizeof(struct inout_ring), 0);
        // 3. 清零初始化
        memset(rInst, 0, sizeof(struct inout_ring));
    }
    // 4. 返回全局实例
    return rInst;
}

接着调用rte_ring_create 创建输入输出队列
队列名称     队列大小   NUMA节点（当前CPU的socket）                 标志位
 rte_ring_create("in ring", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
rte_ring_create("out ring", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
```

rte_ma此处使用DPDK的 `rte_malloc` 在**巨页**上分配内存，并将分配的内存全部置0



（2）启动工作线程

将不同的功能绑定到独立的CPU核心，形成dpdk的多核处理

```c
#if ENABLE_MULTHREAD
    lcore_id = rte_get_next_lcore(lcore_id, 1, 0);
    rte_eal_remote_launch(pkt_process, mbuf_pool, lcore_id);
#endif

#if ENABLE_UDP_APP
    lcore_id = rte_get_next_lcore(lcore_id, 1, 0);
    rte_eal_remote_launch(udp_server_entry, mbuf_pool, lcore_id);
#endif

#if ENABLE_TCP_APP
    lcore_id = rte_get_next_lcore(lcore_id, 1, 0);
    rte_eal_remote_launch(tcp_server_entry, mbuf_pool, lcore_id);
#endif
```

（3）mainloop--while(1)

**1：收包流程**

    dpdk从网卡批量接收数据包  rte_eth_rx_burst

    将数据包放入ring队列



**2：发包流程**

    调用rte_ring_sc_dequeue_burst从ring取出数据

    通过rte_eth_tx_burst发送数据

    并释放mbuf的内存rte_pktmbuf_free





## 2 数据包处理(pthread -2)

```c
static int pkt_process(void *arg) {

    struct rte_mempool *mbuf_pool = (struct rte_mempool *)arg;
    struct inout_ring *ring = ringInstance();

```

拿到入参ring队列，进入循环



```c
    while (1) {
        // 从ring中取出数据包，max32个
        struct rte_mbuf *mbufs[BURST_SIZE];
        unsigned num_recvd = rte_ring_mc_dequeue_burst(ring->in, (void**)mbufs, BURST_SIZE, NULL);
```

取出数据包



```c
        for (i = 0;i < num_recvd;i ++) {// 处理每个数据包

            struct rte_ether_hdr *ehdr = rte_pktmbuf_mtod(mbufs[i], struct rte_ether_hdr*);
            if (ehdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
                struct rte_ipv4_hdr *iphdr =  rte_pktmbuf_mtod_offset(mbufs[i], struct rte_ipv4_hdr *, 
                sizeof(struct rte_ether_hdr));

#if 1 // arp table
                // 从每个IP包获得IP-MAC映射关系，并填充ARP表，后续发送数据可以直接查表
                ng_arp_entry_insert(iphdr->src_addr, ehdr->s_addr.addr_bytes);

#endif
                // 协议分发
                if (iphdr->next_proto_id == IPPROTO_UDP) {
                    udp_process(mbufs[i]);
                } else if (iphdr->next_proto_id == IPPROTO_TCP) {
                    ng_tcp_process(mbufs[i]);
                } else {
                    rte_kni_tx_burst(global_kni, mbufs, num_recvd);
                    //printf("tcp/udp --> rte_kni_handle_request\n");
                }
            } else {
            // ifconfig vEth0 192.168.0.119 up
                rte_kni_tx_burst(global_kni, mbufs, num_recvd);
                //printf("ip --> rte_kni_handle_request\n");
            }
        }
        // 处理KNI请求（运行DPDK与内核通信）
        rte_kni_handle_request(global_kni);
#if ENABLE_UDP_APP
        // 发送UDP响应
        udp_out(mbuf_pool);
#endif
#if ENABLE_TCP_APP
        // 发送TCP响应
        ng_tcp_out(mbuf_pool);
#endif
    }
    return 0;
}

```

处理数据包，结构体化以太网头（数据链路层），并过滤IPV4的数据。—— rte_pktmbuf_mtod；rte_cpu_to_be_16；

然后解析IP头（网络层）。——rte_pktmbuf_mtod_offset

接着进行协议分发（传输层），分析是UDP还是TCP的数据，如果都不是则进行KNI传入内核处理。——ng_tcp_process

最后发送响应。——ng_tcp_out





## 3 TCP应用服务器(pthread-3)

```c
static int tcp_server_entry(__attribute__((unused))  void *arg)  {
    int listenfd = nsocket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        return -1;
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(struct sockaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(9999);
    nbind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

    nlisten(listenfd, 10);}



    int epfd = epoll_create(1);    

    struct epoll_event ev;    
    ev.events = EPOLLIN;    
    ev.data.fd = listenfd;    
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    struct sockaddr_in  clientaddr;    
    socklen_t len = sizeof(clientaddr);


    while (1) {        
        struct epoll_event events[1024] = {0};        
        int nready = epoll_wait(epfd, events, 1024, -1);

        int i = 0;        
        for (i = 0;i < nready;i ++) {
            int connfd = events[i].data.fd;            
            if (connfd == listenfd) {                                
                int clientfd = naccept(listenfd, (struct sockaddr*)&clientaddr, &len);    
                printf("accept finshed: %d\n", clientfd);                
                ev.events = EPOLLIN;                
                ev.data.fd = clientfd;                
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);    

            } else if (events[i].events & EPOLLIN) {                
                char buffer[1024] = {0};                                
                int count = nrecv(connfd, buffer, 1024, 0);

                if (count == 0) { // disconnect                    
                    printf("client disconnect: %d\n", connfd);                    
                    nclose(connfd);                    
                    epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, NULL);    
                    continue;                
                }                
                printf("RECV: %s\n", buffer);                
                count = nsend(connfd, buffer, count, 0);                
                printf("SEND: %d\n", count);            
            }
        }
    }
    nclose(listenfd);
}
```

可见此处是完全调用TCP的epoll实现方案，只是更改了API的调用。从原先Linux内核提供的epoll接口，修改为了用户态封装的epoll接口。这部分封装会在下面介绍。





# 三 底层封装详解

## 1 多线程解构

NUMA节点：

```bash
NUMA节点0 (Socket 0)          NUMA节点1 (Socket 1)
┌─────────────────┐          ┌─────────────────┐
│ 核心0  核心1    │          │ 核心2  核心3    │
│  ↓      ↓      │          │  ↓      ↓      │
│ L3 Cache        │          │ L3 Cache        │
│  ↓              │          │  ↓              │
│ 本地内存 (32GB) │←─互联──→│ 本地内存 (32GB) │
└─────────────────┘          └─────────────────┘
```

DPDK将每个cpu核心抽象为lcore，每个具有唯一id，dpdk维护lcore的状态

```c
// DPDK内部维护lcore状态
struct lcore_config {
    pthread_t thread_id;        // 线程ID
    int state;                  // 状态：WAIT/RUNNING/FINISHED
    void *arg;                  // 线程参数
    int (*f)(void *);          // 线程函数
};

// 状态转换
RTE_LCORE_STATE_WAIT     // 等待被分配任务
RTE_LCORE_STATE_RUNNING  // 正在运行
RTE_LCORE_STATE_FINISHED // 已完成
RTE_LCORE_STATE_PREPARED // 已准备
```

底层就涉及到了线程池的实现原理：

（1） **rte_eal_remote_launch()实现**

```c
int rte_eal_remote_launch(int (*f)(void *), void *arg, unsigned slave_id) {
    struct lcore_config *lc = &lcore_config[slave_id];
    // 1. 检查lcore是否可用
    if (lc->state != RTE_LCORE_STATE_WAIT) {
        return -EBUSY;
    // 2. 设置线程函数和参数
    lc->f = f;
    lc->arg = arg;}

    // 3. 唤醒等待的线程（通过条件变量）
    pthread_mutex_lock(&lc->mutex);
    lc->state = RTE_LCORE_STATE_RUNNING;
    pthread_cond_signal(&lc->cond);
    pthread_mutex_unlock(&lc->mutex);

    return 0;
}
```

（2）线程创建

```c
// DPDK EAL初始化时，为每个lcore创建线程
static int eal_thread_start(void *arg) {
    struct lcore_config *lc = (struct lcore_config *)arg;

    // 设置CPU亲和性
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    while (1) {
        // 等待被分配任务
        pthread_mutex_lock(&lc->mutex);
        while (lc->state == RTE_LCORE_STATE_WAIT) {
            pthread_cond_wait(&lc->cond, &lc->mutex);
        }
        pthread_mutex_unlock(&lc->mutex);
        // 执行任务
        if (lc->state == RTE_LCORE_STATE_RUNNING) {
            lc->ret = lc->f(lc->arg);
            lc->state = RTE_LCORE_STATE_WAIT;  // 回到等待状态
        }
    }
    return 0;
}
```

（3）主函数中的实现

```c
int main(int argc, char *argv[]) {
    // 1. DPDK EAL初始化
    rte_eal_init(argc, argv);  // 创建所有lcore线程

    // 2. 当前运行在lcore 0（主核心）
    unsigned lcore_id = rte_lcore_id();  // lcore_id = 0

    // 3. 创建工作线程
    lcore_id = rte_get_next_lcore(lcore_id, 1, 0);  // 获取lcore 1
    rte_eal_remote_launch(pkt_process, mbuf_pool, lcore_id);  // 在lcore 1启动

    // 4. 主循环（在lcore 0上运行）
    while (1) {
        // 收包、发包、定时器管理
    }
}
```



（4）线程间通信--无锁队列

通过生产者消费者模型设计，主核心调用rte_ring_sp_enqueue_burst（生产者），工作核心调用rte_ring_mc_dequeue_burst（消费者）。

标志：

RING_F_SC_DEQ  
RING_F_MC_DEQ  

```c
┌─────────────────────────────────────────────────────────────┐
│                     核心0 (主循环)                         │
│  while(1) {                                              │
│    收包 → ring->in (生产者)                              │
│    发包 ← ring->out (消费者)                             │
│  }                                                       │
└─────────────────────────────────────────────────────────────┘
                         ↓ 输入队列
┌─────────────────────────────────────────────────────────────┐
│                     核心1 (pkt_process)                    │
│  while(1) {                                              │
│    rte_ring_mc_dequeue_burst(ring->in) ← 取包           │
│    处理包（解析协议、学习ARP）                           │
│    rte_ring_mp_enqueue_burst(ring->out) → 放包          │
│  }                                                       │
└─────────────────────────────────────────────────────────────┘
                         ↓ 输入队列
┌─────────────────────────────────────────────────────────────┐
│                     核心2 (UDP)                            │
│  udp_server_entry() {                                    │
│    nrecvfrom() ← 阻塞等待UDP数据                         │
│    处理UDP数据                                           │
│    nsendto() → 发送UDP响应                               │
│  }                                                       │
└─────────────────────────────────────────────────────────────┘
                         ↓ 输入队列
┌─────────────────────────────────────────────────────────────┐
│                     核心3 (TCP)                            │
│  tcp_server_entry() {                                    │
│    naccept() ← 阻塞等待TCP连接                           │
│    nrecv() ← 阻塞等待TCP数据                             │
│    处理TCP数据                                           │
│    nsend() → 发送TCP响应                                 │
│  }                                                       │
└─────────────────────────────────────────────────────────────┘
```





（5）线程安全

rte_ring内部使用CAS实现无锁。

```c
rte_ring的数据结构
struct rte_ring {
    uint32_t prod_head;    // 生产者头指针（CAS修改）
    uint32_t prod_tail;    // 生产者尾指针
    uint32_t cons_head;    // 消费者头指针（CAS修改）
    uint32_t cons_tail;    // 消费者尾指针
    uint32_t size;         // 队列大小（2的幂次）
    uint32_t mask;         // size - 1（用于取模）
    void *ring[];          // 数据存储区
};

// 多消费者出队（核心CAS操作）
static int rte_ring_mc_dequeue(struct rte_ring *r, void **obj_p) {
    uint32_t cons_head, cons_next;
    uint32_t prod_tail;

    do {
        // 1. 读取当前消费者头
        cons_head = r->cons_head;

        // 2. 检查队列是否为空
        prod_tail = r->prod_tail;
        if (cons_head == prod_tail) {
            return -ENOENT;  // 队列空
        }

        // 3. 计算下一个消费者头
        cons_next = cons_head + 1;

    // 4. CAS操作：尝试将cons_head从旧值更新到新值
    } while (!rte_atomic32_cmpset(&r->cons_head, cons_head, cons_next));
    //                ↑
    //        原子CAS操作！
    //        如果r->cons_head == cons_head，则更新为cons_next，返回true
    //        否则返回false，重新尝试

    // 5. 成功获取槽位，读取数据
    *obj_p = r->ring[cons_head & r->mask];

    // 6. 更新消费者尾（允许其他人看到数据已被消费）
    rte_wmb();  // 写内存屏障，确保数据读取完成
    r->cons_tail = cons_next;

    return 0;
}
```

比如两个消费线程同时修改数据，此时cons_head = 0, cons_tail = 0

当core1改完后，cons_head = 1

```c
// 假设三个工作线程同时从输入队列取包

时间点 T1：
┌─────────────────────────────────────────────────────────┐
│ 输入队列 (ring->in)                                    │
│ [包1][包2][包3][包4][包5][包6][包7][包8]              │
│  ↑                                                    │
│  cons_head = 0 (消费者头指针)                         │
└─────────────────────────────────────────────────────────┘

三个线程同时开始取包：

线程1 (Core 1)          线程2 (Core 2)          线程3 (Core 3)
    ↓                       ↓                       ↓
读取 cons_head=0         读取 cons_head=0         读取 cons_head=0
    ↓                       ↓                       ↓
CAS尝试获取包1           CAS尝试获取包1           CAS尝试获取包1
    ↓                       ↓                       ↓
成功！获取包1            失败！重试              失败！重试
    ↓                       ↓                       ↓
更新 cons_head=1         读取 cons_head=1         读取 cons_head=1
                          CAS尝试获取包2           CAS尝试获取包2
                              ↓                       ↓
                          成功！获取包2            失败！重试
                              ↓                       ↓
                          更新 cons_head=2         读取 cons_head=2
                                                    CAS尝试获取包3
                                                        ↓
                                                    成功！获取包3
                                                        ↓
                                                    更新 cons_head=3

结果：
线程1 → 包1
线程2 → 包2
线程3 → 包3

三个线程同时工作，互不阻塞！
```



互斥锁和condition用来保护TCP流：

当多个线程都需要并发访问TCP流，可能读到不一致的数据。因此在修改TCP状态、修改序列号时，需要上锁。

```c
struct ng_tcp_stream {
    // ...
    pthread_cond_t cond;    // 条件变量
    pthread_mutex_t mutex;  // 互斥锁
};

// 初始化
pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
rte_memcpy(&stream->cond, &blank_cond, sizeof(pthread_cond_t));

pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
rte_memcpy(&stream->mutex, &blank_mutex, sizeof(pthread_mutex_t));
```

在ng_tcp_handle_syn_rcvd函数中，当三次握手完成后，状态变为NG_TCP_STATUS_ESTABLISHED，此时有状态修改需要加锁。在naccept中，接收新连接的过程，也需要加锁。







作业：实现一个TCP发包工具 ./pktgen -t -dmac xxxxx -sip xxxxxx -dipxx.xx.xx.xx -sport xxx -dport xxx

用于后面做测试


