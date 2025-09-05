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

// Abstract actor is the functionality which handles Behaviour, Implementation of Actors, majorly the state logic(like OnMessage,etc)

template <typename MessageType, typename ResultType>
class AbstractActor
{
public:
    AbstractActor() = default;
    AbstractActor(ActorSystem *system_, const std::string& actor_id, MemStore* store) : system_(system), actor_id_(actor_id), store_(store) {}

    // Load state from persistence on creation
    virtual void Initialize() {
        if (store_) {
            LoadStateFromStore();
        }
    }
    virtual ~AbstractActor() {}
    /* Actor behaviour */
    virtual void OnMessageReceived(MessageType &&msg, AbstractActor *sender = nullptr){
        ProcessMessage(msg);
        PersistState(); // Auto-persist after each message;
    }
    virtual ResultType OnMessageReceivedWithResult(MessageType msg, AbstractActor *sender = nullptr) = 0;

    /* Actor interface */
    void Tell(MessageType &&msg, AbstractActor *sender = nullptr)
    {
        Callable callable{[&]
                          { OnMessageReceived(std::move(msg), sender); }};
        std::packaged_task<void()> pt{std::move(callable)};
        std::unique_ptr<Callable> ptUptr{new Callable(std::move(pt))};
        queue.Push(std::move(ptUptr));  
    }

    std::future<ResultType> Ask(MessageType msg, AbstractActor *sender = nullptr)
    {
        // TODO: figure out how to use move semantics for the passed in message here. Could not get the permutations of move-only
        //  and copy-only types to play nicely here ("call to explicitly deleted X")
        auto func = [=]()
        { return this->OnMessageReceivedWithResult(msg, sender); };
        std::packaged_task<ResultType()> pt = std::packaged_task<ResultType()>{func};
        // NOTE: I ran into a (probably typical unique_ptr) bug here with having `return ptUptr->get_future();` as the final statement. Once `ptUptr` is
        // pushed onto the queue, `ptUptr` actually becomes a `nullptr` because the reference in the queue now "owns" the instance it is pointing to.
        auto fut = pt.get_future();
        //std::packaged_task<ResultType()> *ptPtr = new std::packaged_task<ResultType()>{std::move(pt)};
        std::unique_ptr<Callable> ptUptr{new Callable(std::move(pt))};
        queue.Push(std::move(ptUptr));
        return fut;
    }

    uint32_t GetQueueSize() { return queue.GetSize(); };
    BoundedThreadsafeQueue<std::unique_ptr<Callable>> *GetQueueRef() { return &queue; };

    
protected:
    virtual void LoadStateFromStore() = 0;
    virtual void PersistState() = 0;
    
    std::string actor_id_;
    MemStore* store_;
    ActorSystem* system_;

private:
    BoundedThreadsafeQueue<std::unique_ptr<Callable>> queue;
};

#endif // ABSTRACT_ACTOR_H_