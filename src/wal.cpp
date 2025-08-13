#include "wal.h"

WAL::WAL(const std::string& filename) {
    file_.open(filename, std::ios::app);
}

void WAL::append(const std::string& record) {
    std::lock_guard lock(mutex_);
    file_ << record << "\n";
    file_.flush();
}
