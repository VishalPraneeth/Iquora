#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>

#include <grpcpp/grpcpp.h>
#include "proto/iquora.grpc.pb.h"

#include "mem_store.h"
#include "wal.h"
#include "write_behind_worker.h"
#include "actor_lifecycle.h"

// Your utility containers
#include "utils/threadsafe_queue.h"
#include "utils/threadsafe_list.h"
#include "utils/thread_pool.h"

using grpc::ServerContext;
using grpc::Status;
using grpc::ServerWriter;

class IquoraServiceImpl final : public iquora::StateStore::Service {
public:
    // Factory function for easy creation
    static std::shared_ptr<IquoraServiceImpl> Create(
        std::shared_ptr<MemStore> memstore = nullptr,
        std::shared_ptr<WAL> wal = nullptr,
        std::shared_ptr<WriteBehindWorker> wb = nullptr,
        std::shared_ptr<ActorLifecycle> lifecycle = nullptr,
        std::shared_ptr<ThreadPool<>> pool = nullptr);

    // Constructor
    IquoraServiceImpl(std::shared_ptr<MemStore> memstore,
                      std::shared_ptr<WAL> wal,
                      std::shared_ptr<WriteBehindWorker> wb,
                      std::shared_ptr<ActorLifecycle> lifecycle,
                      std::shared_ptr<ThreadPool<>> pool);

    // gRPC methods
    Status Get(ServerContext* context, 
                const iquora::GetRequest* req,
                iquora::GetResponse* resp) override;

    Status Set(ServerContext* context, 
                const iquora::SetRequest* req,
                iquora::SetResponse* resp) override;

    Status Subscribe(ServerContext* context, 
                        const iquora::SubscribeRequest* req,
                        ServerWriter<iquora::SubscribeResponse>* writer) override;

    Status SpawnActor(ServerContext* context,
                        const iquora::SpawnActorRequest* req,
                        iquora::SpawnActorResponse* res) override;
    
    Status TerminateActor(ServerContext* context,
                              const iquora::TerminateActorRequest* req,
                              iquora::TerminateActorResponse* res) override;

    // programmatic helpers
    void publish_change(const std::string& actor_id,
                        const std::string& key,
                        const std::string& value,
                        const std::string& event_type);

private:
    // subscription callback signature
    using SubCallback = std::function<void(const iquora::SubscribeResponse&)>;

    // A small subscription manager using a threadsafe map + threadsafe_list of callbacks
    struct SubscriptionList {
        ThreadSafeList<SubCallback> callbacks; // list of callbacks for an actor
    };

    // find-or-create subscription list for actor
    std::shared_ptr<SubscriptionList> get_or_create_subs(const std::string& actor_id);

    // remove callback by id
    void remove_callback(const std::string& actor_id, size_t cb_id);

private:
    std::shared_ptr<MemStore> memstore_;
    std::shared_ptr<WAL> wal_;
    std::shared_ptr<WriteBehindWorker> writebehind_;
    std::shared_ptr<ActorLifecycle> lifecycle_;
    std::shared_ptr<ThreadPool<>> pool_;

    // actor_id -> SubscriptionList
    std::mutex subs_map_mutex_;
    std::unordered_map<std::string, std::shared_ptr<SubscriptionList>> subs_map_;
};
