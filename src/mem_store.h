#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <string>
#include <optional>
#include <functional>
#include "pubsub.h"
#include <utils/thread_pool.h>
#include "wal.h"
#include "write_behind_worker.h"
using namespace std;

class MemStore
{
public:
    enum class DurabilityMode
    {
        WriteAhead, // Sync WAL before ack
        WriteBehind // Ack first, persist later
    };

    using SubCallback = function<void(const string &, const string &, const string &)>;

    MemStore(shared_ptr<WAL> wal = nullptr, shared_ptr<ThreadPool<>> thread_pool = nullptr, DurabilityMode mode = DurabilityMode::WriteAhead, size_t write_behind_batch_size = 100);

    bool set(const string &actor_id, const string &key, const string &value);
    optional<string> get(const string &actor_id, const string &key);

    uint64_t subscribe(const string &actor_id, SubCallback callback);
    bool unsubscribe(const string &actor_id, uint64_t sub_id);

private:
    unordered_map<string, unordered_map<string, string>> store_;
    mutable shared_mutex mutex_;
    shared_ptr<ThreadPool<>> thread_pool_;
    shared_ptr<WAL> wal_;
    SubscriptionSystem subscription_system_;
    unique_ptr<WriteBehindWorker> write_behind_worker_;
    DurabilityMode durability_mode_;

    void notify_subscribers(const string &actor_id, const string &key, const string &value);
    void write_behind_append(const string &actor_id, const string &key, const string &value);
};
