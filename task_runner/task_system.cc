#include "task_system.h"

namespace base {

const std::size_t simple_task_system::thread_count =
    std::thread::hardware_concurrency();

}  // namespace base
