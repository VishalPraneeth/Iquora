#include "threadsafe_queue.h"

template <typename T>
std::shared_ptr<T> BoundedThreadsafeQueue<T>::WaitAndPop()
{
    std::unique_lock<std::mutex> lock(mu_);
    data_cond.wait(lock, [this]() { return stopped_ || !queue_.empty(); });
    if (stopped_ && queue_.empty()) {
        return std::shared_ptr<T>();
    }
    std::shared_ptr<T> res(std::make_shared<T>(std::move(queue_.front())));
    queue_.pop_front();
    size_--;
    space_cond.notify_one();
    return res;
}

template <typename T>
bool BoundedThreadsafeQueue<T>::WaitAndPop(T &value, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mu_);
    if (timeout.count() > 0) {
        if (!data_cond.wait_for(lock, timeout, [this]() { return stopped_ || !queue_.empty(); })) {
            return false; // Timeout
        }
    } else {
        data_cond.wait(lock, [this]() { return stopped_ || !queue_.empty(); });
    }
    
    if (stopped_ && queue_.empty()) {
        return false;
    }
    value = std::move(queue_.front());
    queue_.pop_front();
    size_--;
    space_cond.notify_one();
    return true;
}

template <typename T>
bool BoundedThreadsafeQueue<T>::TryPop(T &value)
{
    std::lock_guard<std::mutex> lock(mu_);
    if (queue_.empty()) {
        return false;
    }
    value = std::move(queue_.front());
    queue_.pop_front();
    size_--;
    space_cond.notify_one();

    return true;
}

template <typename T>
std::shared_ptr<T> BoundedThreadsafeQueue<T>::TryPop()
{
    std::lock_guard<std::mutex> lock(mu_);
    if (queue_.empty()) {
        return std::shared_ptr<T>();
    }
    std::shared_ptr<T> res(std::make_shared<T>(std::move(queue_.front())));
    queue_.pop_front();
    size_--;
    space_cond.notify_one();
    return res;
}

template <typename T>
void BoundedThreadsafeQueue<T>::Push(T new_value)
{
    std::unique_lock<std::mutex> lock(mu_);

    if (stopped_) {
        return;
    }
    // Handle different overflow policies
    if (queue_.size() >= max_size_) {
        switch (policy_) {
            case OverflowPolicy::Block:
                space_cond.wait(lock, [this](){ return stopped_ || queue_.size() < max_size_; });
                if (stopped_) return;
                break;
            case OverflowPolicy::DropNewest:
                return; // Simply discard the new value
            case OverflowPolicy::DropOldest:
                queue_.pop_front(); // Remove oldest to make space
                size_--;    
                break;
            case OverflowPolicy::Compact:
                // For compacting, need to define equality or key logic at T level.
                // Example: if T has .key field, we could scan & replace.
                while (queue_.size() >= max_size_) {
                    queue_.pop_front();
                    size_--;
                }
                break;
        }
    }
    
    queue_.push_back(std::move(new_value));
    size_++;
    data_cond.notify_one();
}

template <typename T>
bool BoundedThreadsafeQueue<T>::Empty() const {
    std::lock_guard<std::mutex> lock(mu_);
    return queue_.empty();
}

template <typename T>
uint32_t BoundedThreadsafeQueue<T>::GetSize() const {
    std::lock_guard<std::mutex> lock(mu_);
    return queue_.size();
}