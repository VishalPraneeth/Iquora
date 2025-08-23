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
        mutex m;
        shared_ptr<T> data;
        unique_ptr<node> next;
        node* prev;  // previous pointer for easier removal

        node() : next(), prev(nullptr) {}

        explicit node(T const &value) : data(make_shared<T>(value)), prev(nullptr) {}
    };

    node head;
    std::atomic<size_t> size_{0};

public:
    threadsafe_list(){}

    ~threadsafe_list() {
        remove_if([](T const &) { return true; });  // remove node
    }

    threadsafe_list(threadsafe_list const &) = delete;
    threadsafe_list &operator=(threadsafe_list const &) = delete;

    void push_front(T const &value)
    {
        unique_ptr<node> new_node(new node(value));
        lock_guard<mutex> lk(head.m);
        new_node->next = move(head.next);
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
        unique_lock<mutex> lk(head.m);
        while (node *const next = current->next.get())
        {
            unique_lock<mutex> next_lk(next->m);
            lk.unlock();
            f(*next->data);
            current = next;
            lk = move(next_lk);
        }
    }

    template <typename Predicate>
    shared_ptr<T> find_first_if(Predicate p)
    {
        node *current = &head;
        unique_lock<mutex> lk(head.m);
        while (node *const next = current->next.get())
        {
            unique_lock<mutex> next_lk(next->m);
            lk.unlock();
            if (p(*next->data))
            {
                return next->data;
            }
            current = next;
            lk = move(next_lk);
        }
        return {};
    }

    template <typename Predicate>
    bool remove_first_if(Predicate p) {
        node* current = &head;
        unique_lock<mutex> lk(head.m);
        while (node* const next = current->next.get()) {
            unique_lock<mutex> next_lk(next->m);
            if (p(*next->data)) {
                unique_ptr<node> old_next = move(current->next);
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

    template <typename Predicate>
    void remove_if(Predicate p)
    {
        node *current = &head;
        unique_lock<mutex> lk(head.m);
        while (node *const next = current->next.get())
        {
            unique_lock<mutex> next_lk(next->m);
            if (p(*next->data))
            {
                unique_ptr<node> old_next = move(current->next);
                if (old_next->next) {
                    old_next->next->prev = current;
                }
                current->next = move(old_next->next);
                next_lk.unlock();
                size_--;
            }
            lk.unlock();
            current = next;
            lk = move(next_lk);
        }
    }

    size_t size() const noexcept { return size_; }
};

#endif // THREADSAFE_LIST_H_
