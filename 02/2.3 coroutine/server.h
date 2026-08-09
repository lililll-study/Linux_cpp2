
#ifndef _SERVER_H_
#define _SERVER_H_

#include "coroutine.h"

typedef int (*RCALLBACK)(int fd);
#define BUFFER_LENGTH       1024


// 封装IO，读数据写数据内存，以及相应的回调函数
struct conn {
    int fd;

    int file_fd;        // 文件描述符
    off_t file_size;    // 文件大小
    int header_len;     // 响应头长度

    char rbuffer[BUFFER_LENGTH];
    int rlength;

    char wbuffer[BUFFER_LENGTH];
    int wlength;

    // char *payload;  // 解码后的数据
    // char mask[4];   // 掩码

    RCALLBACK send_callback; 
    union {// recv和accept是一个或的关系
        RCALLBACK recv_callback;
        RCALLBACK accept_calllback;
    } r_action;

    int status;

    // 协程字段
    struct coroutine *co;
    int wait_event; // 记录协程在等待的事件 (EPOLLIN/EPOLLOUT)
    
};


int http_requset(struct conn *c);
int http_response(struct conn *c);
// // WebSocket 相关函数声明（在 main 函数之前）
// int ws_request(struct conn *c);
// int ws_response(struct conn *c);

#endif