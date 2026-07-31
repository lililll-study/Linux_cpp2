#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/time.h>

/* 
连接大小：读缓存1K，写缓存1K，2K* = 2M
*/

#define BUFFER_LENGTH       1024
#define CONNECTION_SIZE     1048576 // 1024^2
#define MAX_PORTs           20

#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

typedef int (*RCALLBACK)(int fd);




int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);
int event_register(int fd, int event);



int epfd = 0;
struct timeval begin;


// 封装IO，读数据写数据内存，以及相应的回调函数
struct conn {
    int fd;

    char rbuffer[BUFFER_LENGTH];
    int rlength;

    char wbuffer[BUFFER_LENGTH];
    int wlength;

    RCALLBACK send_callback; 
    union {// recv和accept是一个或的关系
        RCALLBACK recv_callback;
        RCALLBACK accept_calllback;
    } r_action;
    
};

struct conn conn_list[CONNECTION_SIZE] = {0};









int set_event(int fd, int event, int flag) {

    if (flag) { // 1:添加
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    } else {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
    }

}


int event_register(int fd, int event) {

    if (fd < 0) return -1;
    // 为新连接设置不同回调函数以及信息
    conn_list[fd].fd = fd;
    conn_list[fd].r_action.recv_callback = recv_cb;
    conn_list[fd].send_callback = send_cb;

    memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
    conn_list[fd].rlength = 0;

    memset(conn_list[fd].wbuffer, 0, BUFFER_LENGTH);
    conn_list[fd].wlength = 0;

    set_event(fd, event, 1);
}



// listenfd(sockfd) --> EPOLLIN --> accept_cb
int accept_cb(int fd) {

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_len = sizeof(client_addr);

    int clientfd = accept(fd, (struct sockaddr*)&client_addr, &client_len);
    // printf("accept finished clientfd: %d\n", clientfd);
    if (clientfd < 0) {
        printf("accept error: %d --> %s\n", errno, strerror(errno));
        return -1;
    }

    event_register(clientfd, EPOLLIN);

    if ((clientfd % 1000) == 0) {

        struct timeval current;
        gettimeofday(&current, NULL);

        int time_used = TIME_SUB_MS(current, begin);
        memcpy(&begin, &current, sizeof(struct timeval));

        printf("accept finished clientfd: %d, time_used: %d\n", clientfd, time_used);

    }
    return 0;
}


int recv_cb(int fd) {

    int data_len = recv(fd, conn_list[fd].rbuffer, BUFFER_LENGTH, 0);

    if (data_len == 0) { // 断开连接了
        // close(clientfd);
        printf("client disconnect: %d\n", fd);
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);   // unfinished 

        return 0;
    }

    conn_list[fd].rlength = data_len;
    // printf("recv buffer: %s\n", conn_list[fd].rbuffer);

#if 1 // echo
    conn_list[fd].wlength = conn_list[fd].rlength;
    memcpy(conn_list[fd].wbuffer, conn_list[fd].rbuffer, conn_list[fd].wlength);
#endif
    set_event(fd, EPOLLOUT, 0);

    return data_len;
}


int send_cb(int fd) {

    int send_len = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlength, 0);
    // printf("send len: %d\n", send_len);

    set_event(fd, EPOLLIN, 0);

    return send_len;
}


int init_server(unsigned short port) {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);//0-1023不能用，要大于1024
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //0.0.0.0

    if (-1 == bind(sockfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr_in))) {
        printf("bind faild: %s\n", strerror(errno));
    }

    listen(sockfd, 10);
    // printf("listen finished sockfd: %d\n", sockfd);

    return sockfd;
}



int main() {
    unsigned short port = 2000;

    epfd = epoll_create(1);

    int i = 0;

    for (i = 0; i < MAX_PORTs; i++){
        int sockfd = init_server(port + i); 
        conn_list[sockfd].fd = sockfd;
        conn_list[sockfd].r_action.recv_callback = accept_cb;

        set_event(sockfd, EPOLLIN, 1);
    }

    gettimeofday(&begin, NULL);


    while(1) { // mainloop

        struct epoll_event events[1024] = {0};
        int nready = epoll_wait(epfd, events, 1024, -1);

        int i = 0;
        for (i=0; i<nready; i++) {
            int connectionfd = events[i].data.fd;

            // 不用else if是因为，IO可能同时存在IN和OUT事件
            if (events[i].events & EPOLLIN) {
                conn_list[connectionfd].r_action.recv_callback(connectionfd);
            }
            if (events[i].events & EPOLLOUT) {
                conn_list[connectionfd].send_callback(connectionfd);
            }
        }
    }
}









