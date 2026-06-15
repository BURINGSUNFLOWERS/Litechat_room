#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<functional>

class ThreadPool
{
    public:
        explicit ThreadPool(size_t numThreads);
        ~ThreadPool();

        void Enqueue(std::function<void()>  task);

    private:
        std::vector<std::thread>workers;
        std::queue<std::function<void()> >tasks;
        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop;
};

#endif