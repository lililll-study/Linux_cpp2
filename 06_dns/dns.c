#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>     // 套接字相关的函数 (socket, sendto, recvfrom)
#include <netinet/in.h>     // Internet地址结构体 (sockaddr_in, inet_addr)
#include <unistd.h>         // 标准系统调用 (close)

#define DNS_SERVER_PORT 53  // 服务默认端口
#define DNS_SERVER_IP   "114.114.114.114"   // 国内公用的DNS服务器

/*
实现一个DNS（域名系统，domain name system）客户端的简化实现
作用是向DNS服务器发送域名查询请求，并接收解析结果。总体设计方案
1. 用户输入域名 (www.0voice.com)
2. 构建DNS查询请求 (封装成二进制的数据包)
3. 发送到DNS服务器 (114.114.114.114:53)
4. 接收DNS服务器响应
5. 解析并打印结果
*/


// 定义dns协议头
// 一次dns请求，就是把下面的两组数据发送到dns服务器上
struct dns_header {// 固定12字节
    // 16 bits -> 2 bytes 定义为short类型
    unsigned short id;          // 会话标识 （请求和回答的id是一样的）
    unsigned short flags;       // 标志位

    unsigned short questions;   // 问题数
    unsigned short answer;      // 回答 资源记录数

    unsigned short authority;   // 授权 资源记录数
    unsigned short additional;  // 附加 资源记录数
};

struct dns_question {
    // question 中的内容是不固定的，故使用时需要将每一项copy进去
    int length;     // 查询长度
    unsigned short qtype;   // 查询类型
    unsigned short qclass;  // 查询类
    unsigned char *name;    // 查询名--长度不固定
};



// 一、 client sendto dns server
// @func: 构造DNS头部
int dns_create_header(struct dns_header *header) {
    // 1、为DNS头分配内存
    if (header == NULL) return -1;
    memset(header, 0, sizeof(struct dns_header));

    // 2、生成对话ID并填补内容
    srandom(time(NULL));
    header->id = random();// 随机生成会话ID：线程不安全
    header->flags = htons(0x0100);// 将主机字节序转为网络字节序
    header->questions = htons(1);// 表示有一个问题

    return 0;

}

// @func: 构造DNS问题
int dns_create_question(struct dns_question *question, const char *hostname) {
    // 1、为问题动态分配问题
    if (question == NULL || hostname == NULL) return -1;
    memset(question, 0, sizeof(struct dns_question));

    // 2、为DNS编码格式的域名动态分配内存空间，因为相比原始域名，DNS的域名要多出2给字节
    question->name = (char*) malloc(strlen(hostname) + 2);
    if (question->name == NULL) return -2;

    // 3、questions数据初始定义
    question->length = strlen(hostname) + 2;
    question->qtype = htons(1); // 查询类型：A 记录（IPv4 地址）
    question->qclass = htons(1);// 查询类：IN（Internet）


    // 4、转换普通域名为DNS域名
    // hostname: www.0voice.com转换为下面的
    // name: 3www60voice3com0
    const char delim[2] = ".";      // 定义分隔符
    char *qname = question->name;   // 后续不断移动指针，逐步填充数据

    char *hostname_dup = strdup(hostname); // 复制一份hostname字符串（动态分配内存）
    // 1. 先把要写入dns的第一部分（如www）拆分出来
    char *token = strtok(hostname_dup, delim); // 按照分隔符，拆分字符串

    while(token != NULL) {
        // 2. 把拆分出的token长度计算出来
        size_t len = strlen(token); // 获取当前部分的长度
        // 3. 把长度写入qname指向的位置
        *qname = len;   // 存储长度，例如写入3
        qname ++;       // 移动指针，指向可以写入标签内容的位置
        // 4. 把拆分出的token写入qname指向的后面的位置，len+1确保复制包含结尾的\0
        strncpy(qname, token, len+1); // 指定复制的长度
        qname += len;
        // 5. 获取下一个token
        // strtok是一个有状态的函数，上次拆完后，接下来会继续拆剩下的部分。
        token = strtok(NULL, delim); // 也不是线程安全的函数
    }
    // 释放内存
    free (hostname_dup);

}


// @func: 打包成一个完整的请求
// struct dns_hearder *header, 
// struct dns_question *question, 
// char *request：存放打包后的完整数据包
int dns_build_requestion(struct dns_header *header, struct dns_question *question, char *request, int rlen) {
    
    if (header == NULL || question == NULL || request == NULL) return -1;
    memset(request, 0, rlen);

    // header --> request
    //1. 先复制头部的12字节
    memcpy(request, header, sizeof(struct dns_header));
    int offset = sizeof(struct dns_header);

    // question --> request
    // 复制域名
    memcpy(request+offset, question->name, question->length);
    offset += question->length;
    // 复制查询类型
    memcpy(request+offset, &question->qtype, sizeof(question->qtype));
    offset += sizeof(question->qtype);
    // 复制查询类
    memcpy(request+offset, &question->qclass, sizeof(question->qclass));
    offset += sizeof(question->qclass);
    // 返回总长度
    return offset;

}


// 二、 客户端核心逻辑
// DNS服务器是基于无连接的，UDP
int dns_client_commit(const char *domain) {
    // 1. 创建UDP套接字
    // AF_INET: Address Family Internet（IPv4 地址族）
    // SOCK_DGRAM: Socket Datagram（数据报套接字）
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0); // 返回一个整数“套接字描述符”，大于0表示成功，小于0表示失败
    if (sockfd < 0) { // 使用sockfd来确定一个连接的标识，后续针对这个网络连接的操作都需要用到这个标识
        return -1;
    }
    printf("socket created: sockfd = %d\n", sockfd);

    // 2. 构造UDP服务器地址结构体（目的地址）
    struct sockaddr_in servaddr = {0}; // 定义IPV4专用地址结构，并初始化为0
    servaddr.sin_family = AF_INET;      // 使用ipv4地址族
    servaddr.sin_port = htons(DNS_SERVER_PORT);// 定义端口号
    servaddr.sin_addr.s_addr = inet_addr(DNS_SERVER_IP);// IP地址(需要使用二进制的网络序)

    // 3. UDP DNS客户端完整通信流程
        // 单纯的发一次消息过去，为UDP套接字绑定目标地址
    int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    printf("connect: %d\n", ret);
        // 构造DNS请求
    struct dns_header header = {0};
    dns_create_header(&header);
    struct dns_question question = {0};
    dns_create_question(&question, domain);

    char request[1024] = {0}; // 存储最终要发送的数据
    int length = dns_build_requestion(&header, &question, request, 1024);

    // request
    // 把数据发送到dns服务器那一侧
    //     从哪发-要发的数据-数据长度-标志位-目标地址-地址结构大小
    sendto(sockfd, request, length, 0, (struct sockaddr*)&servaddr, sizeof(struct sockaddr));

    // recive from
    // 接收响应，把数据进行回收
    char response[1024] = {0};
    struct sockaddr_in addr;
    size_t addr_len = sizeof(struct sockaddr_in);
        // 阻塞调用，程序停在此处等待DNS服务器回包
    int n = recvfrom(sockfd, response, sizeof(response), 0, (struct sockaddr*)&addr, (socklen_t*)&addr_len);

    printf("recvfrom : %d, %s\n", n, response);
    int i = 0;
    for (i=0; i<n; i++) {
        printf("%c", response[i]);
    }
    for (i=0; i<n; i++) {
        printf("%x", response[i]);
    }
    printf("\n");
    return n;

}



int main(int argc, char *argv[]) {

    if (argc < 2) return -1;
    dns_client_commit(argv[1]);
}








