#pragma once
#include <thread>
#include <atomic>
#include "mem_store.h"
#include "wal.h"
using namespace std;

class WriteBehindWorker
{
public:
    WriteBehindWorker(MemStore &store, WAL &wal);
    void start();
    void stop();

private:
    void run();
    thread worker_;
    atomic<bool> running_;
    MemStore &store_;
    WAL &wal_;
};
