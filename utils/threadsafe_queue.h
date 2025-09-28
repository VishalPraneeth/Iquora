#ifndef THREADSAFE_QUEUE_H_
#define THREADSAFE_QUEUE_H_

#include <memory>
#include <mutex>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <atomic>
#include <deque>
#include <limits>
using namespace std;

// BoundedThreadsafeQueue<T>
// - Single mutex to avoid deadlocks.
// - Supports OverflowPolicy: Block, DropNewest, DropOldest, Compact.
// - Provides Push, WaitAndPop(with timeout), TryPopNonBlocking, Size, Empty.
template <typename T>
class BoundedThreadsafeQueue
{
public:
    enum class OverflowPolicy { Block, DropNewest, DropOldest, Compact };

private:
    mutable std::mutex mu_;
    std::condition_variable data_cond;
    std::condition_variable space_cond;

    size_t max_size_;
    std::atomic<size_t> size_{0};
    OverflowPolicy policy_;
    std::deque<T> queue_;
    std::atomic<bool> stopped_{false};

public:
    explicit BoundedThreadsafeQueue(uint32_t max_size = std::numeric_limits<uint32_t>::max(),
                           OverflowPolicy policy = OverflowPolicy::Block)
        : max_size_(max_size), policy_(policy) {}

    BoundedThreadsafeQueue(const BoundedThreadsafeQueue&) = delete;
    BoundedThreadsafeQueue& operator=(const BoundedThreadsafeQueue&) = delete;

    std::shared_ptr<T> TryPop();
    bool TryPop(T &value);
    std::shared_ptr<T> WaitAndPop();
    bool WaitAndPop(T &value, std::chrono::milliseconds timeout);
    void Push(T new_value);
    bool Empty();
    uint32_t GetSize();

    void Stop() {
        stopped_.store(true);
        data_cond.notify_all();
        space_cond.notify_all();
    }

    bool IsStopped() const { return stopped_.load(); }

};

#include "threadsafe_queue.cpp" 
#endif // THREADSAFE_QUEUE_H_


