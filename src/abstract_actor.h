#ifndef ABSTRACT_ACTOR_H_
#define ABSTRACT_ACTOR_H_

#include "utils/threadsafe_queue.h"
#include "utils/thread_pool.h"
#include "actor_system.h"
#include "mem_store.h"

#include <queue>
#include <future>
#include <utility>
#include <tuple>
#include <memory>
#include <functional>

class ActorSystem;
class ActorLifecycle;

class IActor {
public:
    virtual ~IActor() = default;
    virtual void Stop() = 0;
    virtual std::string GetActorId() const = 0;
    virtual bool IsInitialized() const = 0;
    virtual size_t GetQueueSize() const = 0;
};

// Abstract actor is the functionality which handles Behaviour, Implementation of Actors, majorly the state logic(like OnMessage,etc)
template <typename MessageType, typename ResultType>
class AbstractActor : public IActor, public std::enable_shared_from_this<AbstractActor<MessageType, ResultType>>
{
public:
    using ActorPtr = std::shared_ptr<AbstractActor<MessageType, ResultType>>;
    using WeakActorPtr = std::weak_ptr<AbstractActor<MessageType, ResultType>>;

    AbstractActor() = default;
    AbstractActor(std::shared_ptr<ActorSystem> system, 
                 const std::string& actor_id, 
                 std::shared_ptr<MemStore> store,
                 size_t queue_capacity = 1000)
        : system_(system), lifecycle_(std::make_shared<ActorLifecycle>(store)), actor_id_(actor_id), 
          store_(store), queue_(queue_capacity), is_initialized_(false),
          is_processing_(false) {}

    
    virtual ~AbstractActor() {
        Stop();
    }

    // Lifecycle methods
    virtual bool Initialize() {
        if (is_initialized_.exchange(true)) {
            return true;
        }
        
        try {
            if (!lifecycle_->ActorExists(actor_id_)) {
                if (!lifecycle_->SpawnActor(actor_id_, {})) {
                    throw std::runtime_error("Failed to spawn actor in lifecycle");
                }
            }
            
            if (store_) {
                if (!LoadStateFromStore()) {
                    InitializeDefaultState();
                }
            } else {
                InitializeDefaultState();
            }
            
            StartMessageProcessing();
            return true;
        } catch (const std::exception& e) {
            is_initialized_ = false;
            HandleError("Initialization failed: " + std::string(e.what()));
            return false;
        }
    }

    void Stop() override {
        if (is_initialized_.exchange(false)) {
            is_processing_ = false;
            queue_.Stop();
            
            if (processing_thread_.joinable()) {
                processing_thread_.join();
            }
            
            if (lifecycle_) {
                lifecycle_->TerminateActor(actor_id_, false);
            }
        }
    }

    // Actor interface
    void Tell(MessageType&& msg, ActorPtr sender = nullptr) {
        if (!is_initialized_) {
            HandleError("Actor not initialized: " + actor_id_);
            return;
        }

        try {
            auto task = [this, msg = std::move(msg), sender, self = this->shared_from_this()]() mutable {
                try {
                    OnMessageReceived(std::move(msg), sender);
                    if (auto_persist_) {
                        PersistState();
                    }
                } catch (const std::exception& e) {
                    HandleError("Message processing failed: " + std::string(e.what()));
                }
            };
            
            if (!queue_.Push(std::make_unique<Callable>(std::move(task)))) {
                HandleError("Queue full for actor: " + actor_id_);
            }
        } catch (const std::exception& e) {
            HandleError("Failed to enqueue message: " + std::string(e.what()));
        }
    }

    template<typename T = ResultType>
    std::future<T> Ask(MessageType&& msg, ActorPtr sender = nullptr) {
        static_assert(!std::is_same_v<T, void>, "Ask cannot be used with void result types");
        
        if (!is_initialized_) {
            throw std::runtime_error("Actor not initialized: " + actor_id_);
        }

        auto promise = std::make_shared<std::promise<T>>();
        auto future = promise->get_future();

        auto task = [this, msg = std::move(msg), sender, promise, self = this->shared_from_this()]() mutable {
            try {
                if constexpr (std::is_same_v<T, ResultType>) {
                    auto result = OnMessageReceivedWithResult(std::move(msg), sender);
                    promise->set_value(std::move(result));
                } else {
                    auto result = OnMessageReceivedWithResult(std::move(msg), sender);
                    promise->set_value(static_cast<T>(std::move(result)));
                }
                
                if (auto_persist_) {
                    PersistState();
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
                HandleError("Ask processing failed for actor: " + actor_id_);
            }
        };

        if (!queue_.Push(std::make_unique<Callable>(std::move(task)))) {
            promise->set_exception(std::make_exception_ptr(
                std::runtime_error("Queue full for actor: " + actor_id_)));
        }
        
        return future;
    }

    // State management
    virtual bool PersistState() {
        if (!store_) {
            return false;
        }

        try {
            std::lock_guard<std::mutex> lock(state_mutex_);
            auto state = SerializeState();
            // return store_->Store(actor_id_, state);
            return true;
        } catch (const std::exception& e) {
            HandleError("State persistence failed: " + std::string(e.what()));
            return false;
        }
    }

    // Getters
    std::string GetActorId() const { return actor_id_; }
    uint32_t GetQueueSize() const { return queue_.GetSize(); }
    bool IsInitialized() const { return is_initialized_; }
    bool IsProcessing() const { return is_processing_; }
    BoundedThreadsafeQueue<std::unique_ptr<Callable>>* GetQueueRef() { return &queue_; }

protected:
    // Pure virtual methods
    virtual void OnMessageReceived(MessageType&& msg, ActorPtr sender) = 0;
    virtual ResultType OnMessageReceivedWithResult(MessageType&& msg, ActorPtr sender) = 0;
    virtual bool LoadStateFromStore() = 0;
    virtual void InitializeDefaultState() = 0;
    virtual std::string SerializeState() const = 0;
    virtual void DeserializeState(const std::string& serialized_state) = 0;

    // virtual void PersistState() = 0;

    // Error handling
    virtual void HandleError(const std::string& error_message) {
        if (system_) {
            // system_->HandleActorError(actor_id_, error_message);
        }
    }

    // Message processing
    virtual void StartMessageProcessing() {
        is_processing_ = true;
        processing_thread_ = std::thread([this]() {
            ProcessMessages();
        });
    }

    virtual void ProcessMessages() {
        while (is_processing_) {
            try {
                std::unique_ptr<Callable> task;
                if (queue_.WaitAndPop(task, std::chrono::milliseconds(100))) {
                    (*task)();
                }
            } catch (const std::exception& e) {
                HandleError("Message processing thread error: " + std::string(e.what()));
            }
        }
    }
    
    // configuration
    std::atomic<bool> auto_persist_{true};
    std::string actor_id_;
    std::shared_ptr<MemStore> store_;
    std::shared_ptr<ActorSystem> system_;
    std::shared_ptr<ActorLifecycle> lifecycle_;
    mutable std::mutex state_mutex_;

private:
    BoundedThreadsafeQueue<std::unique_ptr<Callable>> queue_;
    std::atomic<bool> is_initialized_;
    std::atomic<bool> is_processing_;
    std::thread processing_thread_;
};

#endif // ABSTRACT_ACTOR_H_