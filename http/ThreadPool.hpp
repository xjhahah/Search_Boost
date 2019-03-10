#pragma  once

#include <iostream>
#include <queue>
#include <pthread.h>
#include "Log.hpp"

typedef void (*Handler_t)(int sock);

class Task{
    public:
        int sock;
        Handler_t TaskHandler;
    public:
        void InitTask(Handler_t handler_, int sock_)
        {
            TaskHandler = handler_;
            sock = sock_;
        }
};

class ThreadPool{
    private:
        int threads_max_num;
        int threads_idle_num;
        int threads_curr_num;
        bool is_quit;
        std::queue<Task> task_queue;
        pthread_mutex_t lock;
        pthread_cond_t cond;

    private:
        void LockPool()
        {
            pthread_mutex_lock(&lock);
        }
        void UnlockPool()
        {
            pthread_mutex_unlock(&lock);
        }
        void ThreadWait()
        {
            pthread_cond_wait(&cond, &lock);
        }
        int ThreadTimeWait()
        {
            timespec t;
            clock_gettime(CLOCK_REALTIME, &t);
            t.tv_sec += 3;
            return pthread_cond_timedwait(&cond, &lock, &t);
        }
        void NotifyOneThread()
        {
            pthread_cond_signal(&cond);
        }
        void NotifyAllThreads()
        {
            pthread_cond_broadcast(&cond);
        }
        void PushTask(const Task &t)
        {
            if(is_quit){
                return; //if thread pool is begin quit, no new task add
            }
            task_queue.push(t);
        }
        int PopTask(Task &t)
        {
            if(task_queue.empty()){
                return -1;
            }
            t = task_queue.front();
            task_queue.pop();
            return 0;
        }
        int ThreadsIdleNum()
        {
            return threads_idle_num;
        }
        void ThreadsIdleNumInc()
        {
            threads_idle_num++;
        }
        void ThreadsIdleNumDec()
        {
            threads_idle_num--;
        }
        bool IsThreadPoolFull()
        {
            return threads_curr_num == threads_max_num ? true : false;
        }
        bool IsTaskQueueEmpty()
        {
            return task_queue.empty();
        }
        void ThreadIdleRun(bool &is_timeout_){
            ThreadsIdleNumInc();
            while(IsTaskQueueEmpty() && !is_quit){
                if(ThreadTimeWait() == ETIMEDOUT){//该线程超时
                    is_timeout_ = true;
                    break;
                }
            }
            ThreadsIdleNumDec();
        }
        int ThreadStatusCheck(bool &is_timeout_)
        {
            int status = 0;
            if(!IsTaskQueueEmpty()){
                status = 0;
            }else if(is_timeout_){ //该线程状态会一直保存
                threads_curr_num--;
                status = 1;
            }else if(is_quit){
                threads_curr_num--;
                status = 2;
            }else{
            }
            return status;
        }
    public:
        ThreadPool(int threads_max_num_)
        {
            threads_max_num = threads_max_num_;
            threads_idle_num = 0;
            threads_curr_num = 0;
            is_quit = false;

            pthread_mutex_init(&lock, NULL);
            pthread_cond_init(&cond, NULL);
        }
        static void *ThreadRoutine(void* arg)
        {
            ThreadPool *pool_p = (ThreadPool*)arg;
            bool is_timeout = false;
            Task t;

            pthread_detach(pthread_self());
            while(true){
                pool_p->LockPool();
                pool_p->ThreadIdleRun(is_timeout); //!task_queue.empty>is_quit>timeout
                int status = pool_p->ThreadStatusCheck(is_timeout);
                switch(status){
                    case 0:
                        {
                            pool_p->PopTask(t);
                            pool_p->UnlockPool();
                            t.TaskHandler(t.sock);
                        }
                        break;
                    case 1:
                    case 2:
                        {
                            pool_p->UnlockPool();
                            std::cout << "thread quit : " << pthread_self()<< std::endl;
                            pthread_exit((void*)0);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
        void AddTask(const Task &t)
        {
            LockPool();
            pthread_t tid;
            PushTask(t);
            if(ThreadsIdleNum() > 0){
                NotifyOneThread();
            }
            else if(!IsThreadPoolFull()){
                pthread_create(&tid, NULL, ThreadRoutine, (void *)this);
                threads_curr_num++;
            }else{
                //Do Nothing
            }
            UnlockPool();
        }
        void StopAll()
        {
            LockPool();
            if(is_quit){
                UnlockPool();
                return;
            }
            is_quit = true;
            if(threads_curr_num > 0){
                if(threads_idle_num > 0){
                    NotifyAllThreads();
                }
            }else{
                std::cout << "have no work thread" << std::endl;
            }

            UnlockPool();
        }

        ~ThreadPool()
        {
            pthread_mutex_destroy(&lock);
            pthread_cond_destroy(&cond);
            std::cout << "pool destroy" << std::endl;
        }
};
