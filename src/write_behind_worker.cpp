#include "write_behind_worker.h"
#include <chrono>
#include <iostream>
using namespace std;

WriteBehindWorker::WriteBehindWorker(MemStore &store, WAL &wal, size_t batch_size)
    : store_(store), wal_(wal), batch_size_(batch_size), dirty_queue_(1000) {}

WriteBehindWorker::~WriteBehindWorker()
{
    stop();
}

void WriteBehindWorker::start()
{
    if (!running_)
    {
        running_ = true;
        worker_ = thread(&WriteBehindWorker::run, this);
    }
}

void WriteBehindWorker::stop()
{
    running_ = false;

    if (worker_.joinable())
    {
        worker_.join();
    }
    // Flush the remaining records if any
    process_batch();
}

void WriteBehindWorker::run()
{
    while (running_)
    {   
        try{
            auto record = dirty_queue_.WaitAndPop();
            {
                lock_guard<mutex> lock(batch_mutex_);
                current_batch_.push_back(*record);

                if (current_batch_.size() >= batch_size_) {
                    process_batch();
                }
            }
        }catch (...) {
            // Handle queue errors
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void WriteBehindWorker::enqueue(const DirtyRecord& record){
    try {
        dirty_queue_.Push(record);
    } catch (const std::length_error& e) {
        std::cerr << "WriteBehindWorker queue full! Forcing batch process\n";
        process_batch();
        dirty_queue_.Push(record); // Retry
    }
}

void WriteBehindWorker::set_batch_size(size_t size){
    lock_guard<mutex> lock(batch_mutex_);
    batch_size_=size;
}

void WriteBehindWorker::process_batch() {
    std::vector<DirtyRecord> batch;
    {
        std::lock_guard<std::mutex> lock(batch_mutex_);
        batch.swap(current_batch_);
    }

    if (!batch.empty()) {
        // Batch write to WAL
        for (const auto& record : batch) {
            wal_.append( record.actor_id, record.key, record.value);
        }
        
        // Optional: Batch persist to disk/database
        std::cout << "[WriteBehind] Processed batch of " << batch.size() << " records\n";
    }
}