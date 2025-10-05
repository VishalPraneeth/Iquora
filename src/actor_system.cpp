#include "actor_system.h"
#include "scheduler.h"
#include "utils/thread_pool.h"

// Constructor implementation
ActorSystem::ActorSystem(std::shared_ptr<MemStore> store)
    : lifecycle_(std::make_shared<ActorLifecycle>(store)) {
    
    auto poolUptr = std::make_unique<ThreadPool<>>();
    scheduler_ = std::make_unique<Scheduler>(std::move(poolUptr));
}

ActorSystem::~ActorSystem() {
    Shutdown();
}

void ActorSystem::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Stop all actors through the IActor interface
    for (auto& [actor_id, actor] : registered_actors_) {
        actor->Stop();
    }
    
    if (scheduler_) {
        scheduler_->Shutdown();
    }
    registered_actors_.clear();
}

    
        
       