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
private:
    struct Node
    {
        shared_ptr<T> data;
        unique_ptr<Node> next;
    };

    mutable mutex head_mutex_;
    unique_ptr<Node> head_;
    mutable mutex tail_mutex_;
    Node *tail_;
    condition_variable data_cond;
    uint32_t max_size_;
    atomic<uint32_t> size_{0};

private:
    Node *GetTail();
    unique_ptr<Node> PopHead();
    unique_ptr<Node> TryPopHead();
    unique_lock<mutex> WaitForData();
    unique_ptr<Node> WaitPopHead();

public:
    BoundedThreadsafeQueue() 
        : head_(new Node), tail_(head_.get()),
          max_size_(std::numeric_limits<uint32_t>::max()) {}

    explicit BoundedThreadsafeQueue(uint32_t max_size)
        : head_(new Node), tail_(head_.get()),
          max_size_(max_size) {}

    BoundedThreadsafeQueue(const BoundedThreadsafeQueue&) = delete;

    BoundedThreadsafeQueue& operator=(const BoundedThreadsafeQueue&) = delete;

    shared_ptr<T> TryPop();
    bool TryPop(T &value);
    shared_ptr<T> WaitAndPop();
    void WaitAndPop(T &value);
    void Push(T new_value);
    bool Empty();
    uint32_t GetSize();
};

#include "threadsafe_queue.cpp" 
#endif // THREADSAFE_QUEUE_H_


