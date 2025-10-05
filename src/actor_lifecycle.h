#pragma once
#include <string>
#include <iostream>
#include <unordered_set>
#include<vector>
#include<memory>
#include<mutex>
#include<functional>
#include "mem_store.h" 

class ActorLifecycle {
public:
    using LifecycleCallback = std::function<void(const std::string& actor_id)>;
    
    ActorLifecycle(std::shared_ptr<MemStore> store);
    ~ActorLifecycle();

    // Lifecycle operations
    bool SpawnActor(const std::string& actor_id, 
                   const unordered_map<std::string, std::string>& initial_state = {});
    bool TerminateActor(const std::string& actor_id, bool force = false);

    // Check actor status
    bool ActorExists(const std::string& actor_id) const;
    bool IsActorActive(const std::string& actor_id) const;
    
    // Lifecycle hooks
    void RegisterPreSpawnHook(LifecycleCallback hook);
    void RegisterPostSpawnHook(LifecycleCallback hook);
    void RegisterPreTerminateHook(LifecycleCallback hook);
    void RegisterPostTerminateHook(LifecycleCallback hook);

    // Statistics
    size_t GetActiveActorCount() const;
    std::vector<std::string> GetActiveActors() const;

    std::shared_ptr<MemStore> GetStore() const {
        return store_;
    }

private:
    std::shared_ptr<MemStore> store_;
    mutable std::mutex mutex_;
    std::unordered_set<std::string> active_actors_;

    // Lifecycle hooks
    LifecycleCallback pre_spawn_hook_;
    LifecycleCallback post_spawn_hook_;
    LifecycleCallback pre_terminate_hook_;
    LifecycleCallback post_terminate_hook_;
    
    void ExecuteHookSafely(LifecycleCallback& hook, const std::string& actor_id);
    bool ValidateActorId(const std::string& actor_id) const;
};
