#ifndef _TASK_GRAPH_H_
#define _TASK_GRAPH_H_

#include <atomic>
#include <cassert>
#include <functional>
#include <future>
#include <optional>
#include <type_traits>

#include "event.h"
#include "meta/type_pack.h"

namespace base::task_graph {

template <class Tree, class... Args>
constexpr auto graph_result_pack = Tree::template result_pack<Args...>;

// ==================== leaf ====================

// Simplest execution node. Calls provided invocable on its execution and
// forwards result to Then.
template <class F>
struct leaf : F {
    template <class... Args>
    static constexpr auto result_pack =
        std::conditional_t<std::is_void_v<std::invoke_result_t<F, Args...>>,
                           tp::empty_pack,
                           type_pack<std::invoke_result_t<F, Args...>>>{};

    leaf(F f) : F{std::move(f)} {}

    template <class TaskRunner, class Then, class... Args>
    void execute(TaskRunner* const tr, Then then, Args... args) & {
        assert(tr != nullptr);
        if constexpr (tp::empty(result_pack<Args...>)) {
            std::invoke(*this, std::move(args)...);
            std::invoke(then);
        } else {
            std::invoke(then, std::invoke(*this, std::move(args)...));
        }
    }

    template <class Then>
    auto then(Then f);
};

template <class F>
auto make_leaf(F f) {
    return leaf{std::move(f)};
}

// ==================== seq ====================

// Continuation execution node. Executes second subnode right after first one.
// On execution first subnode will use provided parameters.
// Second subnode is executed with first subnode's result.
// Results are represented by results of second subnode execution.
template <class A, class B>
struct seq : A, B {
    template <class... Args>
    static constexpr auto result_pack = tp::apply(
        [](auto... type) {
            return graph_result_pack<B, subtype<decltype(type)>...>;
        },
        graph_result_pack<A, Args...>);

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

// ==================== all ====================

namespace detail {

template <std::size_t n, std::size_t... is, std::size_t nxt,
          std::size_t... rest>
constexpr auto iota(std::index_sequence<is...>,
                    std::index_sequence<nxt, rest...>) {
    if constexpr (sizeof...(rest) == 0) {
        return std::index_sequence<is..., n>{};
    } else {
        return iota<n + nxt>(std::index_sequence<is..., n>{},
                             std::index_sequence<rest...>{});
    }
}

static_assert(
    std::is_same_v<decltype(iota<0>({}, std::index_sequence<3, 6, 9, 1>{})),
                   std::index_sequence<0, 3, 9, 18>>);

}  // namespace detail

// Branching execution node. Execute its subnodes in parallel using provided
// task runner.
// On execution every subnode would take provided parameters by copy.
// Results are represented as a concatenated results of its subnodes.
template <class... Fs>
struct all : Fs... {
    static_assert(sizeof...(Fs) > 0);

    template <class... Args>
    static constexpr auto result_pack = (... + graph_result_pack<Fs, Args...>);

    all(Fs... fs) : Fs{std::move(fs)}... {}

    template <class TaskRunner, class Then, class... Args>
    void execute(TaskRunner* const tr, Then&& then, Args&&... args) & {
        assert(tr != nullptr);

        constexpr auto result_types = result_pack<Args...>;
        if constexpr (tp::empty(result_types)) {
            struct state_type {
                std::atomic<int> left;
                std::decay_t<Then> then;
            };
            auto* state = new state_type{sizeof...(Fs), std::move(then)};
            (..., tr->run(
                      [tr, state, f = static_cast<Fs*>(this)](auto... args) {
                          f->execute(tr,
                                     [state] {
                                         if (state->left.fetch_sub(1) == 1) {
                                             std::invoke(state->then);
                                             delete state;
                                         }
                                     },
                                     std::move(args)...);
                      },
                      args...));
        } else {
            // Number of resulting arguments for every Fs...
            // Example: <1, 3, 3, 0>
            constexpr auto arg_nums = std::index_sequence<tp::size(
                graph_result_pack<Fs, Args...>)...>{};

            // Positions of first resulting arguments for every Fs...
            // Example: <0, 1, 4, 7>
            constexpr auto arg_start_poses = detail::iota<0>({}, arg_nums);

            // Index sequences for resulting arguments for every Fs...
            // Example: <<0>, <0, 1, 2>, <0, 1, 2>, <>>
            constexpr auto arg_indexes = type_pack_v<decltype(
                std::make_index_sequence<tp::size(
                    graph_result_pack<Fs, Args...>)>{})...>;

            execute_impl(tr, std::forward<Then>(then), arg_start_poses,
                         arg_indexes, result_types,
                         std::forward<Args>(args)...);
        }
    }

    template <class Then>
    auto then(Then f);

  private:
    template <class TaskRunner, class Then, std::size_t... start_poses,
              class... IS, class... ResultTypes, class... Args>
    void execute_impl(TaskRunner* const tr, Then&& then,
                      std::index_sequence<start_poses...>, type_pack<IS...>,
                      type_pack<ResultTypes...>, Args&&... args) {
        struct state_type {
            std::atomic<int> left;
            std::decay_t<Then> then;
            std::tuple<std::optional<ResultTypes>...> result;
        };
        auto* state =
            new state_type{sizeof...(Fs), std::forward<Then>(then), {}};

        (..., execute_one<start_poses>(tr, state, static_cast<Fs*>(this), IS{},
                                       args...));
    }

    template <std::size_t start_pos, class TaskRunner, class State, class F,
              std::size_t... Is, class... Args>
    void execute_one(TaskRunner* const tr, State* const state, F* const f,
                     std::index_sequence<Is...>, Args&&... args) {
        tr->run(
            [tr, state, f](auto... args) {
                f->execute(
                    tr,
                    [state](auto&&... results) {
                        (..., std::get<start_pos + Is>(state->result)
                                  .emplace(std::forward<decltype(results)>(
                                      results)));
                        if (state->left.fetch_sub(1) == 1) {
                            std::apply(
                                [then = std::move(state->then)](
                                    auto&&... val) mutable {
                                    std::invoke(then, std::move(*val)...);
                                },
                                std::move(state->result));
                            delete state;
                        }
                    },
                    std::move(args)...);
            },
            std::move(args)...);
    }
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
                  [&finish_event, &then](auto&&... args) {
                      then(std::forward<decltype(args)>(args)...);
                      finish_event.notify();
                  },
                  std::move(args)...);
    finish_event.wait();
}

}  // namespace base::task_graph

#endif  // _TASK_GRAPH_H_
