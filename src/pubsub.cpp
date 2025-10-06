#include "pubsub.h"

SubscriptionSystem::SubID SubscriptionSystem::SubscriptionList::add(SubscriptionSystem::SubCallback callback) {
    SubscriptionSystem::SubID id = next_id_.fetch_add(1);
    callbacks_.push_front(CallbackWrapper(id, std::move(callback)));
    return id;
}

bool SubscriptionSystem::SubscriptionList::remove(SubID id) {
    return callbacks_.remove_first_if([id](const CallbackWrapper &wrapper) {
        return wrapper.id == id;
    });
}

void SubscriptionSystem::SubscriptionList::invoke_all(NotifyHandler handler) {
    callbacks_.for_each([&handler](const CallbackWrapper &wrapper) {
        if (wrapper.callback) {
            handler(wrapper.callback);
        }
    });
}

size_t SubscriptionSystem::SubscriptionList::size() const {
    return callbacks_.size();
}

SubscriptionSystem::SubID SubscriptionSystem::subscribe(const std::string &actor_id, SubCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto &list = subscriptions_[actor_id];
    if (!list) {
        list = std::make_unique<SubscriptionList>();
    }
    return list->add(std::move(callback));
}

bool SubscriptionSystem::unsubscribe(const std::string &actor_id, SubID id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscriptions_.find(actor_id);
    if (it != subscriptions_.end()) {
        bool removed = it->second->remove(id);
        if (removed && it->second->size() == 0) {
            subscriptions_.erase(it);
        }
        return removed;
    }
    return false;
}

void SubscriptionSystem::notify(const std::string &actor_id, NotifyHandler handler) {
    SubscriptionList* list_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.find(actor_id);
        if (it == subscriptions_.end()) return;
        list_ptr = it->second.get();
    }

    if(list_ptr){
        list_ptr->invoke_all(handler);
    }
}

size_t SubscriptionSystem::subscriber_count(const std::string &actor_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscriptions_.find(actor_id);
    return it != subscriptions_.end() ? it->second->size() : 0;
}
