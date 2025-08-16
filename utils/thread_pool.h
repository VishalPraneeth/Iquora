#pragma once
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include "threadsafe_queue.h"

template <typename Iterable = std::vector<std::function<void()>>>
class ThreadPool {
public:
    using Callable = std::function<void()>;
    
    ThreadPool(size_t thread_count = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    void Submit(Callable task);
    void Stop();
    
private:
    std::atomic<bool> done;
    BoundedThreadsafeQueue<Callable> work_queue;
    std::vector<std::thread> threads;
    std::atomic<size_t> active_workers{0};
    
    void WorkerThread();
};

// Include implementation for template class
#include "thread_pool.inl"