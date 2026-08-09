#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <stdlib.h>


#include "server.h"
extern struct scheduler *g_sched;
// 协程调度器实现
#define COROUTINE_STACK_SIZE (1024 * 128) // 128KB 栈
struct scheduler* scheduler_create() {
    struct scheduler *sch = (struct scheduler*)malloc(sizeof(struct scheduler));
    memset(sch, 0, sizeof(struct scheduler));
    sch->running = NULL;
    sch->ready_queue_head = NULL;
    sch->ready_queue_tail = NULL;
    return sch;
}

void scheduler_destroy(struct scheduler *sch) {
    free(sch);
}

// 协程入口包装函数
void coroutine_entry(struct coroutine *co) {
    co->func(co->arg);
    co->state = COROUTINE_DEAD;
    coroutine_yield(co); // 协程结束，让出CPU
}

struct coroutine* coroutine_create(struct scheduler *sch, void (*func)(void *), void *arg) {
    struct coroutine *co = (struct coroutine*)malloc(sizeof(struct coroutine));
    co->sch = sch;
    co->func = func;
    co->arg = arg;
    co->state = COROUTINE_READY;
    co->stack = malloc(COROUTINE_STACK_SIZE);

    getcontext(&co->ctx);
    co->ctx.uc_stack.ss_sp = co->stack;
    co->ctx.uc_stack.ss_size = COROUTINE_STACK_SIZE;
    co->ctx.uc_link = &sch->main_ctx; // 协程结束后回主上下文
    makecontext(&co->ctx, (void (*)(void))coroutine_entry, 1, co);

    return co;
}

void coroutine_destroy(struct coroutine *co) {
    if (co->stack) free(co->stack);
    free(co);
}
// 将协程加入就绪队列
void schedule_add(struct scheduler *sch, struct coroutine *co) {
    if (sch->ready_queue_tail) {
        sch->ready_queue_tail->next = co;
    } else {
        sch->ready_queue_head = co;
    }
    sch->ready_queue_tail = co;
    co->next = NULL;
}

void coroutine_resume(struct coroutine *co) {
    if (co->state == COROUTINE_DEAD) return;

    struct scheduler *sch = co->sch;
    struct coroutine *curr = sch->running;
    
    sch->running = co;
    co->state = COROUTINE_RUNNING;

    if (curr) {
        // 从当前协程切到目标协程
        swapcontext(&curr->ctx, &co->ctx);
    } else {
        // 从主线程切到目标协程
        swapcontext(&sch->main_ctx, &co->ctx);
    }

    // 切回来后，恢复运行标识
    sch->running = curr;
}

void coroutine_yield() {
    struct scheduler *sch = g_sched; // 需要定义全局 g_sched 或者传参
    if (!sch) return;

    struct coroutine *curr = sch->running;
    if (!curr) return; // 主线程不能 yield 自己，只能由调度器接管

    curr->state = COROUTINE_SUSPEND;
    
    // 将当前协程重新放入就绪队列（如果是异步IO模式，这里不应该直接放回，而应该由事件触发放入）
    // 但为了简化，这里假设 yield 是因为忙碌让出，或者逻辑切换
    // 在本模型中，yield 意味着 "等待事件"，所以由 epoll 回调去 resume，这里只做切换回主线程
    swapcontext(&curr->ctx, &sch->main_ctx);
}


#define CONNECTION_SIZE     1048576 // 1024^2
#define MAX_PORTs           20
#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

int epfd = 0;
struct timeval begin;
// struct conn conn_list[CONNECTION_SIZE] = {0};
struct conn *conn_list = NULL;// 优化为动态分配内存
struct scheduler *g_sched = NULL; // 全局调度器


int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);
int event_register(int fd, int event);





int set_event(int fd, int event, int flag) {

    struct epoll_event ev;
    ev.events = event;
    ev.data.fd = fd;
    if (flag) { // 1:添加
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    } else {
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
     printf("accept finished clientfd: %d\n", clientfd);
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

// 协程业务处理函数
void coroutine_handler(void *arg) {
    struct conn *c = (struct conn *)arg;

    // 1. 解析请求：此时数据已经在rbuffer
    printf("Coroutine 解析请求 fd: %d, data: %s\n", c->fd, c->rbuffer);
    http_requset(c);

    // 2. 构造响应并发送（http_response 内部完成所有发送）
    http_response(c);

    // 3. 关闭连接
    printf("Closing connection %d after response.\n", c->fd);
    close(c->fd);
    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
    
    c->co->state = COROUTINE_DEAD;

}

int recv_cb(int fd) {
    struct conn *c = &conn_list[fd];
    memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
    int data_len = recv(fd, conn_list[fd].rbuffer, BUFFER_LENGTH, 0);

    if (data_len <= 0) { // 断开连接了
        // close(clientfd);
        printf("client disconnect: %d\n", fd);

        if (c->co) {
            c->co->state = COROUTINE_DEAD;
        }

        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);   // unfinished 
        return 0;
    }

    conn_list[fd].rlength = data_len;
    // printf("recv buffer: %s\n", conn_list[fd].rbuffer);

#if 1

    if (c->co == NULL) {
        // 创建协程，入口函数为coroutine_handler
        c->co = coroutine_create(g_sched, coroutine_handler, &conn_list[fd]);
    }
    // 唤醒协程：这里只是放入调度逻辑，真正的切换在 execute_ready_coroutines
    schedule_add(g_sched, c->co);

#else
// 接受完数据后做request请求
    http_requset(&conn_list[fd]);

    set_event(fd, EPOLLOUT, 0);

#endif
    return data_len;
}

int send_cb(int fd) {
#if 1
    struct conn *c = &conn_list[fd];
    // 在 send_cb 中实际发送数据
    int sent = send(fd, c->wbuffer, c->wlength, 0);
    printf("send_cb: Sent %d bytes to fd %d\n", sent, fd);
    
    // 唤醒协程，让它知道发送已完成
    if (c->co && c->wait_event == EPOLLOUT) {
        schedule_add(g_sched, c->co);
        c->wait_event = 0;
        // 移除 EPOLLOUT 事件
        set_event(fd, EPOLLIN, 0);
    }
#else
// 发送数据之前做response响应
    http_response(&conn_list[fd]);

    int send_len = 0;

    if (conn_list[fd].status == 1) {
        // printf("send buffer: %s\n", conn_list[fd].wbuffer);
        send_len = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlength, 0);
        set_event(fd, EPOLLOUT, 0);
    } else if (conn_list[fd].status == 2) {
        set_event(fd, EPOLLOUT, 0);
    } else if (conn_list[fd].status == 0) {
        set_event(fd, EPOLLIN, 0);
    }
#endif
    return sent;
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

void execute_ready_coroutines() {
    while (g_sched->ready_queue_head) {
        struct coroutine *co = g_sched->ready_queue_head;
        g_sched->ready_queue_head = co->next;
        if (!g_sched->ready_queue_head) g_sched->ready_queue_tail = NULL;
        
        if (co->state == COROUTINE_DEAD) {
            coroutine_destroy(co);
            continue;
        }
        
        coroutine_resume(co);
    }
}

int main() {

    unsigned short port = 2000;

    // 1. 初始化调度器
    g_sched = scheduler_create();
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

    while(1) {
        struct epoll_event events[1024] = {0};
        int nready = epoll_wait(epfd, events, 1024, -1);
    
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

        // scheduler:执行所有就绪的程序
        // 刚才被recv和wake唤醒的协程，都在这里执行
        // 可以避免在epoll中进行复杂的上下文切换
        execute_ready_coroutines();
    }


}