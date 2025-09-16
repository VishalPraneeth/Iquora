#include "actor_lifecycle.h"
#include<algorithm>
#include<regex>

ActorLifecycle::ActorLifecycle(std::shared_ptr<MemStore> store)
    : store_(store) {}

ActorLifecycle::~ActorLifecycle() {
    // Cleanup if needed
}

bool ActorLifecycle::ValidateActorId(const std::string& actor_id) const {
    // Basic validation // TO-DO need to adjust logic
    static const std::regex valid_id_regex("^[a-zA-Z0-9_-]{1,64}$");
    return std::regex_match(actor_id, valid_id_regex);
}

bool ActorLifecycle::SpawnActor(const std::string& actor_id,
                              const std::unordered_map<std::string, std::string>& initial_state = {}) {
    if (!ValidateActorId(actor_id)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if actor already exists
    if (!ActorExists(actor_id)) {
        return false;
    }
    
    // Execute pre-spawn hook
    ExecuteHookSafely(pre_spawn_hook_, actor_id);
    
    try {
        // Initialize actor state
        if (store_ && !initial_state.empty()) {
            for (const auto& [key, value] : initial_state) {
            store_->set(actor_id, key, value);
            }
        }
        
        // Mark actor as active
        active_actors_.insert(actor_id);
        
        // Execute post-spawn hook
        ExecuteHookSafely(post_spawn_hook_, actor_id);
        
        return true;
    } catch (const std::exception& e) {
        active_actors_.erase(actor_id);  // Erase on failure
        return false;
    }
}

bool ActorLifecycle::TerminateActor(const std::string& actor_id, bool force = false) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (active_actors_.find(actor_id) == active_actors_.end()) {
        return false;
    }
    
    // Execute pre-terminate hook
    ExecuteHookSafely(pre_terminate_hook_, actor_id);
    
    try {
        // Optionally: Clear actor state (if not force, might want to preserve)
        if (force && store_) {
            // Clear all keys for this actor
            // This would require additional methods in MemStore
        }
        
        // Mark actor as inactive
        active_actors_.erase(actor_id);
        
        // Execute post-terminate hook
        ExecuteHookSafely(post_terminate_hook_, actor_id);
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool ActorLifecycle::ActorExists(const std::string& actor_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_actors_.find(actor_id) != active_actors_.end();
}

bool ActorLifecycle::IsActorActive(const std::string& actor_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_actors_.find(actor_id) != active_actors_.end();   
}

void ActorLifecycle::RegisterPreSpawnHook(LifecycleCallback hook) {
    std::lock_guard<std::mutex> lock(mutex_);
    pre_spawn_hook_ = std::move(hook);
}

void ActorLifecycle::RegisterPostSpawnHook(LifecycleCallback hook) {
    std::lock_guard<std::mutex> lock(mutex_);
    post_spawn_hook_ = std::move(hook);
}

void ActorLifecycle::RegisterPreTerminateHook(LifecycleCallback hook) {
    std::lock_guard<std::mutex> lock(mutex_);
    pre_terminate_hook_ = std::move(hook);
}

void ActorLifecycle::RegisterPostTerminateHook(LifecycleCallback hook) {
    std::lock_guard<std::mutex> lock(mutex_);
    post_terminate_hook_ = std::move(hook);
}

void ActorLifecycle::ExecuteHookSafely(LifecycleCallback hook, const std::string& actor_id) {
    if (hook) {
        try {
            hook(actor_id);
        } catch (const std::exception& e) {
            // Log error but don't fail the operation
        }
    }
}

size_t ActorLifecycle::GetActiveActorCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_actors_.size();
}

std::vector<std::string> ActorLifecycle::GetActiveActors() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<std::string>(active_actors_.begin(), active_actors_.end());
}

