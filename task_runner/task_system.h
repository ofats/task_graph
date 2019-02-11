#ifndef _TASK_SYSTEM_H_
#define _TASK_SYSTEM_H_

#include <cassert>
#include <condition_variable>
#include <queue>
#include <thread>

namespace base {

namespace detail {

template <class T>
class notification_queue {
  public:
    template <class U>
    void push(U&& value) {
        {
            std::unique_lock guard{queue_mtx_};
            assert(running_);
            queue_.emplace(std::forward<U>(value));
        }
        queue_cv_.notify_one();
    }

    bool try_pop(T* const ret) {
        assert(ret != nullptr);

        std::unique_lock guard{queue_mtx_};
        while (queue_.empty() && running_) {
            queue_cv_.wait(guard);
        }
        if (queue_.empty()) {
            return false;
        }
        *ret = queue_.front();
        queue_.pop();
        return true;
    }

    void done() {
        {
            std::unique_lock guard{queue_mtx_};
            running_ = false;
        }
        queue_cv_.notify_all();
    }

  private:
    bool running_ = true;
    std::queue<T> queue_;
    std::condition_variable queue_cv_;
    std::mutex queue_mtx_;
};

}  // class notification_queue

class simple_task_system {
  public:
    static const std::size_t thread_count;
    using task_type = void(*)(void*);

  public:
    simple_task_system() {
        threads_.resize(thread_count);
        for (auto& t : threads_) {
            t = std::thread{[&] {
                task_context task;
                for (;;) {
                    if (!tasks_queue_.try_pop(&task)) {
                        break;
                    }
                    assert(task.func != nullptr);
                    task.func(task.args);
                }
            }};
        }
    }

    ~simple_task_system() {
        tasks_queue_.done();
        for (auto& t : threads_) {
            t.join();
        }
    }

    void run(task_type f, void* const args) {
        tasks_queue_.push(task_context{f, args});
    }

  private:
    struct task_context {
        task_type func = nullptr;
        void* args = nullptr;
    };

  private:
    std::vector<std::thread> threads_;
    detail::notification_queue<task_context> tasks_queue_;
};

}  // namespace base

#endif  // _TASK_SYSTEM_H_
