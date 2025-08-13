#include "server.h"
#include "utils.h" // current_timestamp()

#include <iostream>
#include <chrono>

#include "iquora.pb.h"
#include "iquora.grpc.pb.h"

IquoraServiceImpl::IquoraServiceImpl(std::shared_ptr<MemStore> memstore,
                                     std::shared_ptr<WAL> wal,
                                     std::shared_ptr<WriteBehindWorker> wb,
                                     std::shared_ptr<ActorLifecycle> lifecycle,
                                     std::shared_ptr<ThreadPool> pool)
    : memstore_(std::move(memstore)),
      wal_(std::move(wal)),
      writebehind_(std::move(wb)),
      lifecycle_(std::move(lifecycle)),
      pool_(std::move(pool)) {}

Status IquoraServiceImpl::GetState(ServerContext* /*context*/, const iquora::GetRequest* req,
                                   iquora::GetResponse* resp) {
    auto val = memstore_->get(req->actor_id(), req->key());
    if (!val.has_value()) {
        // return empty string as not found (could return NOT_FOUND if you prefer)
        resp->set_value("");
        return Status::OK;
    }
    resp->set_value(*val);
    return Status::OK;
}

Status IquoraServiceImpl::SetState(ServerContext* /*context*/, const iquora::SetRequest* req,
                                   iquora::SetResponse* resp) {
    // 1) Update in-memory store
    bool ok = memstore_->set(req->actor_id(), req->key(), req->value());
    if (!ok) {
        resp->set_success(false);
        return Status::OK;
    }

    // 2) Append to WAL synchronously (write-behind strategy: we still append here for durability)
    //    Simple newline-delimited JSON-ish record for starter: actor_id|key|value|ts
    std::string record = req->actor_id() + "|" + req->key() + "|" + req->value() + "|" + current_timestamp();
    try {
        wal_->append(record);
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
    iquora::StateChange msg;
    msg.set_actor_id(actor_id);
    msg.set_key(key);
    msg.set_value(value);
    msg.set_event_type(event_type);

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
    list->callbacks.for_each([msg](const SubCallback& cb) {
        // schedule on user threadpool to avoid blocking publisher
        // capture message by value; we could move into lambda if necessary
        try {
            cb(msg);
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
    it->second->callbacks.remove(cb_id);
    // optional: if list empty, erase map entry
    // (ThreadSafeList likely provides a size() or is_empty() method — adapt if present)
}

Status IquoraServiceImpl::SubscribeChanges(ServerContext* context, const iquora::SubscribeRequest* req,
                                           ServerWriter<iquora::StateChange>* writer) {
    const std::string actor = req->actor_id();

    // Queue to receive messages for this client
    ThreadSafeQueue<iquora::StateChange> inbound;

    // Create callback that pushes msg into inbound queue
    auto cb = [&inbound](const iquora::StateChange& msg) {
        inbound.push(msg);
    };

    // Register callback in subscription list and keep returned id for removal
    auto subs = get_or_create_subs(actor);
    size_t cb_id = subs->callbacks.add(cb); // add returns an id to remove later

    // Stream loop: block on inbound queue and write to client, exit on client cancellation
    while (!context->IsCancelled()) {
        iquora::StateChange msg;
        bool ok = inbound.pop_wait(msg, std::chrono::milliseconds(500)); // wait up to 500ms
        if (!ok) {
            // timed out, check cancellation again
            continue;
        }
        // Write to the stream; if client disconnected, break
        if (!writer->Write(msg)) {
            break;
        }
    }

    // cleanup callback
    remove_callback(actor, cb_id);

    return Status::OK;
}
