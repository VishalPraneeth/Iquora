#ifndef ACTOR_SYSTEM_H_
#define ACTOR_SYSTEM_H_

#include "scheduler.h"
#include "utils/thread_pool.h"
#include "mem_store.h"
#include <memory>
#include <unordered_map>
#include <mutex>
#include "actor_lifecycle.h"
#include "abstract_actor.h"

class ActorSystem {
public:
    ActorSystem(std::shared_ptr<MemStore> store = nullptr)
            : lifecycle_(std::make_shared<ActorLifecycle>(store)) {
        std::unique_ptr<ThreadPool<>> poolUptr{new ThreadPool<>()};
        scheduler_ = std::unique_ptr<Scheduler>{new Scheduler(std::move(poolUptr))};
    }

    template <typename ActorType>
    bool Register(std::shared_ptr<ActorType> actor) {
        if (!actor) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string& actor_id = actor->GetActorId();

        if (registered_actors_.find(actor_id) != registered_actors_.end()) {
            return false;
        }

        if(!actor->Initialize()){
            return false;
        }
        
        scheduler_->Register(actor);
        registered_actors_[actor_id] = std::static_pointer_cast<IActor>(actor);

        return true;
    }

    template <typename ActorType>
    bool Deregister(const std::string &actor) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = registered_actors_.find(actor_id);
        if (it == registered_actors_.end()) {
            return false;
        }
        
        // Stop the actor through the interface
        it->second->Stop();
        
        scheduler_->Deregister(std::static_pointer_cast<ActorType>(it->second));
        registered_actors_.erase(it);
        
        return lifecycle_->TerminateActor(actor_id, false);
    }

    template <typename ActorType>
    std::shared_ptr<ActorType> GetActor(const std::string& actor_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registered_actors_.find(actor_id);
        if (it != registered_actors_.end()) {
            return std::dynamic_pointer_cast<ActorType>(it->second);
        }
        return nullptr;
    }

    void Shutdown()
    {
         std::lock_guard<std::mutex> lock(mutex_);
        
        // Stop all actors through the IActor interface
        for (auto& [actor_id, actor] : registered_actors_) {
            actor->Stop();
        }
        
        scheduler_->Shutdown();
        registered_actors_.clear();
    }

    std::shared_ptr<ActorLifecycle> GetLifecycle() const {
        return lifecycle_;
    }

    size_t GetRegisteredActorCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return registered_actors_.size();
    }

private:
    std::unique_ptr<Scheduler> scheduler_;
    std::shared_ptr<ActorLifecycle> lifecycle_;
    std::unordered_map<std::string, std::shared_ptr<IActor>> registered_actors_;
    mutable std::mutex mutex_;
};

#endif // ACTOR_SYSTEM_H_