#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include <pthread.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <deque>
using std::deque;

const static int DEFAULT_TIME = 10;             // 每隔10s检测一次
const static int DEFAULT_THREAD_STEP = 10;      // 创建或者销毁线程的步长
const static int MAX_NUMBER_OF_THREAD = 100;    // 线程最大数量
const static int MAX_NUMBER_OF_TASK = 100;      // 任务最大数量

typedef struct {
    void *(*function)(void *);
    void *arg;                              
} task;                                     // 线程任务结构体

class Thread_pool{
public:
    pthread_mutex_t glock;                  // 整个对象的全局锁
    pthread_mutex_t counter_lock;           // 用于记录线程状态的锁
    pthread_cond_t queue_not_full;          // 任务队列不满的条件锁
    pthread_cond_t queue_not_empty;         // 任务队列不空的条件锁

public:
    pthread_t threads[MAX_NUMBER_OF_THREAD]; // 存放线程ID
    pthread_t manager_tid;                  // 管理线程的线程ID
    deque<task> task_deque;    

public:
    int min_thread_num;                     // 最小线程数量
    int max_thread_num;                     // 最大线程数量
    int live_thread_num;                    // 存活线程数量
    int busy_thread_num;                    // 繁忙线程数量
    int destroy_thread_num;                 // 待销毁线程数量
    bool shutdown;                          // 线程池状态标志

public:
    Thread_pool(int, int);
    ~Thread_pool();
    void manager_thread();
    void pool_thread();
    bool is_thread_alive(pthread_t);
    void add(void*(*function)(void *arg), void *arg);
    static void *p_pool_thread(void *arg){
        static_cast<Thread_pool *>(arg)->pool_thread();
    }
    static void *p_manager_thread(void *arg){
        static_cast<Thread_pool *>(arg)->manager_thread();
    }
};
#endif
