#pragma once
#include <string>
#include <mutex>
#include<functional>

class WAL
{
public:
    using EntryHandler = std::function<void(const std::string&, const std::string&, const std::string&)>;

     WAL(const std::string& path = "");
    ~WAL();
    
    void append(const std::string& actor_id, const std::string& key, const std::string& value);
    void set_path(const std::string& path);
    void register_handler(EntryHandler handler);

private:
    EntryHandler handler_;
    std::string path_;
    mutex mutex_;

    void notify_handler(const std::string &actor_id, const std::string &key, const std::string &value);
};
