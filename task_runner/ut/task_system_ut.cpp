#include <iostream>

#include "catch2/catch_all.hpp"
#include "task_runner/event.h"
#include "task_runner/task_system.h"

namespace {

bool global_done = false;

void sample_func(void* arg) {
    global_done = true;
    static_cast<base::manual_event*>(arg)->notify();
}

}  // namespace

TEST_CASE("simple_test", "[simple_task_system]") {
    base::simple_task_system pool;
    base::manual_event event;
    pool.run(sample_func, &event);
    event.wait();
    REQUIRE(global_done);
}
