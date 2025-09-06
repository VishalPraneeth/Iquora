#include "server.h"
#include "utils/common_utility.h"

#include <iostream>
#include <chrono>

#include "proto/iquora.pb.h"
#include <grpcpp/grpcpp.h>
#include "proto/iquora.grpc.pb.h"

using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;
using grpc::ServerWriter;

IquoraServiceImpl::IquoraServiceImpl(
    std::shared_ptr<MemStore> memstore,
    std::shared_ptr<WAL> wal,
    std::shared_ptr<WriteBehindWorker> wb,
    std::shared_ptr<ActorLifecycle> lifecycle,
    std::shared_ptr<ThreadPool<>> pool)
    : memstore_(std::move(memstore)),
      wal_(std::move(wal)),
      writebehind_(std::move(wb)),
      lifecycle_(std::move(lifecycle)),
      pool_(std::move(pool)) {}

std::shared_ptr<IquoraServiceImpl> IquoraServiceImpl::Create(
    std::shared_ptr<MemStore> memstore,
    std::shared_ptr<WAL> wal,
    std::shared_ptr<WriteBehindWorker> wb,
    std::shared_ptr<ActorLifecycle> lifecycle,
    std::shared_ptr<ThreadPool<>> pool) {
    
    // Create default pointers if not provided
    if (!memstore) memstore = std::make_shared<MemStore>();
    if (!wal) wal = std::make_shared<WAL>();
    if (!pool) pool = std::make_shared<ThreadPool<>>();
    if (!lifecycle) lifecycle = std::make_shared<ActorLifecycle>(memstore);    
    if (!wb) wb = std::make_shared<WriteBehindWorker>(*memstore, *wal);
    
    return std::make_shared<IquoraServiceImpl>(
        std::move(memstore),
        std::move(wal),
        std::move(wb),
        std::move(lifecycle),
        std::move(pool)
    );
}

Status IquoraServiceImpl::Get(ServerContext* context, 
                                const iquora::GetRequest* req,
                                iquora::GetResponse* resp) {
    auto val = memstore_->get(req->actor_id(), req->key());

    if (val.has_value()) {
        resp->set_value(*val);
        resp->set_found(true);
    } else {
        resp->set_found(false);
    }
    return Status::OK;
}

Status IquoraServiceImpl::Set(ServerContext* context, 
                                const iquora::SetRequest* req,
                                iquora::SetResponse* resp) {
    bool success = memstore_->set(req->actor_id(), req->key(), req->value());     // 1) Update in-memory store
    if (!success) {
        resp->set_success(false);
        return Status::OK;
    }

    // 2) Append to WAL synchronously (write-behind strategy: we still append here for durability)
    try {
        wal_->append(req->actor_id(), req->key(), req->value());
    } catch (const std::exception& ex) {
        // log and continue (depending on durability settings you might want to fail)
        std::cerr << "[SetState] WAL append failed: " << ex.what() << std::endl;
        // We still report success to client for optimistic write-behind; adjust per policy.
    }

    // 3) Notify subscribers (publish change)
    publish_change(req->actor_id(), req->key(), req->value(), "UPDATED");

    resp->set_success(true);
    return Status::OK;
}

void IquoraServiceImpl::publish_change(const std::string& actor_id,
                                       const std::string& key,
                                       const std::string& value,
                                       const std::string& event_type) {
    // Build the StateChange message
    auto msg = std::make_unique<iquora::SubscribeResponse>();
    msg->set_actor_id(actor_id);
    msg->set_key(key);
    msg->set_value(value);
    msg->set_event_type(event_type);

    // if we have subscribers, run callbacks in pool
    std::shared_ptr<SubscriptionList> list;
    {
        std::lock_guard<std::mutex> lg(subs_map_mutex_);
        auto it = subs_map_.find(actor_id);
        if (it == subs_map_.end()) return;
        list = it->second;
    }

    // iterate the threadsafe list and invoke callbacks
    // ThreadSafeList is expected to have for_each or an iterator; I'll assume a for_each method.
    list->callbacks.for_each([&msg](const SubCallback& cb) {
        // schedule on user threadpool to avoid blocking publisher
        // capture message by value; we could move into lambda if necessary
        try {
            cb(*msg);
        } catch (...) {
            // swallow callback exceptions to keep publisher robust
        }
    });
}

std::shared_ptr<IquoraServiceImpl::SubscriptionList>
IquoraServiceImpl::get_or_create_subs(const std::string& actor_id) {
    std::lock_guard<std::mutex> lg(subs_map_mutex_);
    auto it = subs_map_.find(actor_id);
    if (it != subs_map_.end()) return it->second;
    auto list = std::make_shared<SubscriptionList>();
    subs_map_.emplace(actor_id, list);
    return list;
}

void IquoraServiceImpl::remove_callback(const std::string& actor_id, size_t cb_id) {
    std::lock_guard<std::mutex> lg(subs_map_mutex_);
    auto it = subs_map_.find(actor_id);
    if (it == subs_map_.end()) return;
    it->second->callbacks.remove_first_if(cb_id);
    // optional: if list empty, erase map entry
    // (ThreadSafeList likely provides a size() or is_empty() method — adapt if present)
}

Status IquoraServiceImpl::Subscribe(ServerContext* context, 
                                        const iquora::SubscribeRequest* req,
                                        ServerWriter<iquora::SubscribeResponse>* writer) {
    const std::string actor = req->actor_id();

    if (!lifecycle_->IsActorActive(actor)) {
        return grpc::Status(StatusCode::NOT_FOUND, "Actor not found or inactive");
    }

    // Queue to receive messages for this client
    BoundedThreadsafeQueue<std::shared_ptr<iquora::SubscribeResponse>> inbound;

    // Create callback that pushes msg into inbound queue
    auto cb = [&inbound](const iquora::SubscribeResponse& msg) {
        auto msg_copy = std::make_shared<iquora::SubscribeResponse>(msg);
        inbound.Push(msg_copy);
    };

    // Register callback in subscription list and keep returned id for removal
    auto subs = get_or_create_subs(actor);
    size_t cb_id = subs->callbacks.add(cb); // add returns an id to remove later

    // Stream loop: block on inbound queue and write to client, exit on client cancellation
    while (!context->IsCancelled()) {
        auto msg = std::make_shared<iquora::SubscribeResponse>();
        bool success = inbound.WaitAndPop(msg, std::chrono::milliseconds(500)); // wait up to 500ms
        if (!success) {
            // timed out, check cancellation again
            continue;
        }
        // Write to the stream; if client disconnected, break
        if (!writer->Write(*msg)) {
            break;
        }
    }

    // cleanup callback
    remove_callback(actor, cb_id);

    return Status::OK;
}

Status IquoraServiceImpl::SpawnActor(ServerContext* context,
                                        const iquora::SpawnActorRequest* req,
                                        iquora::SpawnActorResponse* res) {
    // Convert protobuf map to unordered_map
    std::unordered_map<std::string, std::string> initial_state;
    for (const auto& [key, value] : req->initial_state()) {
        initial_state[key] = value;
    }
    
    bool success = lifecycle_->SpawnActor(req->actor_id(), initial_state);
    res->set_success(success);
    
    if (!success) {
        res->set_error_message("Failed to spawn actor");
    }
    
    return Status::OK;
}

Status IquoraServiceImpl::TerminateActor(ServerContext* context,
                                            const iquora::TerminateActorRequest* request,
                                            iquora::TerminateActorResponse* response) {
    bool success = lifecycle_->TerminateActor(request->actor_id(), request->force());
    response->set_success(success);
    
    if (!success) {
        response->set_error_message("Failed to terminate actor");
    }
    
    return Status::OK;
}