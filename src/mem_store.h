#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <string>
#include <optional>
using namespace std;

class MemStore {
public:
    bool set(const string& actor_id, const string& key, const string& value);
    optional<string> get(const string& actor_id, const string& key);
private:
    unordered_map<string, unordered_map<string, string>> store_;
    mutable shared_mutex mutex_;
};
