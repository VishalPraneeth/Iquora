#ifndef CALLABLE_H_
#define CALLABLE_H_

class Callable {
    // This wraps a function and makes it move-only. function callables are
    // copy-constructible, which would not be thread safe since the function
    // could be modified or destroyed anytime after after being passed to a
    // packaged_task
private:
    struct ImplBase {
        virtual void call() = 0;
        virtual ~ImplBase() {}
    };

    template <typename F>
    struct ImplType : ImplBase {
        F f;
        ImplType(F &&func) : f(std::forward<F>(func)) {}
        void call() override { f(); }
    };

    std::unique_ptr<ImplBase> impl;

    template <typename F>
    struct ImplType<std::unique_ptr<F>> : ImplBase {
        // Specialization for unique_ptr pointers to callable types
        F f;
        ImplType(F f_) : f(std::forward<F>(f_)) {}
        void call() {
            f();
        }
    };

public:
    Callable() = default;

    template <typename F>
    Callable(F &&f) {
        impl = std::unique_ptr<ImplType<F>>{new ImplType<F>(std::forward<F>(f))};
    }

    void operator()() {
        if(impl) {
            impl->call();
        }
    }
    
     // Allow moving
    Callable(Callable&&) = default;
    Callable& operator=(Callable&&) = default;   

    Callable(const Callable &) = delete;
    Callable &operator=(const Callable&) = delete;
};

#endif // CALLABLE_H_