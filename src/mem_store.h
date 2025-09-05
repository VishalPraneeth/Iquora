#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <string>
#include <chrono>
#include <optional>
#include <functional>
#include "pubsub.h"
#include <utils/thread_pool.h>
#include <utils/threadsafe_list.h>
#include "wal.h"
#include "write_behind_worker.h"
using namespace std;

class MemStore
{
public:
    using Clock = std::chrono::system_clock;

    struct ValueMetadata {
        std::string value;
        uint64_t version = 0;
        Clock::time_point created_at;
        Clock::time_point last_accessed;
        std::optional<Clock::time_point> expires_at;
    };

    enum class DurabilityMode
    {
        WriteAhead, // Sync WAL before ack
        WriteBehind // Ack first, persist later
    };

    using SubCallback = function<void(const std::string &, const std::string &, const std::string &)>;

    MemStore(std::shared_ptr<WAL> wal = nullptr, 
                std::shared_ptr<ThreadPool<>> thread_pool = nullptr, 
                DurabilityMode mode = DurabilityMode::WriteAhead, 
                size_t write_behind_batch_size = 100);

    bool set(const std::string &actor_id, const std::string &key, const std::string &value, std::optional<int> ttl_secs = std::nullopt);
    std::optional<std::string> get(const std::string &actor_id, const std::string &key);

    bool del(const std::string& actor_id, const std::string& key);
    bool set_if_version(const std::string& actor_id, const std::string& key,
                        const std::string& value, uint64_t expected_version);

    uint64_t subscribe(const std::string &actor_id, SubCallback callback);
    bool unsubscribe(const std::string &actor_id, uint64_t sub_id);
    void cleanup_expired();

private:
    std::unordered_map<std::string, unordered_map<std::string, ValueMetadata>> store_;
    mutable std::shared_mutex mutex_;
    std::shared_ptr<ThreadPool<>> thread_pool_;
    std::shared_ptr<WAL> wal_;
    SubscriptionSystem subscription_system_;
    std::unique_ptr<WriteBehindWorker> write_behind_worker_;
    DurabilityMode durability_mode_;
    ThreadSafeList<std::pair<std::string, std::string>> ttl_index_; // (actor_id, key)

    void notify_subscribers(const std::string &actor_id, const std::string &key, const std::string &value);
    void write_behind_append(const std::string &actor_id, const std::string &key, const std::string &value);
};
