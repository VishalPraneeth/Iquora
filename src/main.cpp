#include "server.h"

int main() {
    // Automatic dependency creation
    auto service = IquoraServiceImpl::Create();
    
    // Or with custom dependencies
    auto memstore = std::make_shared<MemStore>();
    auto wal = std::make_shared<WAL>("custom.wal");
    auto lifecycle = std::make_shared<ActorLifecycle>(memstore);
    auto pool = std::make_shared<ThreadPool<>>(8); // 8 threads
    auto wb = std::make_shared<WriteBehindWorker>(*memstore, *wal);
    
    auto custom_service = IquoraServiceImpl::Create(
        memstore, wal, wb, lifecycle, pool
    );
    
    return 0;
}