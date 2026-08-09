

#include <stdio.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <arpa/inet.h>


int global_portif = 1;

#define NUM_MBUFS 4096
#define BURST_SIZE 128

static const struct rte_eth_conf port_conf_default = {
    .rxmode = { .max_rx_pkt_len = RTE_ETHER_MAX_LEN }
};


static int ustack_init_port(struct rte_mempool *mbuf_pool) {
    // number
    uint16_t nb_sys_ports = rte_eth_dev_count_avail();
    if (nb_sys_ports == 0) rte_exit(EXIT_FAILURE, "no supported eth found\n");

    const int num_rx_queues = 1;
    const int num_tx_queues = 1;
    int ret = rte_eth_dev_configure(global_portif, num_rx_queues, num_tx_queues, &port_conf_default);
    printf("hello dpdk4\n");
    if(ret < 0)
    {
        rte_exit(EXIT_FAILURE,
                "configure port failed\n");
    }
    printf("hello dpdk5\n");
    if (rte_eth_rx_queue_setup(global_portif, 0, 128, rte_eth_dev_socket_id(global_portif), NULL, mbuf_pool) < 0) {
        rte_exit(EXIT_FAILURE, "could not setup rx queue\n");
    }
    printf("hello dpdk6\n");
    if (rte_eth_dev_start(global_portif) < 0) {
        rte_exit(EXIT_FAILURE, "could not start\n");
    }
    printf("hello dpdk7\n");
    return 0;
}
// dpdk在接收数据的时候，数据已经到内存里面了(mbuf)
int main(int argc, char *argv[]) {
    if (rte_eal_init(argc, argv) < 0) {
        rte_exit(EXIT_FAILURE, "ERROR with EAL");
    }

    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("mbuf poll", 
                                                            NUM_MBUFS, 0, 0,
                                                            RTE_MBUF_DEFAULT_BUF_SIZE,
                                                            rte_socket_id()); // socket指的是哪一块内存
    if (mbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "CREATE mbuf pool error\n");
    }
    printf("hello dpdk1\n");
    ustack_init_port(mbuf_pool);
    printf("hello dpdk2\n");
    while (1)
    {
        struct rte_mbuf *mbufs[BURST_SIZE] = {0};
        uint16_t num_recvd = rte_eth_rx_burst(global_portif, 0, mbufs, BURST_SIZE);
        if (num_recvd > BURST_SIZE) {
            rte_exit(EXIT_FAILURE, "ERROR receiving from eth\n");
        }

        int i = 0;
        for (i=0; i<num_recvd; i++) {
            struct rte_ether_hdr *ethdr = rte_pktmbuf_mtod(mbufs[i], struct rte_ether_hdr *);
            if (ethdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
                continue;
            }
            // 把
            struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(mbufs[i], struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
            if (iphdr->next_proto_id == IPPROTO_UDP) {
                struct rte_udp_hdr *udphdr = (struct rte_udp_hdr *)(iphdr + 1);

                printf("udp : %s\n", (char *)(udphdr+1));
            }


        }
    }
    printf("hello dpdk\n");
    
}