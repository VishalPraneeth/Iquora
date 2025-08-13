#include "thread_pool.h"
using namespace std;

template <typename Iterable>
ThreadPool<Iterable>::ThreadPool() : done(false), joiner(threads)
{
    unsigned const thread_count = thread::hardware_concurrency();
    try
    {
        for (auto i = 0; i < thread_count; ++i)
        {
            threads.push_back(thread(&ThreadPool::WorkerThread, this));
        }
    }
    catch (const exception &e)
    {
        cerr << e.what() << '\n';
    }
}

template <typename Iterable>
ThreadPool<Iterable>::~ThreadPool()
{
    done = true;
}

template <typename Iterable>
void ThreadPool<Iterable>::Submit(Callable c)
{
    work_queue.Push(move(c));
}

template <typename Iterable>
void ThreadPool<Iterable>::WorkerThread()
{
    while (!done)
    {
        Callable task;
        if (work_queue.TryPop(task))
        {
            task();
        }
        else
        {
            this_thread::yield();
        }
    }
}