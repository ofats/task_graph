#include "task_runner/task_system.h"

#include <iostream>

#include "task_runner/event.h"
#include "catch/catch.h"

namespace {

void sample_func(void* arg) {
    static_cast<base::manual_event*>(arg)->notify();
}

void inner_call_func(void* arg) {
    auto* t = static_cast<
        std::tuple<base::simple_task_system*, base::manual_event*>*>(arg);

    base::manual_event event;
    std::get<0>(*t)->run(sample_func, &event);
    event.wait();

    std::get<1>(*t)->notify();
}

}  // namespace

TEST_CASE("simple_test", "[simple_task_system]") {
    base::simple_task_system pool;
    base::manual_event event;
    pool.run(sample_func, &event);
    event.wait();
}

TEST_CASE("inner_call_test", "[simple_task_system]") {
    base::simple_task_system pool;
    base::manual_event event;
    std::tuple args_type{&pool, &event};
    pool.run(inner_call_func, &args_type);
    event.wait();
}
