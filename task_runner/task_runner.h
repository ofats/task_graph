#ifndef _TASK_RUNNER_H_
#define _TASK_RUNNER_H_

#include <future>
#include <variant>

#include "event.h"
#include "task_system.h"

namespace base {

namespace detail {

// InvokeContext == std::tuple<std::promise<T>, F, std::tuple<Args...>>
template <class InvokeContext>
void cast_invoke(void* p) {
    std::unique_ptr<InvokeContext> ctx{static_cast<InvokeContext*>(p)};
    try {
        if constexpr (std::is_void_v<decltype(
                          std::apply(std::move(std::get<1>(*ctx)),
                                     std::move(std::get<2>(*ctx))))>) {
            std::apply(std::move(std::get<1>(*ctx)),
                       std::move(std::get<2>(*ctx)));
            std::get<0>(*ctx).set_value();
        } else {
            std::get<0>(*ctx).set_value(std::apply(
                std::move(std::get<1>(*ctx)), std::move(std::get<2>(*ctx))));
        }
    } catch (...) {
        std::get<0>(*ctx).set_exception(std::current_exception());
    }
}

// InvokeContext == std::tuple<F, std::tuple<Args...>>
template <class InvokeContext>
void cast_invoke_detached(void* p) {
    std::unique_ptr<InvokeContext> ctx{static_cast<InvokeContext*>(p)};
    std::apply(std::move(std::get<0>(*ctx)), std::move(std::get<1>(*ctx)));
}

}  // namespace detail

struct nothing {};

constexpr nothing nothing_v;

template <class T>
struct wrap_into_nothing {
    using type = T;
};

template <>
struct wrap_into_nothing<void> {
    using type = nothing;
};

template <class T>
using wrap_into_nothing_t = typename wrap_into_nothing<T>::type;

namespace exec_nodes {

template <class F>
struct leaf : F {
    template <class... Args>
    using invoke_result_t =
        wrap_into_nothing_t<std::invoke_result_t<F, Args...>>;

    leaf(F f) : F{std::move(f)} {}

    template <class TaskRunner, class Then, class... Args>
    void execute(TaskRunner* const tr, Then then, Args... args) & {
        assert(tr != nullptr);
        if constexpr (std::is_void_v<std::invoke_result_t<F, Args...>>) {
            (*this)(std::move(args)...);
            std::move(then)(nothing_v);
        } else {
            std::move(then)((*this)(std::move(args)...));
        }
    }

    template <class Then>
    auto then(Then f);
};

template <class F>
auto make_leaf(F f) {
    return leaf{std::move(f)};
}

template <class A, class B>
struct seq : A, B {
    template <class... Args>
    using invoke_result_t = typename B::template invoke_result_t<
        typename A::template invoke_result_t<Args...>>;

    seq(A&& a, B&& b) : A{std::move(a)}, B{std::move(b)} {}

    template <class TaskRunner, class Then, class... Args>
    void execute(TaskRunner* const tr, Then then, Args... args) & {
        assert(tr != nullptr);
        A::execute(tr,
                   [tr, f = static_cast<B*>(this),
                    then = std::move(then)](auto... a_args) mutable {
                       f->execute(tr, std::move(then), std::move(a_args)...);
                   },
                   std::move(args)...);
    }

    template <class Then>
    auto then(Then f);
};

template <class A, class B>
auto make_seq(A a, B b) {
    return seq{std::move(a), std::move(b)};
}

template <class... Fs>
struct all : Fs... {
    template <class... Args>
    using invoke_result_t =
        std::tuple<typename Fs::template invoke_result_t<Args...>...>;

    all(Fs... fs) : Fs{std::move(fs)}... {}

    template <class TaskRunner, class Then, class... Args>
    void execute(TaskRunner* const tr, Then then, Args... args) & {
        assert(tr != nullptr);
        execute_impl(tr, std::move(then), std::index_sequence_for<Fs...>{},
                     std::move(args)...);
    }

    template <class TaskRunner, class Then, std::size_t... Is, class... Args>
    void execute_impl(TaskRunner& tr, Then then, std::index_sequence<Is...>,
                      Args... args) {
        struct state_type {
            std::atomic<int> left;
            Then then;
            std::tuple<typename Fs::template invoke_result_t<Args...>...>
                result;
        };
        auto* state = new state_type{sizeof...(Fs), std::move(then), {}};

        (..., tr->run(
                  [tr, state, f = static_cast<Fs*>(this)](auto... args) {
                      f->execute(tr,
                                 [state](auto&& result) {
                                     std::get<Is>(state->result) =
                                         std::forward<decltype(result)>(result);
                                     if (state->left.fetch_sub(1) == 1) {
                                         state->then(std::move(state->result));
                                         delete state;
                                     }
                                 },
                                 std::move(args)...);
                  },
                  args...));
    }

    template <class Then>
    auto then(Then f);
};

template <class... Fs>
auto make_all(Fs... fs) {
    return all{std::move(fs)...};
}

template <class T, class = void>
struct is_exec_node : std::false_type {};

template <class F>
struct is_exec_node<leaf<F>> : std::true_type {};

template <class A, class B>
struct is_exec_node<seq<A, B>> : std::true_type {};

template <class... Fs>
struct is_exec_node<all<Fs...>> : std::true_type {};

template <class T>
constexpr bool is_exec_node_v = is_exec_node<T>::value;

template <class F>
auto try_transform(F f) {
    if constexpr (is_exec_node_v<std::decay_t<F>>) {
        return f;
    } else {
        return make_leaf(std::move(f));
    }
};

template <class F>
template <class Then>
auto leaf<F>::then(Then f) {
    return make_seq(std::move(*this), try_transform(std::move(f)));
}

template <class A, class B>
template <class Then>
auto seq<A, B>::then(Then f) {
    return make_seq(std::move(*this), try_transform(std::move(f)));
}

template <class... Fs>
template <class Then>
auto all<Fs...>::then(Then f) {
    return make_seq(std::move(*this), try_transform(std::move(f)));
}

template <class... Fs>
auto when_all(Fs... fs) {
    return make_all(try_transform(std::move(fs))...);
}

template <class TaskRunner, class ExecTree, class Then, class... Args>
void sync_execute(TaskRunner* tr, ExecTree* tree, Then then, Args... args) {
    assert(tr != nullptr);
    assert(tree != nullptr);
    base::manual_event finish_event;
    tree->execute(tr,
                  [&finish_event, &then](auto... args) {
                      then(std::move(args)...);
                      finish_event.notify();
                  },
                  std::move(args)...);
    finish_event.wait();
}

template <class ExecTree, class... Args>
using exec_tree_invoke_result_t = typename ExecTree::template invoke_result_t<Args...>;

template <class TaskRunner, class ExecTree, class... Args>
[[nodiscard]] std::future<
    exec_tree_invoke_result_t<ExecTree, std::decay_t<Args>...>>
async_execute(TaskRunner* tr, ExecTree* tree, Args... args) {
    assert(tr != nullptr);
    assert(tree != nullptr);
    std::promise<exec_tree_invoke_result_t<ExecTree, std::decay_t<Args>...>>
        promise;
    auto future = promise.get_future();
    tr->run(
        [tr, tree, promise = std::move(promise)](auto... args) mutable {
            tree->execute(
                tr,
                [promise = std::move(promise)](auto... result) mutable {
                    promise.set_value(std::move(result)...);
                },
                std::move(args)...);
        },
        std::move(args)...);
    return future;
}

}  // namespace exec_nodes

template <class TaskSystem>
class task_runner_base {
  public:
    template <class F, class... Args>
    [[nodiscard]] std::future<
        std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
    async(F&& f, Args&&... args) {
        using result_t =
            std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
        using args_tuple = std::tuple<std::decay_t<Args>...>;
        using invoke_tuple =
            std::tuple<std::promise<result_t>, std::decay_t<F>, args_tuple>;

        auto p = std::make_unique<invoke_tuple>(
            std::promise<result_t>{}, std::forward<F>(f),
            std::forward_as_tuple(std::forward<Args>(args)...));

        auto ret = std::get<0>(*p).get_future();

        task_system_.run(detail::cast_invoke<invoke_tuple>, p.get());
        // Context transfered to task system successfully.
        p.release();

        return ret;
    }

    template <class F, class... Args>
    void run(F&& f, Args&&... args) {
        using args_tuple = std::tuple<std::decay_t<Args>...>;
        using invoke_tuple = std::tuple<std::decay_t<F>, args_tuple>;

        auto p = std::make_unique<invoke_tuple>(
            std::forward<F>(f),
            std::forward_as_tuple(std::forward<Args>(args)...));

        task_system_.run(detail::cast_invoke_detached<invoke_tuple>, p.get());
        // Context transfered to task system successfully.
        p.release();
    }

  private:
    TaskSystem task_system_;
};

using simple_task_runner = task_runner_base<simple_task_system>;

}  // namespace base

#endif  // _TASK_RUNNER_H_
