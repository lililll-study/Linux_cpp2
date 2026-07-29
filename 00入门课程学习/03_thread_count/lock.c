#include <stdio.h>
#include <pthread.h>


#define THREAD_COUNT 10

pthread_mutex_t mutex;
pthread_spinlock_t spinlock;

int inc(int *value, int add){
    int old;

    __asm__ volatile(
        "lock; xaddl %2, %1;"
        : "=a" (old)
        : "m" (*value), "a" (add)
        : "cc", "memory"
    );
    return old;
}


void *thread_callback(void *arg) {

    int *pcount = (int *)arg;
    int i=0;
    while (i++ < 100000)
    {
#if 0
        // 线程不安全的写法
        // 未加锁，一个线程执行时，其他线程可能也在执行，导致线程写入的数据被其他线程覆盖
        // 
        (*pcount)++;
#elif 0
        // 线程安全的写法
        // 线程加锁，一个线程在执行时，另外一个线程进不来
        pthread_mutex_lock(&mutex);
        (*pcount)++;
        pthread_mutex_unlock(&mutex);
#elif 0
        pthread_spin_lock(&spinlock);
        (*pcount)++;
        pthread_spin_unlock(&spinlock);

#else
        inc(pcount, 1);

#endif
        // 把(*pcount)++这条指令，通过一个函数实现
        usleep(1);
    }
}

int main () {

    pthread_t threadid[THREAD_COUNT] = {0};

    // 线程创建之前进行初始化
    pthread_mutex_init(&mutex, NULL);
    pthread_spin_init(&spinlock, PTHREAD_PROCESS_SHARED);

    int i = 0;
    int count = 0;
    for (i = 0; i < THREAD_COUNT; i++)
    {
        pthread_create(&threadid[i], NULL, thread_callback, &count);
    }
    for ( i = 0; i < 100; i++)
    {
        printf("count = %d\n", count);
        sleep(1);
    }
    
    
}