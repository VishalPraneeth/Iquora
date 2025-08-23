#include "threadsafe_queue.h"

template <typename T>
typename BoundedThreadsafeQueue<T>::Node *BoundedThreadsafeQueue<T>::GetTail()
{
    std::lock_guard<std::mutex> tail_lock{tail_mutex_};
    return tail_;
}

template <typename T>
std::unique_ptr<typename BoundedThreadsafeQueue<T>::Node> BoundedThreadsafeQueue<T>::PopHead()
{
    std::unique_ptr<Node> old_head = std::move(head_);
    head_ = std::move(old_head->next);
    return old_head;
}

template <typename T>
std::unique_ptr<typename BoundedThreadsafeQueue<T>::Node> BoundedThreadsafeQueue<T>::TryPopHead()
{
    std::lock_guard<std::mutex> head_lock(head_mutex_);
    if (head_.get() == GetTail())
    {
        return std::unique_ptr<Node>();
    }
    size_.fetch_sub(1);
    space_cond.notify_one();
    return PopHead();
}

template <typename T>
std::unique_lock<std::mutex> BoundedThreadsafeQueue<T>::WaitForData()
{
    std::unique_lock<std::mutex> head_lock(head_mutex_);
    data_cond.wait(head_lock, [&]
                   { return head_.get() != GetTail(); });
    return std::move(head_lock);
}

template <typename T>
std::unique_ptr<typename BoundedThreadsafeQueue<T>::Node> BoundedThreadsafeQueue<T>::WaitPopHead()
{
    std::unique_lock<std::mutex> head_lock(WaitForData());
    size_.fetch_sub(1);
    space_cond.notify_one();
    return PopHead();
}

template <typename T>
std::shared_ptr<T> BoundedThreadsafeQueue<T>::WaitAndPop()
{
    std::unique_ptr<Node> const old_head = WaitPopHead();
    return old_head->data;
}

template <typename T>
bool BoundedThreadsafeQueue<T>::WaitAndPop(T &value, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> head_lock(head_mutex_);
    if (!data_cond.wait_for(head_lock, timeout, [&] { return head_.get() != GetTail(); }))
        return false; // timeout

    std::unique_ptr<Node> old_head = PopHead();
    value = std::move(*old_head->data);
    size_.fetch_sub(1);
    space_cond.notify_one();
    return true;
}

template <typename T>
bool BoundedThreadsafeQueue<T>::TryPop(T &value)
{
    std::unique_ptr<Node> old_head = TryPopHead();
    if (old_head)
    {
        value = std::move(*old_head->data);
        return true;
    }
    return false;
}

template <typename T>
std::shared_ptr<T> BoundedThreadsafeQueue<T>::TryPop()
{
    std::unique_ptr<Node> old_head = TryPopHead();
    return old_head ? old_head->data : std::shared_ptr<T>();
}

template <typename T>
void BoundedThreadsafeQueue<T>::Push(T new_value)
{
    std::shared_ptr<T> new_data(
        std::make_shared<T>(std::move(new_value)));
    std::unique_ptr<Node> p(new Node);
    Node *const new_tail = p.get();

    if (policy_ == OverflowPolicy::Block) {
        std::unique_lock<std::mutex> tail_lock(tail_mutex_);
        space_cond.wait(tail_lock, [&] { return size_.load() < max_size_; });
        tail_->data = new_data;
        tail_->next = std::move(p);
        tail_ = new_tail;
        size_.fetch_add(1);
        data_cond.notify_one();
    } 
    else if (policy_ == OverflowPolicy::DropNewest) {
        if (size_.load() >= max_size_) {
            return; // drop incoming
        }
        std::lock_guard<std::mutex> tail_lock(tail_mutex_);
        tail_->data = new_data;
        tail_->next = std::move(p);
        tail_ = new_tail;
        size_.fetch_add(1);
        data_cond.notify_one();
    } 
    else if (policy_ == OverflowPolicy::DropOldest) {
        std::lock_guard<std::mutex> tail_lock(tail_mutex_);
        if (size_.load() >= max_size_) {
            // remove head
            std::lock_guard<std::mutex> head_lock(head_mutex_);
            PopHead();
            size_.fetch_sub(1);
        }
        tail_->data = new_data;
        tail_->next = std::move(p);
        tail_ = new_tail;
        size_.fetch_add(1);
        data_cond.notify_one();
    } 
    else if (policy_ == OverflowPolicy::Compact) {
        // For compacting, need to define equality or key logic at T level.
        // Example: if T has .key field, we could scan & replace.
        if (size_.load() >= max_size_) {
            return;
        }
        std::lock_guard<std::mutex> tail_lock(tail_mutex_);
        tail_->data = new_data;
        tail_->next = std::move(p);
        tail_ = new_tail;
        size_.fetch_add(1);
        data_cond.notify_one();
    }
}

template <typename T>
bool BoundedThreadsafeQueue<T>::Empty()
{
    std::lock_guard<std::mutex> lk{head_mutex_};
    return head_.get() == GetTail();
}

template <typename T>
uint32_t BoundedThreadsafeQueue<T>::GetSize()
{
    return size_.load(std::memory_order_relaxed);
}