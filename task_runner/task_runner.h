#ifndef _TASK_RUNNER_H_
#define _TASK_RUNNER_H_

#include <future>
#include <variant>

#include "task_system.h"

namespace base {

namespace detail {

// InvokeContext == std::tuple<std::promise<T>, F, std::tuple<Args...>>
template <class InvokeContext>
void cast_invoke(void* p) {
    std::unique_ptr<InvokeContext> ctx{static_cast<InvokeContext*>(p)};
    try {
        std::get<0>(*ctx).set_value(std::apply(std::move(std::get<1>(*ctx)),
                                               std::move(std::get<2>(*ctx))));
    } catch (...) {
        std::get<0>(*ctx).set_exception(std::current_exception());
    }
}

}  // namespace detail

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
        using invoke_tuple = std::tuple<std::promise<result_t>, std::decay_t<F>, args_tuple>;

        auto p = std::make_unique<invoke_tuple>(
            std::promise<result_t>{}, std::forward<F>(f),
            std::forward_as_tuple(std::forward<Args>(args)...));

        auto ret = std::get<0>(*p).get_future();

        task_system_.run(detail::cast_invoke<invoke_tuple>, p.get());
        // Context transfered to task system successfully.
        p.release();

        return ret;
    }

  private:
    TaskSystem task_system_;
};

using simple_task_runner = task_runner_base<simple_task_system>;

}  // namespace base

#endif  // _TASK_RUNNER_H_
