#include "thread_pool.h"

template <typename Iterable>
ThreadPool<Iterable>::ThreadPool(size_t thread_count) : done(false) {
    try {
        for (size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back(&ThreadPool::WorkerThread, this);
        }
    } catch (...) {
        done = true;
        throw;
    }
}

template <typename Iterable>
ThreadPool<Iterable>::~ThreadPool() {
    Stop();
}

template <typename Iterable>
void ThreadPool<Iterable>::Submit(Callable task) {
    if (!done) {
        work_queue.Push(std::move(task));
    }
}

template <typename Iterable>
void ThreadPool<Iterable>::Stop() {
    done = true;
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

template <typename Iterable>
void ThreadPool<Iterable>::WorkerThread() {
    while (!done) {
        Callable task;
        if (work_queue.TryPop(task)) {
            ++active_workers;
            try {
                task();
            } catch (...) {
                // Log error or handle exception
            }
            --active_workers;
        } else {
            std::this_thread::yield();
        }
    }
}