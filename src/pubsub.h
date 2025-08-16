#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
using namespace std;

// class PubSub {
// public:
//     using Callback = function<void(const string&, const string&, const string&)>;
//     void subscribe(const string& actor_id, Callback cb);
//     void publish(const string& actor_id, const string& key, const string& value);
// private:
//     unordered_map<string, vector<Callback>> subs_;
// };

#ifndef SUBSCRIPTION_SYSTEM_H
#define SUBSCRIPTION_SYSTEM_H

#include "threadsafe_list.h"
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>

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
        CallbackWrapper(SubID id, SubCallback cb);
    };

    class SubscriptionList {
    public:
        SubID add(SubCallback callback);
        bool remove(SubID id);
        void invoke_all();
        size_t size() const;

    private:
        threadsafe_list<CallbackWrapper> callbacks_;
        std::atomic<SubID> next_id_{1};
    };

    std::unordered_map<std::string, std::unique_ptr<SubscriptionList>> subscriptions_;
    mutable std::mutex mutex_;
};

#endif // SUBSCRIPTION_SYSTEM_H