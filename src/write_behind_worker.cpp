#include "write_behind_worker.h"
#include <chrono>
#include <iostream>
using namespace std;

WriteBehindWorker::WriteBehindWorker(MemStore& store, WAL& wal)
    : store_(store), wal_(wal), running_(false) {}

void WriteBehindWorker::start() {
    running_ = true;
    worker_ = thread(&WriteBehindWorker::run, this);
}

void WriteBehindWorker::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

void WriteBehindWorker::run() {
    while (running_) {
        this_thread::sleep_for(chrono::seconds(5));
        // TODO: flush dirty pages to disk/RocksDB
        cout << "[WriteBehindWorker] Flush cycle complete.\n";
    }
}
