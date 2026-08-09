


#include <stdio.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <arpa/inet.h>

int global_portif = 0;

#define NUM_MBUFS 4096
#define BURST_SIZE 128
#define RX_RING_SIZE 128
#define TX_RING_SIZE 512  // vmxnet3要求至少512

static const struct rte_eth_conf port_conf_default = {
    .rxmode = { .max_rx_pkt_len = RTE_ETHER_MAX_LEN }
};

static int ustack_init_port(struct rte_mempool *mbuf_pool) {
    // 获取可用端口数量
    uint16_t nb_sys_ports = rte_eth_dev_count_avail();
    if (nb_sys_ports == 0) {
        rte_exit(EXIT_FAILURE, "no supported eth found\n");
    }
    printf("Available ports: %d\n", nb_sys_ports);

    // // 验证端口是否有效
    // if (!rte_eth_dev_is_valid_port(global_portif)) {
    //     printf("Port %d is not valid, trying to use port 0\n", global_portif);
    //     global_portif = 0;
    // }

    // // 获取设备信息
    // struct rte_eth_dev_info dev_info;
    // rte_eth_dev_info_get(global_portif, &dev_info);
    // printf("Using port %d, driver: %s\n", global_portif, dev_info.driver_name);
    // printf("TX descriptor limits: min=%d, max=%d, align=%d\n", 
    //        dev_info.tx_desc_lim.nb_min, dev_info.tx_desc_lim.nb_max, 
    //        dev_info.tx_desc_lim.nb_align);

    const int num_rx_queues = 1;
    const int num_tx_queues = 1;
    int ret;
    
    // 配置端口前先检查端口是否已停止
    rte_eth_dev_stop(global_portif);
    
    ret = rte_eth_dev_configure(global_portif, num_rx_queues, num_tx_queues, &port_conf_default);
    printf("hello dpdk4\n");
    if(ret < 0) {
        rte_exit(EXIT_FAILURE, "configure port %d failed, error: %d\n", global_portif, ret);
    }
    
    printf("hello dpdk5\n");
    // 设置RX队列 - 使用RX_RING_SIZE
    if (rte_eth_rx_queue_setup(global_portif, 0, RX_RING_SIZE, 
                                rte_eth_dev_socket_id(global_portif), 
                                NULL, mbuf_pool) < 0) {
        rte_exit(EXIT_FAILURE, "could not setup rx queue\n");
    }
    
    printf("hello dpdk6\n");
    // 设置TX队列 - 使用TX_RING_SIZE (至少512)
    if (rte_eth_tx_queue_setup(global_portif, 0, TX_RING_SIZE,
                                rte_eth_dev_socket_id(global_portif),
                                NULL) < 0) {
        rte_exit(EXIT_FAILURE, "could not setup tx queue\n");
    }
    
    if (rte_eth_dev_start(global_portif) < 0) {
        rte_exit(EXIT_FAILURE, "could not start port %d\n", global_portif);
    }
    printf("hello dpdk7\n");
    
    // // 检查链路状态
    // struct rte_eth_link link;
    // rte_eth_link_get_nowait(global_portif, &link);
    // if (link.link_status) {
    //     printf("Port %d link up - speed %u Mbps\n", global_portif, link.link_speed);
    // } else {
    //     printf("Port %d link down\n", global_portif);
    // }
    
    return 0;
}

int main(int argc, char *argv[]) {
    int i;
    
    /*1. dpdk环境初始化*/
    if (rte_eal_init(argc, argv) < 0) rte_exit(EXIT_FAILURE, "ERROR with EAL"); // 初始化DPDK环境抽象层(EAL)


    /*2. dpdk内存池管理*/
    // 创建装用的内存储mbuf pool
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", 
                                                            NUM_MBUFS, 0, 0,
                                                            RTE_MBUF_DEFAULT_BUF_SIZE,
                                                            rte_socket_id());
    if (mbuf_pool == NULL) rte_exit(EXIT_FAILURE, "CREATE mbuf pool error\n");


    // printf("hello dpdk1\n");
    /*3. dpdk端口管理
        获取可用DPDK端口数量 rte_eth_dev_count_avail()
        配置DPDK以太网设备  rte_eth_dev_configure()
        设置DPDK接收队列    rte_eth_rx_queue_setup()
        设置DPDK发送队列    rte_eth_tx_queue_setup()
        启动DPDK设备        rte_eth_dev_start()
    */
    ustack_init_port(mbuf_pool);
    
    // printf("hello dpdk2\n");

    /* dpdk数据包接收处理核心mainloop */
    while (1) {
        // 分配mbuf指针数组：准备接收缓冲区（这个数组不存储数据，只存储指向mbuf的指针（每个指针8字节））
        struct rte_mbuf *mbufs[BURST_SIZE];

        /*4. DPDK数据包接收*/       
        //                                   网卡端口ID（0）接收队列编号        最多接收128个包
        uint16_t num_recvd = rte_eth_rx_burst(global_portif, 0, mbufs, BURST_SIZE); // 批量接收数据包
        if (num_recvd > BURST_SIZE) rte_exit(EXIT_FAILURE, "ERROR receiving from eth\n");


        /*5. DPDK数据包处理
            遍历处理每个数据包
        */
        for (i = 0; i < num_recvd; i++) {
            //                      解析以太网头：   获取mbuf中数据的起始地址       将其转换为以太网头结构体指针
            struct rte_ether_hdr *ethdr = rte_pktmbuf_mtod(mbufs[i], struct rte_ether_hdr *);

            // 过滤非IPv4包：只处理IPv4数据包，忽略ARP、IPv6等。
            if (ethdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
                continue;
            }
            
            // 解析IP头：跳过以太网头（14字节），指向IP头
            struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(mbufs[i], 
                struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
            
            // 检查UDP协议
            if (iphdr->next_proto_id == IPPROTO_UDP) {
                // 解析UDP头和数据
                struct rte_udp_hdr *udphdr = (struct rte_udp_hdr *)(iphdr + 1);
                printf("Received UDP packet: %s\n", (char *)(udphdr + 1));
            }
            
            // 释放mbuf
            // rte_pktmbuf_free(mbufs[i]);
        }
    }
    printf("hello dpdk\n");
    
    return 0;
}