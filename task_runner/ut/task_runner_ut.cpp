#include "task_runner/task_runner.h"

#include <iostream>

#include "task_runner/event.h"
#include "catch/catch.h"

namespace {

constexpr int SAMPLE_A = 4;
constexpr int SAMPLE_B = 7;
constexpr int ITER_COUNT = 10;

}  // namespace

TEST_CASE("task_runner_smoking_test", "[task_runner]") {
    base::simple_task_runner tr;
    auto fut = tr.async([](int a, int b) { return a + b; }, SAMPLE_A, SAMPLE_B);
    REQUIRE(fut.valid());
    REQUIRE(fut.get() == SAMPLE_A + SAMPLE_B);
}

TEST_CASE("task_runner_concurrency_test", "[task_runner]") {
    base::simple_task_runner tr;
    base::auto_event e1;
    base::auto_event e2;

    std::vector<int> vals;

    auto fut1 = tr.async([&] {
        e1.wait();
        for (int i = 0; i < ITER_COUNT; ++i) {
            vals.push_back(1);
            e2.notify();
            e1.wait();
        }
        e2.notify();
        return true;
    });
    auto fut2 = tr.async([&] {
        e2.wait();
        for (int i = 0; i < ITER_COUNT; ++i) {
            vals.push_back(2);
            e1.notify();
            e2.wait();
        }
        return true;
    });
    e1.notify();
    REQUIRE(fut1.get());
    REQUIRE(fut2.get());
    REQUIRE(std::adjacent_find(vals.begin(), vals.end()) == vals.end());
}
