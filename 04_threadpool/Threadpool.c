#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/*
    1. 链表操作宏
    双向链表支持 O(1) 插入和删除
    宏的方式避免函数调用开销
    do-while(0) 保证宏像语句一样安全
*/

// @func：头插法 o(1)插入
#define LIST_INSERT(item, list) do{ \
    item->previous = NULL; \
    item->next = list; \
    list = item; \
}while(0)
// @func：删除节点 o(1)删除
        // 定义宏LIST_REMOVE，用于从链表中删除一个联系人
        // item表示要删除的联系人，list表示链表的头指针
#define LIST_REMOVE(item, list) do{ \
    if(item->previous) { item->previous->next = item->next; } \
    if(item->next) { item->next->previous = item->previous; } \
    if(list == item) { list = item->next; } \
    item->previous = item->next = NULL; \
}while(0)


/*
    2. 数据结构部分定义
        nTask描述要做什么
        nWork描述要谁来做
        nManager统一管理所有的工作者和任务
    使用指针的意义是：所有线程都指向同一个内存地址的任务链表和工作者链表，实现数据共享。
*/

// 定义线程池
    // 1.1 任务节点
struct nTask {
    void (*task_func)(void *arg);   // 要执行的函数指针：每一个人到营业厅的任务不一样，定义执行的任务
    void *user_data;                // 函数的参数：// 每一个任务都有相应的参数
    struct nTask *previous;              // 链表前驱
    struct nTask *next;             // 链表后继
};

    // 1.2 工作者节点
struct nWorker {
    pthread_t threadid;     // 线程ID
    int terminate;          // 线程是否终止的标志（新增！用于安全退出）
    // 通过nWorker操作nManager，需要有nManager对象
    struct nManager *manager; // 所属线程池
    struct nWorker *previous;   // 链表前驱
    struct nWorker *next;   // 链表后继
};
    // 1.3 线程池管理器
typedef struct nManager {
    struct nTask *tasks;    // 任务链表头
    struct nWorker *workers;   // 工作者链表头
    pthread_mutex_t mutex;  // 互斥锁，保护任务链表和工作者链表的访问
    pthread_cond_t cond;  // 条件变量，用于通知工作线程有新任务到来
}ThreadPool;



/*
    3. 核心的工作循环
        // 同步机制：
        // mutex (互斥锁，保护队列)，cond（唤醒线程）
*/
        // 线程池的回调函数：回调函数不等于任务，他表示线程池的工作线程在等待任务时的行为
        // 查看有没有任务，如果有就执行任务，如果没有就等待
// @func：消费者们(工作线程)，所有线程都在回调函数中循环等待
static void *nThreadPoolCallback(void *arg){
    // 创建线程时把worker指针塞进来，我们把它取出来
    struct nWorker *worker = (struct nWorker *)arg;   // 获取工作线程的结构体
    
    printf("nThreadPoolCallback\n");   // 打印线程ID，方便调试
    while(1) {// 进入循环状态：只要你不主动退出，这个线程的 CPU 时间片就会一直执行这段代码。
        // === 步骤1：加锁 ===
        pthread_mutex_lock(&worker->manager->mutex);    // 上锁，保护任务链表和工作者链表的访问

        // === 步骤2：没任务就睡觉 ===
        while (worker->manager->tasks == NULL) {        // 如果没有订单，线程就等待，然后把mutex锁释放，使得生产者能够向锁中添加任务
            if (worker->terminate) break;
            pthread_cond_wait(&worker->manager->cond, &worker->manager->mutex);   // 等待条件变量，释放互斥锁
        }

        // === 步骤3：检查是否要退出 ===
        if (worker->terminate) {
            pthread_mutex_unlock(&worker->manager->mutex);  // 解锁，允许其他线程访问任务链表和工作者链表,防止死锁现象
            break;                                       // 如果线程被要求终止，就跳出循环，结束线程
        }

        // === 步骤4：取出任务 ===
        struct nTask *task = worker->manager->tasks;   // 获取任务链表的头节点
        LIST_REMOVE(task, worker->manager->tasks);   // 从任务链表中删除任务节点

        // === 步骤5：立即解锁（！实现高并发） ===
        pthread_mutex_unlock(&worker->manager->mutex);   // 解锁，允许其他线程访问任务链表和工作者链表
        
        // === 步骤6：执行任务（无锁状态） ===
        task->task_func(task);   // 抢任务是多线程执行的，在执行任务时，不需要锁住任务
        // 任务执行完，回到循环头部
        // **这就是线程池能高并发的根本原因：只在“抢资源”时排队，在“处理业务”时并行。
    }
    free(worker);   // 释放工作线程的结构体内存
    return NULL;
}

/*
    4. 线程池的核心API层
        // 所有API 通过mutex 和 cond 保证线程安全
*/
    // 4.1 创建线程池
    // @func：完成了线程池的初始化
        // 初始化条件变量（用于线程间通信）
        // 初始化互斥锁（用于保护共享数据）
        // 创建 N 个工作线程（并加入链表管理）
int nThreadPoolCreate(ThreadPool *pool, int numWorkers){
    // 1. 参数检查（防止空指针崩溃）
    if (pool == NULL) return -1;
    if (numWorkers < 1) numWorkers = 1;
    memset(pool, 0, sizeof(ThreadPool));   // 清空线程池结构体，初始化为0

    // 2. 初始化条件变量：让 pool->cond 准备好被使用
    // 正常写法：pthread_cond_init(&pool->cond, NULL);
    // 此处先把条件变量
    pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;   // 静态初始化器的值
    memcpy(&pool->cond, &blank_cond, sizeof(pthread_cond_t));   // pool是在堆上动态分配的，通过memcpy把blank_cond拷贝到pool->cond
    // 3. 初始化互斥锁
    pthread_mutex_init(&pool->mutex, NULL);



    // 4. 创建numWorkers个工作线程
    int i = 0;
    for (i=0; i<numWorkers; i++){
        // 4.1 分配员工的工牌
        struct nWorker *worker =(struct nWorker *) malloc(sizeof(struct nWorker));
        if (worker == NULL) {
            perror("malloc worker error");
            return -2;
        }
        // 4.2 工牌清零
        memset(worker, 0, sizeof(struct nWorker));

        // 4.3 告诉员工谁是他的老板
        worker->manager = pool; // 每个线程都知道自己的线程池

        // 4.4 招聘员工（创建线程）
        // 创建线程，执行nThreadPoolCallback函数，传入worker作为参数
        int ret = pthread_create(&worker->threadid, NULL, nThreadPoolCallback, worker);
        if (ret) {
            perror("pthread_create error");
            free(worker);
            return -3;
        }
        // 4.5 把员工登记在册
        LIST_INSERT(worker, pool->workers);// 插入worker到线程池的workers链表中
    }
    return 0;

}


    // 4.2 销毁线程池
    // @func：
int nThreadPoolDestory(ThreadPool *pool){

    // 1. 设置所有线程的终止标志
    struct nWorker *worker = NULL;
    for (worker = pool->workers;worker != NULL;worker = worker->next){
        worker->terminate = 1;   // 设置线程终止标志
    }

    // 2. 唤醒所有线程，让它们检查终止标志
    pthread_mutex_lock(&pool->mutex);   // 上锁，保护任务链表和工作者链表的访问
    pthread_cond_broadcast(&pool->cond);   // 通知所有线程，唤醒它们
    pthread_mutex_unlock(&pool->mutex);   // 解锁，允许其他线程访问任务链表和工作者链表

    // 3. 清空登记表，每个被唤醒的线程，都会在while中检查terminate标志，然后break跳出，接着free释放
    pool->workers = NULL;   // 清空线程池的workers链表
    pool->tasks = NULL;   // 清空线程池的tasks链表

    return 0;

}

    // 4.3 向线程池推送任务
    // @func：加锁，把任务添加到线程池的任务队列，然后唤醒线程，再解锁
    // 线程被唤醒后取出任务并执行
int nThreadPoolPushTask(ThreadPool *pool, struct nTask *task){

    pthread_mutex_lock(&pool->mutex);
    LIST_INSERT(task, pool->tasks); // 把task添加到pool
    pthread_cond_signal(&pool->cond);// 通知一个线程，有新任务到来
    pthread_mutex_unlock(&pool->mutex);

}

// pthread_cond_broadcast(&pool->cond); 唤醒所有等待这个条件变量的线程
// pthread_cond_signal(&pool->cond); 唤醒一个等待这个条件变量的线程

// sdk --> debug thread pool

#if 1
// 初始化线程池的线程的数量
#define THREADPOOL_INIT_COUNT 20
// 定义总任务数量
#define TASK_INIT_SIZE 1000

void task_entry(void *arg){

    struct nTask *task = (struct nTask*)arg;

    int *idx = (int *)task->user_data;

    printf("idx: %d\n", *idx);

    free(task->user_data);
    free(task);
}

int main(void){
    ThreadPool pool;

    nThreadPoolCreate(&pool, THREADPOOL_INIT_COUNT);

    int i = 0;
    for (i=0; i<TASK_INIT_SIZE; i++){   //创建1000个任务
        struct nTask *task = (struct nTask *) malloc(sizeof(struct nTask));
        if (task == NULL){          // 分配内存失败，打印错误信息并退出
            perror("malloc");
            exit(1);
        }
        memset(task, 0, sizeof(struct nTask));// 把task结构体清零，防止野指针

        task-> task_func = task_entry;      // ③ 设置任务函数
        task -> user_data = malloc(sizeof(int));// ④ 分配参数内存
        *(int*)task->user_data = i;         // ⑤ 设置参数值（0~999）

        nThreadPoolPushTask(&pool, task);   // ⑥ 提交任务
    }
    // 等待一会儿，让任务执行完成
    sleep(2);
    // 销毁线程池
    nThreadPoolDestory(&pool);
    
    return 0;
}

#endif

