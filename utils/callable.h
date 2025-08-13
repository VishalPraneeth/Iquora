#ifndef CALLABLE_H_
#define CALLABLE_H_
using namespace std;

class Callable
{
    // This wraps a function and makes it move-only. function callables are
    // copy-constructible, which would not be thread safe since the function
    // could be modified or destroyed anytime after after being passed to a
    // packaged_task
private:
    struct ImplBase
    {
        virtual void call() = 0;
        virtual ~ImplBase() {}
    };
    unique_ptr<ImplBase> impl;

    template <typename F>
    struct ImplType : ImplBase
    {
        F f;
        ImplType(F &&f_) : f(move(f_)) {}
        void call() { f(); }
    };

    template <typename F>

    struct ImplType<unique_ptr<F>> : ImplBase
    {
        // Specialization for unique_ptr pointers to callable types
        F f;
        ImplType(F f_) : f(move(f_)) {}
        void call()
        {
            f();
        }
    };

public:
    template <typename F>
    Callable(F &&f)
    {
        impl = unique_ptr<ImplBase>{new ImplType<F>(move(f))};
    }

    void operator()()
    {
        impl->call();
    }

    Callable() = default;
    Callable(Callable &&other) : impl(move(other.impl))
    {
    }

    Callable &operator=(Callable &&other)
    {
        impl = move(other.impl);
        return *this;
    }
    
    Callable(const Callable &) = delete;
    Callable(Callable &) = delete;
    Callable &operator=(const Callable &) = delete;
};

#endif // CALLABLE_H_