#include "mem_store.h"
#include <mutex>
#include<string>
#include "utils/thread_pool.h"
#include "thread_pool.h"

MemStore::MemStore(std::shared_ptr<WAL> wal,
                  std::shared_ptr<ThreadPool<>> thread_pool, 
                  DurabilityMode mode, 
                  size_t write_behind_batch_size)
    : wal_(wal ? wal : std::make_shared<WAL>()), durability_mode_(mode), 
      thread_pool_(thread_pool ? thread_pool : std::make_shared<ThreadPool<>>()) {
        if( durability_mode_ == DurabilityMode::WriteBehind) {
            write_behind_worker_ = std::make_unique<WriteBehindWorker>(*this, *wal_, write_behind_batch_size);
            write_behind_worker_->start();
        }
      }

bool MemStore::set(const std::string& actor_id, const std::string& key, const std::string& value, std::optional<int> ttl_secs = std::nullopt) {
    // 1. Update store (synchronous)
    std::unique_lock lock(mutex_);
    auto& entry = store_[actor_id][key];
    entry.value = value;
    entry.version++;
    entry.created_at = Clock::now();
    entry.last_accessed = entry.created_at;
    if (ttl_secs) {
        entry.expires_at = entry.created_at + std::chrono::seconds(*ttl_secs);
        ttl_index_.push_front({actor_id, key});
    }
    // 2. Append to WAL
    if (durability_mode_ == DurabilityMode::WriteAhead) {
        wal_->append(actor_id, key, value);
    } else {
        write_behind_append(actor_id, key, value);
    }
    // 3. Notify all subscribers
    notify_subscribers(actor_id, key, value);
    
    return true;
}

optional<std::string> MemStore::get(const std::string& actor_id, const std::string& key) {
    std::shared_lock lock(mutex_);
    auto it = store_.find(actor_id);
    if (it == store_.end()) return std::nullopt;

    auto keyIt = it->second.find(key);
    if (keyIt == it->second.end()) return std::nullopt;

    auto& meta = keyIt->second;
    meta.last_accessed = Clock::now();

    if (meta.expires_at && Clock::now() > *meta.expires_at) {
        return std::nullopt; // expired
    }

    return meta.value;
}

bool MemStore::del(const std::string& actor_id, const std::string& key) {
    std::unique_lock lock(mutex_);
    auto it = store_.find(actor_id);
    if (it == store_.end()) return false;

    auto erased = it->second.erase(key);
    return erased > 0;
}

bool MemStore::set_if_version(const std::string& actor_id, const std::string& key,
                              const std::string& value, uint64_t expected_version) {
    std::unique_lock lock(mutex_);
    auto& entry = store_[actor_id][key];
    if (entry.version != expected_version) return false;

    entry.value = value;
    entry.version++;
    entry.last_accessed = Clock::now();

    if (durability_mode_ == DurabilityMode::WriteAhead) {
        wal_->append(actor_id, key, value);
    } else if (durability_mode_ == DurabilityMode::WriteBehind) {
        write_behind_append(actor_id, key, value);
    }

    notify_subscribers(actor_id, key, value);
    return true;
}

void MemStore::notify_subscribers(const std::string& actor_id, const std::string& key, const std::string& value){
    auto notify_handler = [key, value](const auto& cb) {
        cb(key, value);
    };
    
    if(thread_pool_){
        thread_pool_->Submit([this, actor_id, notify_handler]() {
            subscription_system_.notify(actor_id, notify_handler);
        });
    } else {
        subscription_system_.notify(actor_id, notify_handler);
    }
}

uint64_t MemStore::subscribe(const std::string& actor_id, SubCallback callback) {
    return subscription_system_.subscribe(actor_id, 
        [callback, actor_id](const std::string& key, const std::string& value) {
            callback(actor_id, key, value);
        });
}

bool MemStore::unsubscribe(const std::string& actor_id, uint64_t sub_id) {
    return subscription_system_.unsubscribe(actor_id, sub_id);
}

void MemStore::write_behind_append(const std::string& actor_id, const std::string& key, const std::string& value) {
    if (write_behind_worker_) {
        WriteBehindWorker::DirtyRecord record{actor_id, key, value};
        write_behind_worker_->enqueue(record);
    }
}

void MemStore::cleanup_expired() {
    ttl_index_.remove_if([this] (auto const &actor_key) {
        const auto& [actor_id, key] = actor_key;
        auto it = store_.find(actor_id);
        if (it == store_.end()) return true;

        auto keyIt = it->second.find(key);
        if (keyIt == it->second.end()) return true;

        auto& meta = keyIt->second;
        if (meta.expires_at && Clock::now() > *meta.expires_at) {
            it->second.erase(key);
            return true;
        }
        return false;
    });
}