#include "threadsafe_queue.h"

template <typename T>
std::shared_ptr<T> BoundedThreadsafeQueue<T>::WaitAndPop()
{
    std::unique_lock<std::mutex> lock(mutex_);
    data_cond_.wait(lock, [this](){ return !queue_.empty(); });
    
    std::shared_ptr<T> result = std::make_shared<T>(std::move(queue_.front()));
    queue_.pop_front();
    space_cond_.notify_one();
    return result;
}

template <typename T>
bool BoundedThreadsafeQueue<T>::WaitAndPop(T &value, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (!data_cond_.wait_for(lock, timeout, [this](){ return !queue_.empty(); })) {
        return false; // Timeout
    }
    
    value = std::move(queue_.front());
    queue_.pop_front();
    space_cond_.notify_one();
    return true;
}

template <typename T>
bool BoundedThreadsafeQueue<T>::TryPop(T &value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return false;
    }
    value = std::move(queue_.front());
    queue_.pop_front();
    space_cond_.notify_one();

    return true;
}

template <typename T>
std::shared_ptr<T> BoundedThreadsafeQueue<T>::TryPop()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.empty()) {
        return std::shared_ptr<T>();
    }
    std::shared_ptr<T> result = std::make_shared<T>(std::move(queue_.front()));
    queue_.pop_front();
    space_cond_.notify_one();

    return result;
}

template <typename T>
void BoundedThreadsafeQueue<T>::Push(T new_value)
{
    std::unique_lock<std::mutex> lock(mutex_);

    // Handle different overflow policies
    if (queue_.size() >= max_size_) {
        switch (policy_) {
            case OverflowPolicy::Block:
                space_cond_.wait(lock, [this](){ return queue_.size() < max_size_; });
                break;
            case OverflowPolicy::DropNewest:
                return; // Simply discard the new value
            case OverflowPolicy::DropOldest:
                queue_.pop_front(); // Remove oldest to make space
                break;
            case OverflowPolicy::Compact:
                // For compacting, need to define equality or key logic at T level.
                // Example: if T has .key field, we could scan & replace.
                while (queue_.size() >= max_size_) {
                    queue_.pop_front();
                }
                break;
        }
    }
    
    queue_.push_back(std::move(new_value));
    data_cond_.notify_one();
}

template <typename T>
bool BoundedThreadsafeQueue<T>::Empty()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

template <typename T>
uint32_t BoundedThreadsafeQueue<T>::GetSize()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}