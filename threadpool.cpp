#include"threadpool.h"
#include"mysql_db.h"

ThreadPool::ThreadPool(size_t numThreads ) : stop(false){
   for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this, i] {
            // 每个工作线程初始化独立的 MySQL 连接，失败则重试
            int retry = 0;
            while (!InitThreadMySQL() && retry < 3) {
                fprintf(stderr, "[WARN] Thread %zu MySQL init failed, retry %d...\n", i, retry + 1);
                retry++;
            }
            if (retry >= 3) {
                fprintf(stderr, "[ERROR] Thread %zu MySQL init failed after 3 retries\n", i);
                return;  // 该线程退出
            }
            while(true){
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    condition.wait(lock,[this]{return stop||!tasks.empty();});
                    if(stop&& tasks.empty())
                    break;
                    task=std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
            DestroyThreadMySQL();
        });
        
    }
}
ThreadPool ::~ThreadPool(){
    {std::lock_guard<std::mutex> lock(queue_mutex);
    stop=true;
    }
    condition.notify_all();
    for(std::thread &worker: workers){
    if(worker.joinable())
        {
            worker.join();
        }
    }

}

void ThreadPool::Enqueue(std::function<void()> task){
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                tasks.emplace(std::move(task));
            }
            condition.notify_one();
        }