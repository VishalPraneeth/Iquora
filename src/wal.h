#pragma once
#include <string>
#include <mutex>
#include<functional>
using namespace std;

class WAL
{
public:
    using EntryHandler = function<void(const string&, const string&, const string&)>;

     WAL(const string& path = "");
    ~WAL();
    
    void append(const string& actor_id, const string& key, const string& value);
    void set_path(const string& path);
    void register_handler(EntryHandler handler);

private:
    EntryHandler handler_;
    string path_;
    mutex mutex_;

    void notify_handler(const string &actor_id, const string &key, const string &value);
};
