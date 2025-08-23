#ifndef THREADSAFE_QUEUE_H_
#define THREADSAFE_QUEUE_H_

#include <memory>
#include <mutex>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <atomic>
#include <limits>
using namespace std;

template <typename T>
class BoundedThreadsafeQueue
{
public:
    enum class OverflowPolicy { Block, DropNewest, DropOldest, Compact };

private:
    struct Node
    {
        std::shared_ptr<T> data;
        std::unique_ptr<Node> next;
    };

    mutable mutex head_mutex_;
    std::unique_ptr<Node> head_;
    mutable mutex tail_mutex_;
    Node *tail_;
    std::condition_variable data_cond;
    std::condition_variable space_cond;

    uint32_t max_size_;
    std::atomic<uint32_t> size_{0};
    OverflowPolicy policy_;

private:
    Node *GetTail();
    std::unique_ptr<Node> PopHead();
    std::unique_ptr<Node> TryPopHead();
    std::unique_lock<mutex> WaitForData();
    std::unique_ptr<Node> WaitPopHead();

public:
    BoundedThreadsafeQueue(uint32_t max_size = std::numeric_limits<uint32_t>::max(),
                           OverflowPolicy policy = OverflowPolicy::Block)
        : head_(new Node), tail_(head_.get()),
          max_size_(max_size), policy_(policy) {}

    BoundedThreadsafeQueue(const BoundedThreadsafeQueue&) = delete;
    BoundedThreadsafeQueue& operator=(const BoundedThreadsafeQueue&) = delete;

    std::shared_ptr<T> TryPop();
    bool TryPop(T &value);
    std::shared_ptr<T> WaitAndPop();
    bool WaitAndPop(T &value, std::chrono::milliseconds timeout);
    void WaitAndPop(T &value);
    void Push(T new_value);
    bool Empty();
    uint32_t GetSize();
};

#include "threadsafe_queue.cpp" 
#endif // THREADSAFE_QUEUE_H_


