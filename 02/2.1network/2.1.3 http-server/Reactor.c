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
#include <stdlib.h>

#include "server.h"


#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

/* 
连接大小：读缓存1K，写缓存1K，2K* = 2M
*/


#define CONNECTION_SIZE     1048576 // 1024^2
#define MAX_PORTs           512

#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)





int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);
int event_register(int fd, int event);



int epfd = 0;
struct timeval begin;




// struct conn conn_list[CONNECTION_SIZE] = {0};
// 优化为动态分配内存
struct conn *conn_list = NULL;








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

    memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
    int data_len = recv(fd, conn_list[fd].rbuffer, BUFFER_LENGTH, 0);

    if (data_len == 0) { // 断开连接了
        // close(clientfd);
        printf("client disconnect: %d\n", fd);
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);   // unfinished 

        return 0;
    } else if (data_len < 0) {
        printf("count: %d, errno: %d, %s\n", data_len, errno, strerror(errno));
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);

        return 0;
    }

    conn_list[fd].rlength = data_len;
    // printf("recv buffer: %s\n", conn_list[fd].rbuffer);

#if 0 // echo
    conn_list[fd].wlength = conn_list[fd].rlength;
    memcpy(conn_list[fd].wbuffer, conn_list[fd].rbuffer, conn_list[fd].wlength);

#elif 1
// 接受完数据后做request请求
    http_requset(&conn_list[fd]);

#else
    ws_request(&conn_list[fd]);

#endif
    set_event(fd, EPOLLOUT, 0);
    // // 根据状态决定注册什么事件
    // if (conn_list[fd].status == 1) {
    //     // 握手完成，注册 EPOLLOUT 发送响应
    //     set_event(fd, EPOLLOUT, 0);
    // } else if (conn_list[fd].status == 2) {
    //     // 收到数据，准备发送响应
    //     set_event(fd, EPOLLOUT, 0);
    // } else {
    //     // 其他情况，继续监听读事件
    //     set_event(fd, EPOLLIN, 0);
    // }

    return data_len;
}

//1 不同事件触发不同回调
//2 rbuffer, wbuffer





int send_cb(int fd) {

#if 1
// 发送数据之前做response响应
    http_response(&conn_list[fd]);

#else 
    ws_response(&conn_list[fd]);

#endif

    int send_len = 0;

#if 0

    if (conn_list[fd].status == 1) {
        // printf("send buffer: %s\n", conn_list[fd].wbuffer);
        send_len = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlength, 0);
        set_event(fd, EPOLLOUT, 0);
    } else if (conn_list[fd].status == 2) {
        set_event(fd, EPOLLOUT, 0);
    } else if (conn_list[fd].status == 0) {
        set_event(fd, EPOLLIN, 0);
    }

#else

	if (conn_list[fd].wlength != 0) {
		send_len = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlength, 0);
	}
	
	set_event(fd, EPOLLIN, 0);

#endif

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

    listen(sockfd, 1024);
    // printf("listen finished sockfd: %d\n", sockfd);

    return sockfd;
}



int main() {
    unsigned short port = 2000;

    epfd = epoll_create(1);

    conn_list = (struct conn *)malloc(CONNECTION_SIZE * sizeof(struct conn));
    if (!conn_list) {
        printf("malloc conn_list failed\n");
        return -1;
    }

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









