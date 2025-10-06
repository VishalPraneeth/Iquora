#pragma once
#include <functional>
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>
#include "utils/threadsafe_list.h"

class SubscriptionSystem {
public:
    using SubCallback = std::function<void(const std::string&, const std::string&)>;
    using NotifyHandler = std::function<void(const SubCallback&)>;
    using SubID = uint64_t;

    // Subscription management
    SubID subscribe(const std::string& actor_id, SubCallback callback);
    bool unsubscribe(const std::string& actor_id, SubID id);

    // Notification
    void notify(const std::string& actor_id, NotifyHandler handler);

    // Query
    size_t subscriber_count(const std::string& actor_id) const;

private:
    struct CallbackWrapper {
        SubID id;
        SubCallback callback;
        CallbackWrapper(SubID id, SubCallback cb) : id(id), callback(std::move(cb)) {}
    };

    class SubscriptionList {
    public:
        SubID add(SubCallback callback);
        bool remove(SubID id);
        void invoke_all(NotifyHandler handler);
        size_t size() const;

    private:
        ThreadSafeList<CallbackWrapper> callbacks_;
        std::atomic<SubID> next_id_{1};
    };

    std::unordered_map<std::string, std::unique_ptr<SubscriptionList>> subscriptions_;
    mutable std::mutex mutex_;
};
