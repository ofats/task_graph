#include "task_runner/task_runner.h"

#include <iostream>

#include "catch/catch.h"
#include "task_runner/event.h"

namespace {

bool global_done = false;
base::auto_event global_event;
int global_result = 0;

void foo() {
    global_done = true;
    global_event.notify();
}

void foo_args(int a, int b, int c) {
    global_result = a + b + c;
    global_event.notify();
}

struct foo_t {
    void foo(base::manual_event* event) {
        done = true;
        event->notify();
    }
    bool done = false;
};

}  // namespace

TEMPLATE_PRODUCT_TEST_CASE("task_runner_test", "[task_runner]",
                           base::task_runner_base, (base::simple_task_system)) {
    TestType task_runner;

    SECTION("Task can be simple function") {
        task_runner.run(foo);
        global_event.wait();
    }

    SECTION("Task can be simple function taking arguments") {
        task_runner.run(foo_args, 1, 2, 3);
        global_event.wait();
        REQUIRE(global_result == 1 + 2 + 3);
    }

    SECTION("Task can be lambda") {
        bool done = false;
        base::manual_event event;
        task_runner.run([&] {
            done = true;
            event.notify();
        });
        event.wait();
        REQUIRE(done);
    }

    SECTION("Task can be lambda with args") {
        int result = 0;
        base::manual_event event;
        task_runner.run([&](int a, int b, int c) {
            result = a + b + c;
            event.notify();
        }, 1, 2, 3);
        event.wait();
        REQUIRE(result == 1 + 2 + 3);
    }

    SECTION(
        "Task can be member function (first argument - object for which "
        "function would be called)") {
        foo_t f;
        REQUIRE_FALSE(f.done);
        base::manual_event event;
        task_runner.run(&foo_t::foo, &f, &event);
        event.wait();
        REQUIRE(f.done);
    }

    SECTION("Task takes arguments only be value") {
        std::exception_ptr exc;
        auto f = [&exc](std::vector<int> v) {
            try {
                REQUIRE(v.size() == 5);
            } catch (...) {
                exc = std::current_exception();
            }
        };
        std::vector<int> v(5, 10);
        REQUIRE(v.size() == 5);
        task_runner.run(f, std::move(v));
        // Object was moved out
        REQUIRE(v.empty());
        if (exc) {
            std::rethrow_exception(exc);
        }
    }

    SECTION("Reference wrappers are applicable") {
        auto f = [](std::vector<int>& v, base::manual_event& e) {
            v.push_back(10);
            e.notify();
        };
        std::vector<int> v(5, 10);
        base::manual_event event;
        REQUIRE(v.size() == 5);
        task_runner.run(f, std::ref(v), std::ref(event));
        event.wait();
        REQUIRE(v.size() == 6);
    }
}
