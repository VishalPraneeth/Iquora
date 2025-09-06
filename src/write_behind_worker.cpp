#include "write_behind_worker.h"
#include "wal.h"
#include <chrono>
#include <iostream>
using namespace std;

WriteBehindWorker::WriteBehindWorker(MemStore &store, WAL &wal, size_t batch_size)
    : store_(store), wal_(wal), batch_size_(batch_size), dirty_queue_(1000, BoundedThreadsafeQueue<DirtyRecord>::OverflowPolicy::Block) {}

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

        DirtyRecord record;
        if (dirty_queue_.WaitAndPop(record, std::chrono::milliseconds(100)))
        {
            lock_guard<mutex> lock(batch_mutex_);
            current_batch_.push_back(record);

            if (current_batch_.size() >= batch_size_)
            {
                process_batch();
            }
        }
        else
        {
            if (!running_)  break;

            if (!current_batch_.empty())
            {
                process_batch();
            }
        }
    }
}

void WriteBehindWorker::enqueue(const DirtyRecord &record)
{
    dirty_queue_.Push(record);
}

void WriteBehindWorker::set_batch_size(size_t size)
{
    lock_guard<mutex> lock(batch_mutex_);
    batch_size_ = size;
}

void WriteBehindWorker::process_batch()
{
    std::vector<DirtyRecord> batch;
    {
        std::lock_guard<std::mutex> lock(batch_mutex_);
        batch.swap(current_batch_);
    }

    if (!batch.empty())
    {
        try { // Batch write to WAL
            for (const auto &record : batch)
            {
                wal_.append(record.actor_id, record.key, record.value);
            }

            // Optional: Batch persist to disk/database
            std::cout << "[WriteBehind] Processed batch of " << batch.size() << " records\n";
        } catch (const std::exception& ex){
            std::cerr << "[WriteBehind] Error while flushing batch: " << ex.what() << "\n";
            // TODO: retry logic or dead-letter queue
        }
    }
}