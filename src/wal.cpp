#include "wal.h"
#include<chrono>
#include<string>
#include<iomanip>
#include<fstream>
#include<sstream>
#include<filesystem>
#include <stdexcept>
using std::ios; 

WAL::WAL(const std::string& path, size_t max_size_bytes) 
        : path_(path), max_size_bytes_(max_size_bytes) {
            open_log();
        }

WAL::~WAL() {
    if (wal_file_.is_open()) {
        wal_file_.flush();
        wal_file_.close();
    }
}

void WAL::open_log() {
    wal_file_.open(path_, ios::app);
    if (!wal_file_) {
        throw std::runtime_error("Failed to open WAL file: " + path_);
    }
}

void WAL::append(const std::string& actor_id, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    Entry entry{++seq_counter_, actor_id, key, value, timestamp};

    if (wal_file_) {
        wal_file_ << entry.seq_no << "|" << entry.timestamp << "|" 
                  << actor_id << "|" << key << "|" << value << "\n";
        wal_file_.flush();
    }

    // Rotate if too large
    if (std::filesystem::file_size(path_) >= max_size_bytes_) {
        rotate();
    }
    
    notify_handler(entry);
    
}

void WAL::set_path(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = path;
    open_log();
}

void WAL::register_handler(EntryHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handler_ = move(handler);
}

void WAL::rotate() {
    wal_file_.close();
    std::string rotated = path_ + ".1";
    std::filesystem::rename(path_, rotated);
    open_log();
}

void WAL::notify_handler(const Entry& entry) {
    if (handler_) {
        handler_(entry);
    }
}

void WAL::replay() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream infile(path_);
    std::string line;
    while (getline(infile, line)) {
        std::istringstream iss(line);
        std::string seq_str, ts_str, actor_id, key, value;
        if (getline(iss, seq_str, '|') &&
            getline(iss, ts_str, '|') &&
            getline(iss, actor_id, '|') &&
            getline(iss, key, '|') &&
            getline(iss, value)) {
            Entry e{stoull(seq_str), actor_id, key, value, stoull(ts_str)};
            notify_handler(e);
        }
    }
}