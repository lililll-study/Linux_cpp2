


#include <stdio.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <arpa/inet.h>

int global_portif = 0;

#define NUM_MBUFS 4096
#define BURST_SIZE 128
#define RX_RING_SIZE 128
#define TX_RING_SIZE 512  // vmxnet3要求至少512

#define ENABLE_SEND 1
#define ENABLE_TCP 1

#define TCP_INIT_WINDOWS 14600

#if ENABLE_TCP
uint8_t global_flags;
uint32_t global_seqnum;
uint32_t global_acknum;


typedef enum __USTACK_TCP_STATUS {
    USTACK_TCP_STATUS_CLOSED = 0,
    USTACK_TCP_STATUS_LISTEN,
    USTACK_TCP_STATUS_SYN_RCVD,
    USTACK_TCP_STATUS_SYN_SENT,
    USTACK_TCP_STATUS_ESTABLISHED,
    USTACK_TCP_STATUS_FIN_WAIT_1,
    USTACK_TCP_STATUS_FIN_WAIT_2,
    USTACK_TCP_STATUS_CLOSING,
    USTACK_TCP_STATUS_TIMEWAIT,
    USTACK_TCP_STATUS_CLOSE_WAIT,
    USTACK_TCP_STATUS_LAST_ACK
    
} USTACK_TCP_STATUS;

uint8_t tcp_status = USTACK_TCP_STATUS_LISTEN;
#endif


#if ENABLE_SEND

uint8_t global_smac[RTE_ETHER_ADDR_LEN];
uint8_t global_dmac[RTE_ETHER_ADDR_LEN];

uint32_t global_sip;
uint32_t global_dip;

uint16_t global_sport;
uint16_t global_dport;

#endif


static const struct rte_eth_conf port_conf_default = {
    .rxmode = { .max_rx_pkt_len = RTE_ETHER_MAX_LEN }
};

static int ustack_init_port(struct rte_mempool *mbuf_pool) {
    // 获取可用端口数量
    uint16_t nb_sys_ports = rte_eth_dev_count_avail();
    if (nb_sys_ports == 0) rte_exit(EXIT_FAILURE, "no supported eth found\n");
    printf("Available ports: %d\n", nb_sys_ports);

    // // 验证端口是否有效
    // if (!rte_eth_dev_is_valid_port(global_portif)) {
    //     printf("Port %d is not valid, trying to use port 0\n", global_portif);
    //     global_portif = 0;
    // }

    // 获取设备信息
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(global_portif, &dev_info);
    // printf("Using port %d, driver: %s\n", global_portif, dev_info.driver_name);
    // printf("TX descriptor limits: min=%d, max=%d, align=%d\n", 
    //        dev_info.tx_desc_lim.nb_min, dev_info.tx_desc_lim.nb_max, 
    //        dev_info.tx_desc_lim.nb_align);

    const int num_rx_queues = 1;

#if ENABLE_SEND
    const int num_tx_queues = 1;
#else
    const int num_tx_queues = 0;
#endif
    int ret;
    
    // 配置端口前先检查端口是否已停止
    rte_eth_dev_stop(global_portif);
    
    ret = rte_eth_dev_configure(global_portif, num_rx_queues, num_tx_queues, &port_conf_default);

    if(ret < 0) rte_exit(EXIT_FAILURE, "configure port %d failed, error: %d\n", global_portif, ret);

    // 设置RX队列 - 使用RX_RING_SIZE
    if (rte_eth_rx_queue_setup(global_portif, 0, RX_RING_SIZE, 
                                rte_eth_dev_socket_id(global_portif), 
                                NULL, mbuf_pool) < 0) {
        rte_exit(EXIT_FAILURE, "could not setup rx queue\n");
    }

#if ENABLE_SEND
    struct rte_eth_txconf txq_conf = dev_info.default_txconf;
    txq_conf.offloads = port_conf_default.rxmode.offloads;

    // 设置TX队列 - 使用TX_RING_SIZE (至少512)
    if (rte_eth_tx_queue_setup(global_portif, 0, TX_RING_SIZE, rte_eth_dev_socket_id(global_portif), &txq_conf) < 0) {
        rte_exit(EXIT_FAILURE, "could not setup tx queue\n");
    }

#endif
    if (rte_eth_dev_start(global_portif) < 0) {
        rte_exit(EXIT_FAILURE, "could not start port %d\n", global_portif);
    }
    
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

// msg：数据包的内存，
static int ustack_encode_udp_pkt(uint8_t *msg, uint8_t *data, uint16_t total_len) {

    // 1 ether header

    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    rte_memcpy(eth->d_addr.addr_bytes, global_dmac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(eth->s_addr.addr_bytes, global_smac, RTE_ETHER_ADDR_LEN);
    eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);

    // 2 IP header
    // 在以太网头后面加上ip
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr*)(eth + 1); //msg + sizeof(struct rte_ether_hdr);
    // 4表示ipv4，5表示首部长度(header len)为5，单位是4字节，即20字节
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = htons(total_len - sizeof(struct rte_ether_hdr));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64;
    ip->next_proto_id = IPPROTO_UDP;
    ip->src_addr = global_sip;
    ip->dst_addr = global_dip;

    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    // 3. UDP header
    struct rte_udp_hdr *udp = (struct rte_udp_hdr*)(ip + 1);
    udp->src_port = global_sport;
    udp->dst_port = global_dport;
    uint16_t udplen = total_len - sizeof(struct rte_ether_hdr) - sizeof(struct rte_ipv4_hdr);
    udp->dgram_len = htons(udplen);

    rte_memcpy((uint8_t*)(udp+1), data, udplen);
    udp->dgram_cksum = 0;
    udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);

    return 0;

}

#if 1
static int ustack_encode_tcp_pkt(uint8_t *msg, uint16_t total_len) {

    // 1 ether header

    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    rte_memcpy(eth->d_addr.addr_bytes, global_dmac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(eth->s_addr.addr_bytes, global_smac, RTE_ETHER_ADDR_LEN);
    eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);

    // 2 IP header
    // 在以太网头后面加上ip
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr*)(eth + 1); //msg + sizeof(struct rte_ether_hdr);
    // 4表示ipv4，5表示首部长度(header len)为5，单位是4字节，即20字节
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = htons(total_len - sizeof(struct rte_ether_hdr));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64;
    ip->next_proto_id = IPPROTO_TCP;
    ip->src_addr = global_sip;
    ip->dst_addr = global_dip;

    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    // 3. TCP header
    struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr*)(ip + 1);
    tcp->src_port = global_sport;
    tcp->dst_port = global_dport;
    tcp->sent_seq = htonl(12345);
    tcp->recv_ack = htonl(global_seqnum + 1);
    tcp->data_off = 0x50;
    tcp->tcp_flags =  RTE_TCP_SYN_FLAG | RTE_TCP_ACK_FLAG; //0x1 << 1; //syn位
   
    tcp->rx_win = TCP_INIT_WINDOWS;//htons(4096);
    tcp->cksum = 0;
    tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);

    return 0;

}

#endif

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

#if ENABLE_SEND

                rte_memcpy(global_smac, ethdr->d_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
                rte_memcpy(global_dmac, ethdr->s_addr.addr_bytes, RTE_ETHER_ADDR_LEN);

                rte_memcpy(&global_sip, &iphdr->dst_addr, sizeof(uint32_t));
                rte_memcpy(&global_dip, &iphdr->src_addr, sizeof(uint32_t));

                rte_memcpy(&global_sport, &udphdr->dst_port, sizeof(uint16_t));
                rte_memcpy(&global_dport, &udphdr->src_port, sizeof(uint16_t));

                struct in_addr addr;
                addr.s_addr = iphdr->src_addr;
                printf("sip %s:%d, ", inet_ntoa(addr), ntohs(udphdr->src_port));
                addr.s_addr = iphdr->dst_addr;
                printf("dip %s:%d, ", inet_ntoa(addr), ntohs(udphdr->dst_port));

                uint16_t length = ntohs(udphdr->dgram_len);
                uint16_t total_len = length + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_ether_hdr);

                struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
                if (!mbuf) rte_exit(EXIT_FAILURE, "ERROR rte_pktmbuf_alloc\n");
                mbuf->pkt_len = total_len;
                mbuf->data_len = total_len;

                uint8_t *msg = rte_pktmbuf_mtod(mbuf, uint8_t *);

                ustack_encode_udp_pkt(msg, (uint8_t*)(udphdr+1), total_len);

                // 直接回发数据

                rte_eth_tx_burst(global_portif, 0, &mbuf, 1);
#endif
                printf("Received UDP packet: %s\n", (char *)(udphdr + 1));
            } else if (iphdr->next_proto_id == IPPROTO_TCP) {
                // TCP建立连接 -> 数据传输 -> 断开连接
                printf("TCP\n");
                struct rte_tcp_hdr *tcphdr = (struct rte_tcp_hdr *)(iphdr + 1);

                // struct in_addr addr;
                // addr.s_addr = iphdr->src_addr;
                // printf("sip %s:%d, ", inet_ntoa(addr), ntohs(tcphdr->src_port));
                // addr.s_addr = iphdr->dst_addr;
                // printf("dip %s:%d, ", inet_ntoa(addr), ntohs(tcphdr->dst_port));

                // 
                rte_memcpy(global_smac, ethdr->d_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
                rte_memcpy(global_dmac, ethdr->s_addr.addr_bytes, RTE_ETHER_ADDR_LEN);

                rte_memcpy(&global_sip, &iphdr->dst_addr, sizeof(uint32_t));
                rte_memcpy(&global_dip, &iphdr->src_addr, sizeof(uint32_t));

                rte_memcpy(&global_sport, &tcphdr->dst_port, sizeof(uint16_t));
                rte_memcpy(&global_dport, &tcphdr->src_port, sizeof(uint16_t));
#if ENABLE_TCP
                global_flags = tcphdr->tcp_flags;
                global_seqnum = ntohl(tcphdr->sent_seq);
                global_acknum = ntohl(tcphdr->recv_ack);

                struct in_addr addr;
                addr.s_addr = iphdr->src_addr;
                printf("sip %s:%d, ", inet_ntoa(addr), ntohs(tcphdr->src_port));
                addr.s_addr = iphdr->dst_addr;
                printf("dip %s:%d, flags: %x, seqnum: %d, acknum: %d\n", 
                        inet_ntoa(addr), ntohs(tcphdr->dst_port), global_flags, global_seqnum, global_acknum);

                // 状态机实现TCP连接建立
                if (global_flags & RTE_TCP_SYN_FLAG) {

                    if (tcp_status == USTACK_TCP_STATUS_LISTEN) {
                        uint16_t total_len = sizeof(struct rte_tcp_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_ether_hdr);

                        struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
                        if (!mbuf) rte_exit(EXIT_FAILURE, "ERROR rte_pktmbuf_alloc\n");
                        mbuf->pkt_len = total_len;
                        mbuf->data_len = total_len;

                        uint8_t *msg = rte_pktmbuf_mtod(mbuf, uint8_t *);
                        ustack_encode_tcp_pkt(msg, total_len);

                        rte_eth_tx_burst(global_portif, 0, &mbuf, 1);
                        tcp_status = USTACK_TCP_STATUS_SYN_RCVD;
                    }

                }
                if (global_flags & RTE_TCP_ACK_FLAG) {
                    if (tcp_status == USTACK_TCP_STATUS_SYN_RCVD) {
                        tcp_status = USTACK_TCP_STATUS_ESTABLISHED;
                    }
                }
                if (global_flags & RTE_TCP_PSH_FLAG) {
                    if (tcp_status == USTACK_TCP_STATUS_ESTABLISHED) {
                        
                        uint8_t hdrlen = (tcphdr->data_off >> 4) * sizeof(uint32_t);
                        uint8_t *data = ((uint8_t*)tcphdr + hdrlen);
                        printf("tcp data: %s\n", data);
                    }
                }
#endif
            }
            
            // 释放mbuf
            rte_pktmbuf_free(mbufs[i]);
        }
    }
    printf("hello dpdk\n");
    
    return 0;
}