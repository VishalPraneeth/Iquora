#include "pubsub.h"
#include "threadsafe_list.h"
#include <unordered_map>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

class SubscriptionSystem
{
public:
    using SubCallback = std::function<void(const std::string&, const std::string&)>;
    using NotifyHandler = std::function<void(const SubCallback &)>;
    using SubID = uint64_t;

private:
    struct CallbackWrapper
    {
        SubID id;
        SubCallback callback;

        SubscriptionSystem::CallbackWrapper::CallbackWrapper(SubID id, SubCallback cb) : id(id), callback(std::move(cb)) {}
    };

    class SubscriptionList
    {
        threadsafe_list<CallbackWrapper> callbacks_;
        std::atomic<SubID> next_id_{1};

    public:
        SubscriptionSystem::SubID SubscriptionSystem::SubscriptionList::add(SubCallback callback)
        {
            SubID id = next_id_.fetch_add(1);
            callbacks_.push_front(CallbackWrapper(id, std::move(callback)));
            return id;
        }

        bool SubscriptionSystem::SubscriptionList::remove(SubID id)
        {
            return callbacks_.remove_first_if([id](const CallbackWrapper &wrapper)
                                              { return wrapper.id == id; });
        }

        void SubscriptionSystem::SubscriptionList::invoke_all(SubscriptionSystem::NotifyHandler handler)
        {
            callbacks_.for_each([&handler](const CallbackWrapper &wrapper){
                if (wrapper.callback) {
                    handler(wrapper.callback);
                } 
            });
        }

        size_t SubscriptionSystem::SubscriptionList::size() const
        {
            return callbacks_.size();
        }
    };

    std::unordered_map<std::string, std::unique_ptr<SubscriptionList>> subscriptions_;
    mutable std::mutex mutex_;

public:
    SubID SubscriptionSystem::subscribe(const std::string &actor_id, SubCallback callback)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto &list = subscriptions_[actor_id];
        if (!list)
        {
            list = std::make_unique<SubscriptionList>();
        }
        return list->add(std::move(callback));
    }

    bool SubscriptionSystem::unsubscribe(const std::string &actor_id, SubID id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.find(actor_id);
        if (it != subscriptions_.end())
        {
            bool removed = it->second->remove(id);
            if (removed && it->second->size() == 0)
            {
                subscriptions_.erase(it);
            }
            return removed;
        }
        return false;
    }

    void SubscriptionSystem::notify(const std::string &actor_id, NotifyHandler handler)
    {
        std::unique_ptr<SubscriptionList> list_copy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = subscriptions_.find(actor_id);
            if (it == subscriptions_.end())
                return;
            list_copy = std::make_unique<SubscriptionList>(*it->second);
        }

        list_copy->invoke_all(handler);
    }

    size_t SubscriptionSystem::subscriber_count(const std::string &actor_id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.find(actor_id);
        return it != subscriptions_.end() ? it->second->size() : 0;
    }
};