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

    // Callback wrapper with ID for removal
    struct CallbackWrapper {
        size_t id;
        SubCallback callback;
        
        CallbackWrapper(size_t id, SubCallback cb) : id(id), callback(std::move(cb)) {}
    };

    // A small subscription manager using a threadsafe map + threadsafe_list of callbacks
    struct SubscriptionList {
        ThreadSafeList<CallbackWrapper> callbacks; // list of callbacks for an actor

        std::atomic<size_t> next_id_{1};
        
        size_t add(SubCallback callback) {
            size_t id = next_id_.fetch_add(1);
            callbacks.push_front(CallbackWrapper(id, std::move(callback)));
            return id;
        }
        
        bool remove(size_t id) {
            return callbacks.remove_first_if([id](const CallbackWrapper& wrapper) {
                return wrapper.id == id;
            });
        }
        
        size_t size() const {
            return callbacks.size();
        }
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
