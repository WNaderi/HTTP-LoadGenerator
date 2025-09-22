#include <queue>
#include <thread>
#include <functional>
#include "ThreadPool.hpp"

void ThreadPool::Worker_Func() {

    std::function<void()> task;

    while (true) {

        {

            // Wait until thread is released by master thread.
            std::unique_lock<std::mutex> lock(task_queue_mutex);
            cv.wait(lock, [this]{return shutdown || !taskQueue.empty();});
            if (shutdown && taskQueue.empty())
                return;
            // Execute task
            task = taskQueue.front();
            taskQueue.pop();

        }

        task();

        std::unique_lock<std::mutex> lock(task_queue_mutex);
        if (taskQueue.empty())
            cv.notify_all();

    }

}

ThreadPool::ThreadPool(std::function<void()> task, int num_requests, int req_pool_size) {

    // Create threads for the pool;
    for (int i = 0; i < req_pool_size; i++)
        threads.emplace_back(&ThreadPool::Worker_Func, this);

    // Create task queue that need to be executed by threads
    std::unique_lock<std::mutex> lock(task_queue_mutex);
    for (int i = 0; i < num_requests; i++) {
        taskQueue.push(task);
        cv.notify_one(); // wake up a thread to service task
    }

}

// "Master" thread. simple wrap things up.
void ThreadPool::Terminate_Thread_Pool() {

    std::unique_lock<std::mutex> lock(task_queue_mutex);
    cv.wait(lock, [this]{return taskQueue.empty();});
    // Check if tasks are complete
    if (taskQueue.empty())
        shutdown = true;
    cv.notify_all(); 

}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(task_queue_mutex);
        shutdown = true;
    }
    cv.notify_all();
    for (auto& t : threads) {
        if (t.joinable())
            t.join();
    }
}


