#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define BUFFER_LENGTH 1024
#define EPOLL_SIZE 1024

#define MAX_PORT 100

// 客户端处理线程
void *client_routine(void *arg) {

    // 先转为 int*，再解引用得到 clientfd
    // accept() 返回的客户端套接字，专门用于和这个客户端通信
    int clientfd = *(int *)arg;


    while (1) {
        char buffer[BUFFER_LENGTH] = {0};
        int len = recv(clientfd, buffer, BUFFER_LENGTH, 0); // 从客户端接收数据

        if (len < 0) { // 读取出错
            close(clientfd);
            break;
        } else if (len == 0) { // disconnected了，断开连接了
            close(clientfd);
            break;
        } else { // >0 收到了len字节的数据
            printf("Recv : %s, %d byte(s)\n", buffer, len);
        }
    }

}

int islistenfd(int fd, int *fds) {
    int i = 0;
    for (i =0; i <MAX_PORT; i++) {
        if (fd == *(fds+i)) return fd;
    }
    return 0;
}




int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("argc error");
        return -1;
    }

    int port = atoi(argv[1]); // start port
    int sockfds[MAX_PORT] = {0};    // listen fd
    int epfd = epoll_create(1);

    int i = 0;
    for (i = 0; i < MAX_PORT; i++) {
    
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port+i); // 8888 8889 8890 ... 8987 设置100个监听端口
        addr.sin_addr.s_addr = INADDR_ANY;  //绑定到本机所有网卡
        // 把套接字和IP+port绑定到一起：这个套接字负责处理发往这个端口的数据
        int ret = bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
        if(ret < 0) {
            perror("bind");
            return 2;
        }
        // 此处需要强制类型转换，因为addr是 IPv4 专用的 struct sockaddr_in*，需要转换为通用类型 struct sockaddr*

        // 开始监听：等待队列的最大长度5（积压连接数）
        // 最多允许 5 个客户端在排队等待被 accept
        ret = listen(sockfd, 5);
        if(ret < 0) {
            perror("listen");
            return 3;
        }
        printf("tcp server listen on port : %d\n", port+i);

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = sockfd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

        sockfds[i] = sockfd;
    }

    
#if 0 

    while (1) {

        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(struct sockaddr_in));
        socklen_t client_len = sizeof(client_addr);

        // accept 会从sockfd获取客户端地址和端口号，存入client_addr
        // 这里的接收是阻塞调用的，程序会停在这里等待客户端连接
        // 当有客户端连接时，accept() 返回一个新的套接字 clientfd
        int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);

        pthread_t thread_id;
        // 创建一个新线程，执行 client_routine 函数
        // 把 clientfd 的地址传给线程，clientfd即是入参
        pthread_create(&thread_id, NULL, client_routine, &clientfd);


    }

#else
    // 
    
    struct epoll_event events[EPOLL_SIZE] = {0};



    while (1) {
        // events告诉我我们总共有事件的Io有多少个
        // -1表示一直处于阻塞；如果设置5，则表示超过5s后没有事件
        int nready = epoll_wait(epfd, events, EPOLL_SIZE, 5);
        if (nready == -1) continue;
        int i=0;
        for (i=0; i < nready; i++) {

            int sockfd = islistenfd(events[i].data.fd, sockfds);

            if (sockfd) { // listen监听套接字
                struct sockaddr_in client_addr;
                memset(&client_addr, 0, sizeof(struct sockaddr_in));
                socklen_t client_len = sizeof(client_addr);

                int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);

                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET; // 使用边沿触发来做
                ev.data.fd = clientfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);
            } else {

                int clientfd = events[i].data.fd;

                char buffer[BUFFER_LENGTH] = {0};
                int len = recv(clientfd, buffer, BUFFER_LENGTH, 0); // 从客户端接收数据
                if (len < 0) { // 读取出错
                    close(clientfd);

                    struct epoll_event ev;
                    ev.events = EPOLLIN;
                    ev.data.fd = clientfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, &ev);

                } else if (len == 0) { // disconnected了，断开连接了
                    close(clientfd);

                    struct epoll_event ev;
                    ev.events = EPOLLIN;
                    ev.data.fd = clientfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, &ev);

                } else { // >0 收到了len字节的数据
                    printf("Recv : %s, %d byte(s), clientfd: %d\n", buffer, len, clientfd);
                }
            }
        }
    }



#endif
    return 0;
}


