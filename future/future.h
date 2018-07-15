#ifndef _ADVANCED_FUTURE_H_
#define _ADVANCED_FUTURE_H_

#include <future>

template <typename R>
struct my_future;

template <typename F, typename... Args>
my_future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
my_async(F&& f, Args&&... args);

template <typename R>
struct my_future {
    std::future<R> future_;
    my_future(std::future<R> future) : future_(std::move(future)) {}

    template <typename F, typename... Args>
    my_future<std::invoke_result_t<std::decay_t<F>, R, std::decay_t<Args>...>>
    then(F&& f, Args&&... args) {
        return my_async(
            [f = std::forward<F>(f)](auto fut, auto&&... args) {
                return f(fut.get(), std::forward<decltype(args)>(args)...);
            },
            std::move(future_), std::forward<Args>(args)...);
    }

    R get() { return future_.get(); }

    bool valid() { return future_.valid(); }
};

template <>
struct my_future<void> {
  public:
    std::future<void> future_;
    my_future(std::future<void> future) : future_(std::move(future)) {}

    template <typename F, typename... Args>
    my_future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
    then(F&& f, Args&&... args) {
        return my_async(
            [f = std::forward<F>(f)](auto fut, auto&&... args) {
                fut.get();
                return f(std::forward<decltype(args)>(args)...);
            },
            std::move(future_), std::forward<Args>(args)...);
    }

    void get() { future_.get(); }

    bool valid() { return future_.valid(); }
};

template <typename F, typename... Deps>
my_future<std::invoke_result_t<
    std::decay_t<F>, decltype(std::declval<Deps>().get())...>>
contraction(F&& f, Deps... deps) {
    return my_async(
        [f = std::forward<F>(f)](auto... deps) { return f(deps.get()...); },
        std::move(deps)...);
}

template <typename F, typename... Args>
my_future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
my_async(F&& f, Args&&... args) {
    return std::async(std::forward<F>(f), std::forward<Args>(args)...);
}

#endif  // _ADVANCED_FUTURE_H_
