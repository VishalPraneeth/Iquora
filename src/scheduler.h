#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <utils/threadsafe_list.h>
#include <utils/thread_pool.h>
#include <utils/callable.h>
#include <abstract_actor.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <atomic>

/*
Scheduler responsibilities:
1. Dispatch actor tasks from registered work queues into the thread pool.
2. Manage timed tasks (one-off and periodic) via priority queue.
3. Unified shutdown of both actor and timer scheduling.
*/

class Scheduler
{
public:
    explicit Scheduler(std::unique_ptr<ThreadPool<>> poolPtr_);
    ~Scheduler();

    template <typename ActorType>
    void Register(ActorType &actor); // The compiler should be able to deduce the actor type. Take a reference to the actors queue here.

    template <typename ActorType>
    void Deregister(ActorType &actor);

    // Schedule a one-off task at a specific time point
    void ScheduleAt(std::chrono::steady_clock::time_point when, std::function<void()> fn);

    // Schedule a repeated task every interval, starting at now + interval
    void ScheduleEvery(std::chrono::milliseconds interval, std::function<void()> fn);

    void Shutdown();

private:
    struct TimedTask {
        std::chrono::steady_clock::time_point run_at;
        std::function<void()> fn;
        std::chrono::milliseconds repeat_interval{0};

        bool operator>(const TimedTask &other) const {
            return run_at > other.run_at;
        }
    };


    void ScheduleActors();
    void ScheduleTimers();

    ThreadSafeList<BoundedThreadsafeQueue<std::unique_ptr<Callable>> *> work_queues;
    std::unique_ptr<ThreadPool<>> poolPtr;
    std::atomic<bool> done;

    // Timed tasks
    std::priority_queue<TimedTask, std::vector<TimedTask>, std::greater<TimedTask>> timer_queue;
    std::mutex timer_mutex;
    std::condition_variable timer_cv;
    std::thread timer_thread;
};


#endif // SCHEDULER_H_