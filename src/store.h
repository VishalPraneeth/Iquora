#pragma once
#include <string>
#include <optional>
#include <functional>
#include <cstdint>
#include <vector>

class IStore {
public:
    struct ValueMetadata {
        std::string value;
        uint64_t version;
        uint64_t created_at;    // unix timestamp (ms)
        uint64_t last_accessed; // unix timestamp (ms)
        std::optional<uint64_t> expires_at; // unix timestamp (ms)
    };

    using SubCallback = std::function<void(const std::string&, const std::string&, const std::string&)>;

    virtual ~IStore() = default;

    // Core API
    virtual bool set(const std::string& actor_id, const std::string& key,
                     const std::string& value, std::optional<int> ttl_secs = std::nullopt) = 0;

    virtual std::optional<std::string> get(const std::string& actor_id, const std::string& key) = 0;

    virtual bool del(const std::string& actor_id, const std::string& key) = 0;

    virtual bool set_if_version(const std::string& actor_id, const std::string& key,
                                const std::string& value, uint64_t expected_version) = 0;

    // Pub/Sub
    virtual uint64_t subscribe(const std::string& actor_id, SubCallback callback) = 0;
    virtual bool unsubscribe(const std::string& actor_id, uint64_t sub_id) = 0;

    // Maintenance
    virtual void cleanup_expired() = 0;

    // Persistence-related (optional)
    virtual void snapshot(const std::string& snapshot_path) = 0;
    virtual void recover_from_snapshot(const std::string& snapshot_path) = 0;
};
