#ifndef THREADSAFE_LIST_H_
#define THREADSAFE_LIST_H_

#include <memory>
#include <mutex>
#include <atomic>    
#include <cstddef>     
#include <functional>

/*
A threadsafe linked list, particularly useful because it support iteration.
*/

template <typename T>
class ThreadSafeList
{
    struct node
    {
        std::mutex m;
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
        node* prev;  // previous pointer for easier removal

        node() : next(), prev(nullptr) {}

        explicit node(T const &value) : data(std::make_shared<T>(value)), prev(nullptr) {}
    };

    node head;
    std::atomic<size_t> size_{0};
    std::atomic<size_t> next_id_{1};  // ID generator

public:
    ThreadSafeList(){}

    ~ThreadSafeList() {
        remove_if([](T const &) { return true; });  // remove node
    }

    ThreadSafeList(ThreadSafeList const &) = delete;
    ThreadSafeList &operator=(ThreadSafeList const &) = delete;

    void push_front(T const &value)
    {
        std::unique_ptr<node> new_node(new node(value));
        std::lock_guard<std::mutex> lk(head.m);
        new_node->next = std::move(head.next);
        if (head.next) {
            head.next->prev = new_node.get();
        }
        new_node->prev = &head;
        head.next = std::move(new_node);
        size_++;
    }

    template <typename Function>
    void for_each(Function f)
    {
        node *current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node *const next = current->next.get())
        {
            std::unique_lock<std::mutex> next_lk(next->m);
            lk.unlock();
            f(*next->data);
            current = next;
            lk = std::move(next_lk);
        }
    }

    template <typename Predicate>
    std::shared_ptr<T> find_first_if(Predicate p)
    {
        node *current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node *const next = current->next.get())
        {
            std::unique_lock<std::mutex> next_lk(next->m);
            lk.unlock();
            if (p(*next->data))
            {
                return next->data;
            }
            current = next;
            lk = std::move(next_lk);
        }
        return {};
    }

    template <typename Predicate>
    bool remove_first_if(Predicate p) {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node* const next = current->next.get()) {
            std::unique_lock<std::mutex> next_lk(next->m);
            if (p(*next->data)) {
                std::unique_ptr<node> old_next = std::move(current->next);
                if (old_next->next) {
                    old_next->next->prev = current;
                }
                current->next = std::move(old_next->next);
                next_lk.unlock();
                size_--;
                return true;
            }
            lk.unlock();
            current = next;
            lk = std::move(next_lk);
        }
        return false;
    }

    template <typename Predicate>
    void remove_if(Predicate p)
    {
        node *current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node *const next = current->next.get())
        {
            std::unique_lock<std::mutex> next_lk(next->m);
            if (p(*next->data))
            {
                std::unique_ptr<node> old_next = std::move(current->next);
                if (old_next->next) {
                    old_next->next->prev = current;
                }
                current->next = std::move(old_next->next);
                next_lk.unlock();
                size_--;
            }
            lk.unlock();
            current = next;
            lk = std::move(next_lk);
        }
    }

    size_t add(T const &value)
    {
        size_t new_id = next_id_++;
        std::unique_ptr<node> new_node(new node(value, new_id));
        std::lock_guard<std::mutex> lk(head.m);
        new_node->next = std::move(head.next);
        if (head.next) {
            head.next->prev = new_node.get();
        }
        new_node->prev = &head;
        head.next = std::move(new_node);
        size_++;
        return new_id;
    }

    bool remove_by_id(size_t id)
    {
        node *current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node *const next = current->next.get())
        {
            std::unique_lock<std::mutex> next_lk(next->m);
            if (next->id == id)  // Check ID match
            {
                std::unique_ptr<node> old_next = move(current->next);
                if (old_next->next) {
                    old_next->next->prev = current;
                }
                current->next = move(old_next->next);
                next_lk.unlock();
                size_--;
                return true;
            }
            lk.unlock();
            current = next;
            lk = move(next_lk);
        }
        return false;
    }

    size_t size() const noexcept { return size_; }
};

#endif // THREADSAFE_LIST_H_
