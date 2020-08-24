#include "thread_pool.h"
using std::cout;
using std::endl;

Thread_pool::Thread_pool(int min, int max){
    min_thread_num = min;
    max_thread_num = max;
    busy_thread_num = 0;
    live_thread_num = min_thread_num;
    shutdown = false;
    // 初始化锁及条件
    pthread_mutex_init(&glock, NULL);
    pthread_mutex_init(&counter_lock, NULL);
    if(pthread_cond_init(&queue_not_full, NULL) != 0
      || pthread_cond_init(&queue_not_empty, NULL) != 0)
    {
        cout << "\033[31mcond init fail...\033[0m" << endl;
        exit(-1);
    }
    // 创建工作线程
    for(int i = 0; i < min_thread_num; i++){
        if(0 != pthread_create(&threads[i], NULL, p_pool_thread, (void*)this)){
            cout << "\033[31mcreate thread fail...\033[0m" << endl;
        }
        else{
            cout << "starting thread " << threads[i] << endl;
        }
    }
    if(0 != pthread_create(&manager_tid, NULL, p_manager_thread, (void*)this))
        cout << "\033[31mcreate manager thread fail...\033[0m" << endl;
}
 

void Thread_pool::pool_thread(){
    task cur_task;
    while(true){
        pthread_mutex_lock(&glock);
        // 没有任务，阻塞
        while(task_deque.empty() && !shutdown){
            cout << "thread " << pthread_self() << " is waiting..." << endl;
            pthread_cond_wait(&queue_not_empty, &glock);
            // 没有任务时被唤醒，让该线程自杀
            if(destroy_thread_num > 0){
                destroy_thread_num--;
                 // 当且仅当剩余活着的线程大于最小线程数量，结束当前线程
                 if(live_thread_num > min_thread_num){
                    cout << "\033[34mthread " << pthread_self() << " is destroying\033[0m" << endl;
                    live_thread_num--;
                    pthread_mutex_unlock(&glock);
                    pthread_exit(NULL);
                
                 }
            }
        }

        if(shutdown){
            pthread_mutex_unlock(&glock);
            cout << "\033[31mthread " << pthread_self() << " is destroying\033[0m" << endl;
            pthread_exit(NULL);
        }

        cur_task.function = task_deque.front().function;
        cur_task.arg = task_deque.front().arg;
        task_deque.pop_front();

        pthread_cond_broadcast(&queue_not_full);
        pthread_mutex_unlock(&glock);

        pthread_mutex_lock(&counter_lock);
        busy_thread_num++;
        pthread_mutex_unlock(&counter_lock);

        (*(cur_task.function))(cur_task.arg);

        cout << "\033[31mthread " << pthread_self() << " end working\033[0m" << endl;
        pthread_mutex_lock(&counter_lock);
        busy_thread_num++;
        pthread_mutex_unlock(&counter_lock);
    }
    pthread_exit(NULL);
}

void Thread_pool::manager_thread(){
    while(!shutdown){
        sleep(DEFAULT_TIME);
        pthread_mutex_lock(&glock);
        int cur_live_thread_num = live_thread_num;
        int deque_size = task_deque.size();
        pthread_mutex_unlock(&glock);

        pthread_mutex_lock(&counter_lock);
        int cur_busy_thread_num = busy_thread_num;
        pthread_mutex_unlock(&counter_lock);

        if(deque_size > min_thread_num && cur_live_thread_num < max_thread_num){
            pthread_mutex_lock(&glock);
            int add = 0;
            for(int i = 0; i < max_thread_num && add < DEFAULT_THREAD_STEP && cur_live_thread_num < max_thread_num; i++){
                if(!threads[i]){
                    pthread_create(&threads[i], NULL, p_pool_thread, (void*)this);
                    add++;
                    live_thread_num++;

                }
            }
            pthread_mutex_unlock(&glock);
        }

        if(cur_busy_thread_num * 2 < live_thread_num && live_thread_num > min_thread_num){
            pthread_mutex_lock(&glock);
            destroy_thread_num = DEFAULT_THREAD_STEP;
            pthread_mutex_lock(&glock);
            for(int i = 0; i < DEFAULT_THREAD_STEP; i++){
                pthread_cond_signal(&queue_not_empty);
            }
        }
    }
}


bool Thread_pool::is_thread_alive(pthread_t pid){
    int kill_r = pthread_kill(pid, 0);
    if(kill_r == ESRCH) return false;
    return true;
}

Thread_pool::~Thread_pool(){
    pthread_mutex_lock(&glock);
    pthread_mutex_destroy(&glock);
    pthread_mutex_lock(&counter_lock);    
    pthread_mutex_destroy(&counter_lock);
    pthread_cond_destroy(&queue_not_full);
    pthread_cond_destroy(&queue_not_empty);
}

void Thread_pool::add(void*(*function)(void *arg), void *arg){
    pthread_mutex_lock(&glock);
    while(MAX_NUMBER_OF_TASK == task_deque.size() && !shutdown){
        pthread_cond_wait(&queue_not_full, &glock);
    }

    if(shutdown) pthread_mutex_unlock(&glock);

    task cur_task;
    cur_task.function = function;
    cur_task.arg = arg;
    task_deque.push_back(cur_task);

    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&glock);
}


#if test
void *process(void *arg){
    cout << "thread " << pthread_self() << " is working" << endl;
    sleep(1);
    cout << "thread " << pthread_self() << " end working" << endl;

}

int main()
{
    Thread_pool pool(3, MAX_NUMBER_OF_THREAD);
    cout << "033[31mthread inited" << endl;

    int task[190];
    for(int i = 0; i < 190; i++){
        task[i] = i;
        cout << "add task " << i << endl;
        pool.add(process, (void*)&task[i]);

    }

    sleep(10);
    cout << "live thread number: " << pool.live_thread_num << endl;
    return 0;
}
#endif
