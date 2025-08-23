#include "mem_store.h"
#include <mutex>
#include<string>
#include "utils/thread_pool.h"
#include "thread_pool.h"

MemStore::MemStore(shared_ptr<WAL> wal,
                  shared_ptr<ThreadPool<>> thread_pool, DurabilityMode mode, size_t write_behind_batch_size)
    : wal_(wal ? wal : make_shared<WAL>()), durability_mode_(mode), 
      thread_pool_(thread_pool ? thread_pool : make_shared<ThreadPool<>>()) {
        if( durability_mode_ == DurabilityMode::WriteBehind) {
            write_behind_worker_ = std::make_unique<WriteBehindWorker>(*this, *wal_, write_behind_batch_size);
            write_behind_worker_->start();
        }
      }

bool MemStore::set(const std::string& actor_id, const std::string& key, const std::string& value) {
    // 1. Update store (synchronous)
    unique_lock lock(mutex_);
    store_[actor_id][key] = value;
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
    shared_lock lock(mutex_);
    auto it = store_.find(actor_id);
    if (it == store_.end()) return nullopt;

    auto keyIt = it->second.find(key);
    if (keyIt == it->second.end()) return nullopt;

    return keyIt->second;
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