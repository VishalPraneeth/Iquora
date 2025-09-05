#pragma once
#include <string>
#include <mutex>
#include<functional>
#include <fstream>
#include <cstdint>
#include <optional>

class WAL
{
public:
    struct Entry {
        uint64_t seq_no;
        std::string actor_id;
        std::string key;
        std::string value;
        uint64_t timestamp;
    };

    using EntryHandler = std::function<void(const Entry&)>;

    explicit WAL(const std::string& path = "wal.log", size_t max_size_bytes = 10 * 1024 * 1024);
    ~WAL();
    
    void append(const std::string& actor_id, const std::string& key, const std::string& value);
    void set_path(const std::string& path);
    void register_handler(EntryHandler handler);

    void replay();
    void rotate();

private:
    EntryHandler handler_;
    std::string path_;
    std::ofstream wal_file_;
    size_t max_size_bytes_;
    uint64_t seq_counter_ = 0;
    mutex mutex_;

    void notify_handler(const Entry& entry);
    void open_log();
};
