#ifndef _COROUTINE_H_
#define _COROUTINE_H_


#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// 前向声明
struct coroutine;
struct scheduler;

// 协程状态
enum CoroutineState {
    COROUTINE_READY,   // 刚创建，等待第一次运行
    COROUTINE_RUNNING, // 正在运行
    COROUTINE_SUSPEND, // 挂起（让出CPU）
    COROUTINE_DEAD     // 结束
};

struct coroutine {
    int id;
    enum CoroutineState state;
    ucontext_t ctx;
    char *stack;
    struct scheduler *sch;
    void (*func)(void *arg);
    void *arg;
    struct coroutine *next;
};

// 调度器结构体（单线程）
struct scheduler {
    ucontext_t main_ctx;        // 主上下文（Reactor 线程本身）
    struct coroutine *running;  // 当前正在运行的协程
    struct coroutine *ready_queue_head; // 就绪队列头
    struct coroutine *ready_queue_tail; // 就绪队列尾
};
// 初始化调度器
struct scheduler* scheduler_create();

// 销毁调度器
void scheduler_destroy(struct scheduler *sch);

// 创建协程
struct coroutine* coroutine_create(struct scheduler *sch, void (*func)(void *), void *arg);

// 恢复协程运行
void coroutine_resume(struct coroutine *co);

// 当前协程让出 CPU
void coroutine_yield();

// 销毁协程
void coroutine_destroy(struct coroutine *co);

#endif