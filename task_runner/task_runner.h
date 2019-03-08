#ifndef _TASK_RUNNER_H_
#define _TASK_RUNNER_H_

#include "task_system.h"

namespace base {

template <class TaskSystem>
class task_runner_base {
  public:
    template <class F, class... Args>
    void run(F&& f, Args&&... args) {
        using args_tuple = std::tuple<std::decay_t<Args>...>;
        using invoke_tuple = std::tuple<std::decay_t<F>, args_tuple>;

        auto p = std::make_unique<invoke_tuple>(
            std::forward<F>(f),
            std::forward_as_tuple(std::forward<Args>(args)...));

        task_system_.run(trampoline<invoke_tuple>, p.get());
        // Context transfered to task system successfully.
        p.release();
    }

  private:
    // InvokeContext == std::tuple<F, std::tuple<Args...>>
    template <class InvokeContext>
    static void trampoline(void* p) {
        std::unique_ptr<InvokeContext> ctx{static_cast<InvokeContext*>(p)};
        std::apply(std::move(std::get<0>(*ctx)), std::move(std::get<1>(*ctx)));
    }

  private:
    TaskSystem task_system_;
};

using simple_task_runner = task_runner_base<simple_task_system>;

}  // namespace base

#endif  // _TASK_RUNNER_H_
