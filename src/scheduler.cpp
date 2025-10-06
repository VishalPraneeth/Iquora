#include "scheduler.h"
#include <utils/thread_pool.h>

Scheduler::Scheduler(std::unique_ptr<ThreadPool<>> poolPtr_) : poolPtr(std::move(poolPtr_)), work_queues(), done(false) {
    // Start actor scheduling loop
    auto func = [this]() { this->ScheduleActors(); };
    poolPtr->Submit(std::move(func));

    // Start timer thread
    timer_thread = std::thread([this]() { this->ScheduleTimers(); });
}

Scheduler::~Scheduler() {
    Shutdown();
}

template <typename ActorType>
void Scheduler::Register(ActorType &actor) {
    if (!done)
    {
        work_queues.push_front(actor.GetQueueRef());
    }
}

template <typename ActorType>
void Scheduler::Deregister(ActorType& actor) {
    work_queues.remove_if([&actor](BoundedThreadsafeQueue<std::unique_ptr<Callable>> *qPtr)
                          { return (qPtr == actor.GetQueueRef()); });
}

void Scheduler::ScheduleActors() {
    while (!done) {
        work_queues.for_each([this](BoundedThreadsafeQueue<std::unique_ptr<Callable>> *qPtr) {
            std::unique_ptr<Callable> workPtr;
            while (qPtr->WaitAndPop(workPtr, std::chrono::milliseconds(50))) {
                if (workPtr) {
                    (*workPtr)();
                }
            }
        });
        // Avoid tight spin if no work
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Scheduler::ScheduleAt(std::chrono::steady_clock::time_point when, std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lock(timer_mutex);
        timer_queue.push(TimedTask{when, std::move(fn), std::chrono::milliseconds(0)});
    }
    timer_cv.notify_one();
}

void Scheduler::ScheduleEvery(std::chrono::milliseconds interval, std::function<void()> fn) {
    auto first_run = std::chrono::steady_clock::now() + interval;
    {
        std::lock_guard<std::mutex> lock(timer_mutex);
        timer_queue.push(TimedTask{first_run, std::move(fn), interval});
    }
    timer_cv.notify_one();
}

void Scheduler::ScheduleTimers() {
    std::unique_lock<std::mutex> lock(timer_mutex);
    while (!done) {
        if (timer_queue.empty()) {
            timer_cv.wait(lock);
        } else {
            auto now = std::chrono::steady_clock::now();
            auto next_task = timer_queue.top();

            if (now >= next_task.run_at) {
                timer_queue.pop();
                lock.unlock();

                poolPtr->Submit(next_task.fn);

                // Reschedule if periodic
                if (next_task.repeat_interval.count() > 0) {
                    auto new_task = next_task;
                    new_task.run_at = now + new_task.repeat_interval;
                    ScheduleAt(new_task.run_at, new_task.fn);
                }

                lock.lock();
            } else {
                timer_cv.wait_until(lock, next_task.run_at);
            }
        }
    }
}

void Scheduler::Shutdown(){
    if (done.exchange(true)) return;

    // Stop timer thread
    {
        std::lock_guard<std::mutex> lock(timer_mutex);
    }
    timer_cv.notify_all();
    if (timer_thread.joinable()) {
        timer_thread.join();
    }

    // Stop actor queues
    work_queues.for_each([](BoundedThreadsafeQueue<std::unique_ptr<Callable>> *qPtr) {
        qPtr->Stop();
    });
}