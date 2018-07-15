#include <future>
#include <iostream>
#include <cassert>

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

    R get() {
        return future_.get();
    }

    bool valid() {
        return future_.valid();
    }
};

template <>
struct my_future<void> {
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

    void get() {
        return future_.get();
    }

    bool valid() {
        return future_.valid();
    }
};

template <typename F, typename... Deps>
my_future<std::invoke_result_t<
    std::decay_t<F>,
    decltype(std::declval<std::decay_t<Deps>>().get())...>>
contraction(F&& f, Deps&&... deps) {
    return my_async(
        [f = std::forward<F>(f)](auto... deps) { return f(deps.get()...); },
        std::forward<Deps>(deps)...);
}

template <typename F, typename... Args>
my_future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
my_async(F&& f, Args&&... args) {
    return std::async(std::forward<F>(f), std::forward<Args>(args)...);
}

int test() {
    auto future = my_async([](int a, int b) { return a + b; }, 3, 4)
                      .then([](int c, int d) { return c * d; }, 5);
    return future.get();
}

int test2() {
    return contraction(
               [](int a, int b, int c) { return a + b * c; },
               my_async([] { return 2; }),
               contraction(
                   [](int a, int b) { return a + b; },
                   my_async([](int a, int b) { return a * b; }, 5, 6),
                   my_async([] { return 2; })),
               my_async([] { return 2; }))
        .get();
}

int test3() {
    auto f1 = my_async([](int a, int b) { return a * b + 5; }, 2, 3);
    auto f2 = f1.then([](int a, int b) { return a + b; }, 25);
    assert(!f1.valid());
    return f2.get();
}

void test4() {
    auto f1 =
        my_async([](int a, int b) { std::cout << a * b << std::endl; }, 2, 3)
            .then([](int a, int b) { std::cout << a + b << std::endl; }, 4, 5);
    f1.get();
}

int main() {
    std::cout << test() << std::endl;
    std::cout << test2() << std::endl;
    std::cout << test3() << std::endl;
    test4();
    return 0;
}
