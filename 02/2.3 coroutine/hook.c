// 启动Linux额外拓展函数，使得可以调用dlsym
#define _GNU_SOURCE

#include <stdio.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>


#include <ucontext.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>


// (*read_t):函数指针，         这个函数接收 3 个参数
// ssize_t: 有符号整型，专门用来表示“字节数”或“大小”
// 注意：ssize_t表示的数可能有负数
// size_t是无符号的，不能是负数

typedef ssize_t (*read_t)(int fd, void *buf, size_t count);
read_t read_f = NULL;

typedef ssize_t (*write_t)(int fd, void *buf, size_t count);
write_t write_f = NULL;


// 劫持read和wirte的系统调用：把阻塞的 IO 操作变成非阻塞的协程切换。
// 解决的问题：
// 调用read时，如果没有数据，IO会阻塞
// 修改后，没有数据则切换上下文
ssize_t read(int fd, void *buf, size_t count) {
    struct pollfd fds[1] = {0};

    fds[0].fd = fd;
    fds[0].events = POLLIN;


    // res>0有数据，小于0没数据
    int res = poll(fds, 1, 0); // timeout = 0，不阻塞！

    if (res <= 0) {
        // 在这里，一个fd对应一个协程，切换之前把fd加入epoll中
        // 
        swapcontext();
    }
    // 有数据则读取
    // 此处调用的是read_f。而read_f在初始化hook时，用dlsym指向了真正的read函数
    ssize_t ret = read_f(fd, buf, count);
    printf("read: %s\n", (char *)buf);
    return ret;

}


ssize_t write(int fd, const void *buf, size_t count) {
    printf("write: %s\n", (const char *)buf);

    return write_f(fd, buf, count);
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


void init_hook() {
    // 判断全局变量 read_f 是否为空。read_f 是一个函数指针，用于存储真正 read 函数的地址。
    if (!read_f) {
        read_f = dlsym(RTLD_NEXT, "read");
    }
    // RTLD_NEXT: 表示“下一个符号”
    // dlsym:根据函数名的字符串（如 "read"），拿到这个函数在内存中的实际地址，它是动态链接器提供给用户的“反射”接口。

    if (!write_f) {
        write_f = dlsym(RTLD_NEXT, "write");
    }
}






int main() {

    init_hook();
    int sockfd = init_server(2000);

    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
    int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
    printf("accept\n");

    while (1)
    {
        char buffer[128] = {0};
        int data_len = read(clientfd, buffer, 128);
        if (data_len == 0) {
            break;
        }
        write(clientfd, buffer, data_len);
        printf("sockfd: %d, clientfd: %d, data_len: %d, buffer: %s\n", sockfd, clientfd, data_len, buffer);
    }
    
    return 0;




}









