#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>     // 套接字相关的函数 (socket, sendto, recvfrom)
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <unistd.h>

#include <fcntl.h>
#include <netdb.h>
#include <errno.h>

#define HTTP_VERSION "HTTP/1.1"
#define CONNECTION_TYPE "Connection: close\r\n"
#define BUFFER_SIZE 4096


// 不用之前dns的方式获取ip地址
// 搜索一下struct hostent
char *host_to_ip(const char *hostname) {
    
    struct hostent *host_entry = gethostbyname(hostname);

    // 14.215.177.39 (点分十进制) 可以通过一个无符号int型表示
    // inet_ntoa把uint型转换为"点分十进制"字符串
    if (host_entry) {
        return inet_ntoa(*(struct in_addr*)*host_entry -> h_addr_list);
    } else {
        return NULL;
    }    
}

int http_create_socket(char *ip) {
    // SOCK_STREAM: 使用TCP协议（流式套接字）
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // 初始化地址结构：定义并清零一个IPv4地址结构体
    struct sockaddr_in sin = {0};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(80);
    sin.sin_addr.s_addr = inet_addr(ip);

    // 连接到指定服务器
    if (0 != connect(sockfd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in))){
        perror("connect");
        close(sockfd);
        return -1;
    }
    // 设置为非阻塞
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    return sockfd;
}


char *http_send_request(const char *hostname, const char *resource) {
    char *ip = host_to_ip(hostname); // 通过这个函数获取响应ip地址
    int sockfd = http_create_socket(ip);
    printf("套接字创建连接信息：%d\n", sockfd);

    char buffer[BUFFER_SIZE] = {0};
    sprintf(buffer, 
"GET %s %s\r\n\
Host: %s\r\n\
%s\r\n\
\r\n", 
    resource, HTTP_VERSION, 
    hostname, 
    CONNECTION_TYPE);
    // 这里没有用sendto，两者具有区别，重点是是否区分目标地址。
    // 并且send之前必须先connect，而sendto之前不用
    // send(TCP，可靠，面向连接)
    // sendto(UDP, 不可靠，可连接也可不连接)
    send(sockfd, buffer, strlen(buffer), 0);

    // select
    fd_set fdread;  // 定义一个fd标志位集合，如果有数据来了就置位
    FD_ZERO(&fdread); // 先把集合都清空
    FD_SET(sockfd, &fdread);    // 设置要监控的fd：将sockfd对应的bit置为1

    struct timeval tv;
    tv.tv_sec = 5;  // 设置超时时间5s，0us
    tv.tv_usec = 0;

    char *result = malloc(sizeof(int));
    memset(result, 0, sizeof(int)); //malloc出来的一定要memset，防止其中有脏数据

    while (1) {
        int selection = select(sockfd+1, &fdread, NULL, NULL, &tv);
        if (!selection || !FD_ISSET(sockfd, &fdread)) {
            // 超时：没有时间发生， !FD_ISSET(sockfd, &fdread)：sockfd没有可读事件
            break;
        } else {
            memset(buffer, 0, BUFFER_SIZE);
            int len = recv(sockfd, buffer, BUFFER_SIZE, 0);
            if (len == 0) break; // 说明对端已经关闭了，disconnect
            // 动态扩展 result 缓冲区，以便追加新接收的数据。
            result = realloc(result, (strlen(result) + len + 1) * sizeof(char));
            strncat(result, buffer, len);
        }
    }
    return result;
}

int main(int argc, char *argv[]) {
    if (argc < 3) return -1;
    char *response = http_send_request(argv[1], argv[2]);
    printf("response : %s\n", response);

    free(response);
}


