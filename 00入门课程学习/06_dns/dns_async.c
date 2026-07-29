#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>     // 套接字相关的函数 (socket, sendto, recvfrom)
#include <netinet/in.h>     // Internet地址结构体 (sockaddr_in, inet_addr)
#include <unistd.h>         // 标准系统调用 (close)

#include <pthread.h>
#define DNS_SERVER_PORT 53  // 服务默认端口
#define DNS_SERVER_IP   "114.114.114.114"   // 国内公用的DNS服务器

/*
实现一个多线程（异步）DNS，主线程不受阻塞。通过任务队列管理和监控查询状态。
作用是向DNS服务器发送域名查询请求，并接收解析结果。总体设计方案
核心机制
任务队列：管理所有正在进行的查询
工作线程：每个查询启动一个线程执行同步查询
状态管理：跟踪每个查询的进度（等待/完成/失败）
非阻塞检查：主线程可以随时检查查询是否完成
*/

// DNS查询任务结构
typedef struct dns_task {
    char domain[256];       // 待查询的域名
    char response[2048];    // 响应数据
    int response_len;    // 响应长度
    int status;          // 0:等待, 1:查询中, 2:完成, -1:失败
    pthread_t thread;
    struct dns_task *next;  // 链表指针
} dns_task_t;

// 全局链表头
static dns_task_t *g_task_list = NULL;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;



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

// ============ 线程工作函数 ============

void* dns_thread_work(void *arg) {
    dns_task_t *task = (dns_task_t*)arg;
    
    printf("[线程 %lu] 开始查询: %s\n", pthread_self(), task->domain);
    
    // 1. 创建socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        task->status = -1;
        return NULL;
    }
    
    // 设置超时
    struct timeval tv = {5, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    // 2. 构造服务器地址
    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(DNS_SERVER_PORT);
    servaddr.sin_addr.s_addr = inet_addr(DNS_SERVER_IP);
    
    // 3. 构造DNS请求
    struct dns_header header = {0};
    dns_create_header(&header);
    struct dns_question question = {0};
    dns_create_question(&question, task->domain);
    
    char request[1024] = {0};
    int length = dns_build_requestion(&header, &question, request, 1024);
    
    // 4. 发送请求
    sendto(sockfd, request, length, 0,
           (struct sockaddr*)&servaddr, sizeof(servaddr));
    
    // 5. 接收响应
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int n = recvfrom(sockfd, task->response, sizeof(task->response), 0,
                    (struct sockaddr*)&addr, &addr_len);
    
    free(question.name);
    close(sockfd);
    
    // 6. 更新状态
    if (n > 0) {
        task->response_len = n;
        task->status = 1;  // 成功
        printf("[线程 %lu] ✓ 完成: %s (%d 字节)\n", pthread_self(), task->domain, n);
    } else {
        task->status = -1; // 失败
        printf("[线程 %lu] ✗ 失败: %s\n", pthread_self(), task->domain);
    }
    
    return NULL;
}

// ============ 链表操作函数 ============

// 添加任务到链表（头插法）
int dns_add_task(const char *domain) {
    // 1. 创建任务节点
    dns_task_t *task = (dns_task_t*)calloc(1, sizeof(dns_task_t));
    if (!task) return -1;
    
    strncpy(task->domain, domain, sizeof(task->domain)-1);
    task->status = 0;  // 查询中
    
    // 2. 创建线程
    if (pthread_create(&task->thread, NULL, dns_thread_work, task) != 0) {
        free(task);
        return -1;
    }
    pthread_detach(task->thread);
    
    // 3. 添加到链表（头插法）
    pthread_mutex_lock(&g_mutex);
    task->next = g_task_list;  // 新节点指向旧头
    g_task_list = task;        // 头指针指向新节点
    pthread_mutex_unlock(&g_mutex);
    
    printf("[主线程] 添加任务: %s\n", domain);
    return 0;
}

// 从链表中查找并移除已完成的任务
dns_task_t* dns_find_completed_task() {
    pthread_mutex_lock(&g_mutex);
    
    dns_task_t *prev = NULL;
    dns_task_t *curr = g_task_list;
    
    while (curr) {
        if (curr->status != 0) {  // 已完成或失败
            // 从链表中移除
            if (prev) {
                prev->next = curr->next;
            } else {
                g_task_list = curr->next;
            }
            
            pthread_mutex_unlock(&g_mutex);
            return curr;  // 返回已完成的任务
        }
        prev = curr;
        curr = curr->next;
    }
    
    pthread_mutex_unlock(&g_mutex);
    return NULL;  // 没有已完成的任务
}

// 打印所有任务状态
void dns_print_tasks() {
    pthread_mutex_lock(&g_mutex);
    
    printf("\n========== 任务列表 ==========\n");
    dns_task_t *curr = g_task_list;
    int count = 0;
    
    while (curr) {
        const char *status_str;
        switch(curr->status) {
            case 0: status_str = "查询中"; break;
            case 1: status_str = "已完成"; break;
            case -1: status_str = "失败"; break;
            default: status_str = "未知";
        }
        printf("  [%d] %s -> %s\n", ++count, curr->domain, status_str);
        curr = curr->next;
    }
    
    if (count == 0) {
        printf("  (无任务)\n");
    }
    printf("===============================\n\n");
    
    pthread_mutex_unlock(&g_mutex);
}

// 等待所有任务完成
void dns_wait_all(int timeout_sec) {
    int elapsed = 0;
    
    while (elapsed < timeout_sec) {
        pthread_mutex_lock(&g_mutex);
        int has_active = 0;
        dns_task_t *curr = g_task_list;
        while (curr) {
            if (curr->status == 0) {
                has_active = 1;
                break;
            }
            curr = curr->next;
        }
        pthread_mutex_unlock(&g_mutex);
        
        if (!has_active) {
            printf("\n所有任务已完成！\n");
            return;
        }
        
        printf("等待任务完成... %d/%d 秒\n", elapsed+1, timeout_sec);
        sleep(1);
        elapsed++;
    }
    
    printf("\n等待超时！\n");
}

// ============ 主函数 ============

int main(int argc, char *argv[]) {
    printf("========== 多线程异步DNS（带链表） ==========\n");
    int i = 0;
    
    // 1. 添加多个DNS查询任务
    if (argc > 1) {
        // 命令行指定域名
        dns_add_task(argv[1]);
    } else {
        // 默认查询多个域名
        char *domains[] = {
            "www.baidu.com",
            "www.qq.com",
            "www.163.com",
            "www.sina.com"
        };
        
        for (i = 0; i < 4; i++) {
            dns_add_task(domains[i]);
            usleep(10000);  // 稍微延迟，避免冲突
        }
    }
    
    // 2. 主线程做其他事情
    printf("\n主线程执行其他工作...\n");
    for (i = 0; i < 3; i++) {
        printf("[主线程] 工作 %d/3\n", i+1);
        dns_print_tasks();  // 打印当前任务状态
        sleep(1);
    }
    
    // 3. 等待所有任务完成
    dns_wait_all(10);
    
    // 4. 获取并打印所有完成的任务
    printf("\n========== 查询结果 ==========\n");
    dns_task_t *task;
    int success_count = 0;
    int fail_count = 0;
    
    while ((task = dns_find_completed_task()) != NULL) {
        if (task->status == 1) {
            printf("✓ %s -> 成功 (%d 字节)\n", task->domain, task->response_len);
            // 打印部分响应数据
            printf("  响应数据: ");
            for ( i = 0; i < (task->response_len < 16 ? task->response_len : 16); i++) {
                printf("%02x ", (unsigned char)task->response[i]);
            }
            printf("...\n");
            success_count++;
        } else {
            printf("✗ %s -> 失败\n", task->domain);
            fail_count++;
        }
        free(task);
    }
    
    printf("\n统计: 成功 %d, 失败 %d\n", success_count, fail_count);
    
    return 0;
}