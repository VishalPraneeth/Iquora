#include "mem_store.h"
#include <mutex>
using namespace std;


bool MemStore::set(const string& actor_id, const string& key, const string& value) {
    unique_lock lock(mutex_);
    store_[actor_id][key] = value;
    return true;
}

optional<string> MemStore::get(const string& actor_id, const string& key) {
    shared_lock lock(mutex_);
    auto it = store_.find(actor_id);
    if (it == store_.end()) return nullopt;
    auto keyIt = it->second.find(key);
    if (keyIt == it->second.end()) return nullopt;
    return keyIt->second;
}
