#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <thread>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>

class ThreadPool {

    private:

    bool shutdown = false;

    std::vector<std::thread> threads;

    std::mutex task_queue_mutex;
    std::condition_variable cv;

    std::queue<std::function<void()>> taskQueue;

    void Worker_Func();
    
    public:

        ThreadPool(std::function<void()> func, int num_requests, int req_pool_size);
        ~ThreadPool();

        void Terminate_Thread_Pool();

};

#endif