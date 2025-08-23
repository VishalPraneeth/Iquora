#pragma once
#include <thread>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include "utils/threadsafe_queue.h"

class MemStore;   // forward declare
class WAL;        // forward declare

class WriteBehindWorker
{
public:
    struct DirtyRecord
    {
        string actor_id;
        string key;
        string value;
    };

    WriteBehindWorker(MemStore &store, WAL &wal, size_t batch_size = 100);
    ~WriteBehindWorker();
    void start();
    void stop();
    void enqueue(const DirtyRecord& record);
    void set_batch_size(size_t size);

private:
    void run();
    void process_batch();

    std::thread worker_;
    std::atomic<bool> running_{false};
    MemStore &store_;
    WAL &wal_;
    BoundedThreadsafeQueue<DirtyRecord> dirty_queue_;
    size_t batch_size_;
    std::mutex batch_mutex_;
    std::vector<DirtyRecord> current_batch_;
};
