#include <stdio.h>
#include <liburing.h>
#include <netinet/in.h>


#define EVENT_ACCEPT 0
#define EVENT_READ 1
#define EVENT_WRITE 1


struct conn_info {
    int fd;
    int event;
};

int init_server(unsigned short port) {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);//0-1023不能用，要大于1024
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //0.0.0.0

    if (-1 == bind(sockfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr_in))) {
        printf("bind faild: %s\n", strerror(errno));
    }

    listen(sockfd, 1024);
    // printf("listen finished sockfd: %d\n", sockfd);

    return sockfd;
}

#define ENTRIES_LENGTH 1024


int set_event_accept(struct io_uring *ring, int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

    struct conn_info accept_info = {
        .fd = sockfd,
        .event = EVENT_ACCEPT,
    };

    io_uring_prep_accept(sqe, sockfd, (struct sockaddr*)addr, addrlen, flags);
    memcpy(&sqe->user_data, &accept_info, sizeof(struct conn_info));
}

int set_event_recv(struct io_uring *ring, int sockfd, void *buf, size_t *len, int flags) {

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

    struct conn_info accept_info = {
        .fd = sockfd,
        .event = EVENT_READ,
    };

    io_uring_prep_recv(sqe, sockfd, buf, len, flags);
    memcpy(&sqe->user_data, &accept_info, sizeof(struct conn_info));
}

int set_event_send(struct io_uring *ring, int sockfd, void *buf, size_t *len, int flags) {

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

    struct conn_info accept_info = {
        .fd = sockfd,
        .event = EVENT_WRITE,
    };

    io_uring_prep_send(sqe, sockfd, buf, len, flags);
    memcpy(&sqe->user_data, &accept_info, sizeof(struct conn_info));
}

int main(int argc, char *argv[]) {

    unsigned short port = 9999;
    int sockfd = init_server(port);

    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    struct io_uring ring;
    io_uring_queue_init_params(ENTRIES_LENGTH, &ring, &params);



#if 0
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_len = sizeof(client_addr);
    int clientfd = accept(fd, (struct sockaddr*)&client_addr, &client_len);

#else
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_len = sizeof(client_addr);
    // 往ring队列中添加一个节点
    set_event_accept(&ring, sockfd, (struct sockaddr*)&client_addr, &client_len, 0);
#endif


    char buffer[1024] ={0};
    while(1) {

        io_uring_submit(&ring);

        struct io_uring_cqe *cqe;
        io_uring_wait_cqe(&ring, cqe);

        struct io_uring_cqe *cqes[128];
        int nready = io_uring_peek_batch_cqe(&ring, cqes, 128);  // epoll_wait

        int i = 0;
        for (i=0; i<nready; i++) {

            struct io_uring_cqe *entries = cqes[i];
            struct conn_info result;
            memcpy(&result, &entries->user_data, sizeof(struct conn_info));

            if (result.event == EVENT_ACCEPT) {
                // 在每次处理EVENT_ACCEPT
                // 需要再set一次
                set_event_accept(&ring, sockfd, (struct sockaddr*)&client_addr, &client_len, 0);
                printf("set set_event_accept\n");
                
                int connfd = entries->res;

                set_event_recv(&ring, connfd, buffer, 1024, 0);

            } else if (result.event == EVENT_READ) {

                int ret = entries->res;
                printf("set set_event_recv: %s, len: ret%s\n", buffer, ret);

                if (ret == 0) {
                    close(result.fd);
                }else if (ret > 0) {
                    set_event_send(&ring, result.fd, buffer, ret, 0);
                }
            } else if (result.event == EVENT_WRITE) {

                int ret = entries->res;
                printf("set set_event_send: %s, len: ret%s\n", buffer, ret);

                set_event_recv(&ring, result.fd, buffer, 1024, 0);
            }
        }

        io_uring_cq_advance(&ring, nready);
    }
}

