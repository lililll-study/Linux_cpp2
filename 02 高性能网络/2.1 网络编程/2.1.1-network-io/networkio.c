#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#include <poll.h>
#include <sys/epoll.h>

#define BUFFER_LENGTH 1024

// 客户端处理线程 （写入线程要执行的任务）
void *client_thread(void *arg) {
    int clientfd = *(int *)arg;

    while (1) {
        char buffer[BUFFER_LENGTH] = {0};
        int data_len = recv(clientfd, buffer, BUFFER_LENGTH, 0);

        if (data_len == 0) {  // 读取出错
            // close(clientfd);
            printf("client disconnect: %d\n", clientfd);
            close(clientfd);
            break;
        } else if (data_len < 0){
            printf("read error: %d\n", clientfd);
            close(clientfd);
            break;
        }
        printf("recv buffer: %s\n", buffer);

        int send_len = send(clientfd, buffer, data_len, 0);
        printf("send len: %d\n", send_len);

    }

}




int main(int argc, char *argv[]) {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(2000);//0-1023不能用，要大于1024
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //0.0.0.0

    if (-1 == bind(sockfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr_in))) {
        printf("bind faild: %s\n", strerror(errno));
    }

    listen(sockfd, 10);
    printf("listen finished sockfd: %d\n", sockfd);

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_len = sizeof(client_addr);

#if 0 // 直接读取数据
    int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
    printf("accept finished\n");

    char buffer[BUFFER_LENGTH] = {0};
    int data_len = recv(clientfd, buffer, BUFFER_LENGTH, 0);
    printf("recv buffer: %s\n", buffer);

    int send_len = send(clientfd, buffer, data_len, 0);
    printf("send len: %d\n", send_len);

#elif 0 // 一请求一线程

    while(1) { // 单线程循环，存在多客户端的连接阻塞问题，故用多线程连接
        int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        printf("accept finished clientfd: %d\n", clientfd);

        // 创建线程并执行任务，用于执行接收客户端数据的任务
        pthread_t th_id;
        pthread_create(&th_id, NULL, client_thread, &clientfd);
    }

#elif 0 // IO多路复用 select
    fd_set rfds, rset;  // rfds作为完整监控集合，rset作为临时副本
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    int maxfd = sockfd;

    while (1) {
        rset = rfds;
        int nready = select(maxfd+1, &rset, NULL, NULL, NULL); // select 会修改可读集合，把有事件的置位
        // 判断sockfd是否在rfds中被标记
        if (FD_ISSET(sockfd, &rset)) { // accept

            int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
            printf("accept finished clientfd: %d\n", clientfd);

            FD_SET(clientfd, &rfds);    // 有新连接，把连接置位到完整集合中

            if (clientfd > maxfd) maxfd = clientfd;
        }
        // recv
        int i = 0;
        for (i = sockfd+1; i <= maxfd; i++) {

            if (FD_ISSET(i, &rset)) {
                char buffer[BUFFER_LENGTH] = {0};
                int data_len = recv(i, buffer, BUFFER_LENGTH, 0);

                if (data_len == 0) { // 断开连接了
                    // close(clientfd);
                    printf("client disconnect: %d\n", i);
                    close(i);

                    FD_CLR(i, &rfds);

                    continue;
                } else if (data_len < 0){  // 读取出错
                    printf("read error: %d\n", i);
                    close(i);
                    continue;
                }
                printf("recv buffer: %s\n", buffer);

                int send_len = send(i, buffer, data_len, 0);
                printf("send len: %d\n", send_len);
            }


        }
    }


#elif 0  // poll
    struct pollfd fds[1024] = {0};
    fds[sockfd].fd = sockfd;
    fds[sockfd].events = POLLIN;

    int maxfd = sockfd;

    while(1) {
        int nready = poll(fds, maxfd+1, -1);

        if (fds[sockfd].revents & POLLIN) {
            int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
            printf("accept finished clientfd: %d\n", clientfd);

            fds[clientfd].fd = clientfd;
            fds[clientfd].events = POLLIN;

            if (clientfd > maxfd) maxfd = clientfd;

        }
        int i = 0;
        for (i = sockfd+1; i <= maxfd; i++) {
            if (fds[i].revents & POLLIN) {
                char buffer[BUFFER_LENGTH] = {0};
                int data_len = recv(i, buffer, BUFFER_LENGTH, 0);

                if (data_len == 0) { // 断开连接了
                    // close(clientfd);
                    printf("client disconnect: %d\n", i);
                    close(i);

                    fds[i].fd = -1;
                    fds[i].events = 0;

                    continue;
                } else if (data_len < 0){  // 读取出错
                    printf("read error: %d\n", i);
                    close(i);
                    continue;
                }
                printf("recv buffer: %s\n", buffer);
                int send_len = send(i, buffer, data_len, 0);
                printf("send len: %d\n", send_len);
            }
        }
    }


#else

    int epfd = epoll_create(10);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    while (1) {
        struct epoll_event events[1024] = {0};
        int nready = epoll_wait(epfd, events, 1024, -1);

        int i = 0;
        for (i = 0; i < nready; i++) {

            int connectionfd = events[i].data.fd;

            if (connectionfd == sockfd) {

                int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
                printf("accept finished clientfd: %d\n", clientfd);

                ev.events = EPOLLIN;
                ev.data.fd = clientfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);

            } else if (events[i].events & EPOLLIN) {

                char buffer[BUFFER_LENGTH] = {0};
                int data_len = recv(connectionfd, buffer, BUFFER_LENGTH, 0);

                if (data_len == 0) { // 断开连接了
                    // close(clientfd);
                    printf("client disconnect: %d\n", connectionfd);
                    close(connectionfd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, connectionfd, NULL);

                    continue;
                } else if (data_len < 0){  // 读取出错
                    printf("read error: %d\n", connectionfd);
                    close(i);
                    continue;
                }
                printf("recv buffer: %s\n", buffer);
                int send_len = send(connectionfd, buffer, data_len, 0);
                printf("send len: %d\n", send_len);
            }
        }

    }

#endif

    getchar();




    return 0;

}