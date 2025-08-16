#pragma once
#include <thread>
#include <atomic>
#include "mem_store.h"
#include "wal.h"
#include <utils/threadsafe_queue.h>
using namespace std;

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

    thread worker_;
    atomic<bool> running_{false};
    MemStore &store_;
    WAL &wal_;
    BoundedThreadsafeQueue<DirtyRecord> dirty_queue_;
    size_t batch_size_;
    mutex batch_mutex_;
    vector<DirtyRecord> current_batch_;
};
