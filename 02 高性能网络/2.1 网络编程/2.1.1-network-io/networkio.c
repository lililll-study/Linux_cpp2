#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include <pthread.h>


#define BUFFER_LENGTH 1024

// 客户端处理线程 （写入线程要执行的任务）
void *client_thread(void *arg) {
    int clientfd = *(int *)arg;

    while (1) {
        char buffer[BUFFER_LENGTH] = {0};
        int data_len = recv(clientfd, buffer, BUFFER_LENGTH, 0);
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
    printf("listen finished\n");

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

#else 

    while(1) { // 单线程循环，存在多客户端的连接阻塞问题，故用多线程连接
        int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        printf("accept finished\n");

        // 创建线程并执行任务，用于执行接收客户端数据的任务
        pthread_t th_id;
        pthread_create(&th_id, NULL, client_thread, &clientfd);
    }

#endif

    getchar();




    return 0;

}