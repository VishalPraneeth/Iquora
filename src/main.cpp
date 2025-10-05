#include <grpcpp/grpcpp.h>
#include "proto/iquora.grpc.pb.h"

#include "server.h"
#include "mem_store.h"
#include "wal.h"
#include "write_behind_worker.h"
#include "actor_lifecycle.h"
#include "utils/thread_pool.h"

#include <iostream>
#include <memory>
#include <string>

using grpc::Server;
using grpc::ServerBuilder;


int main(int argc, char** argv) {
    std::string server_address("0.0.0.0:50051");
    if (argc > 1) {
        server_address = argv[1];  // override from CLI
    }
    
    // Core components
    auto memstore = std::make_shared<MemStore>();
    auto wal = std::make_shared<WAL>("custom.wal");
    auto lifecycle = std::make_shared<ActorLifecycle>(memstore);
    auto pool = std::make_shared<ThreadPool<>>(4); // 4 threads for testing
    auto wb = std::make_shared<WriteBehindWorker>(*memstore, *wal);
    
    auto service = IquoraServiceImpl::Create(
        memstore, wal, wb, lifecycle, pool
    );

    // Build gRPC server
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[Iquora] StateStore server listening on " << server_address << std::endl;

    // Start background workers
    wb->start();

    // Wait until shutdown
    server->Wait();

    // Stop workers cleanly
    wb->stop();
    pool->Stop();
    
    return 0;
}