#include "wal.h"
#include<chrono>
#include<iomanip>
#include<fstream>
using namespace std;

WAL::WAL(const string& path) : path_(path) {}

WAL::~WAL() {
    // Flush any pending writes if needed
}

void WAL::append(const string& actor_id, const string& key, const string& value) {
    lock_guard<mutex> lock(mutex_);

    if(!path_.empty()){
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::ofstream wal(path_, std::ios::app);
        if (wal) {
            wal << std::put_time(std::localtime(&time), "%F %T") << " | "
                << actor_id << " | " << key << " | " << value << "\n";
        }
        notify_handler(actor_id, key, value);
    }
    
}

void WAL::set_path(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = path;
}

void WAL::register_handler(EntryHandler handler) {
    lock_guard<mutex> lock(mutex_);
    handler_ = move(handler);
}


void WAL::notify_handler(const string& actor_id, const string& key, const string& value) {
    if (handler_) {
        handler_(actor_id, key, value);
    }
}