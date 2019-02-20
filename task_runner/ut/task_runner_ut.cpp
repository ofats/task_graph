#include "task_runner/task_runner.h"

#include <numeric>
#include <set>

#include "catch/catch.h"
#include "task_runner/event.h"

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

TEST_CASE("exec_nodes_leaf_test", "[exec_nodes]") {
    std::vector<int> v;
    base::simple_task_runner tr;
    base::exec_nodes::leaf node{[&] { v.push_back(1); }};

    base::exec_nodes::sync_execute(&tr, &node,
                                   [&](base::nothing) { v.push_back(2); });
    REQUIRE(std::is_sorted(v.begin(), v.end()));
}

TEST_CASE("exec_nodes_seq_test", "[exec_nodes]") {
    std::vector<int> v;
    base::simple_task_runner tr;
    auto node = base::exec_nodes::leaf{[&] { v.push_back(1); }}.then(
        [&](base::nothing) { v.push_back(2); });

    base::exec_nodes::sync_execute(&tr, &node,
                                   [&](base::nothing) { v.push_back(3); });
    REQUIRE(std::is_sorted(v.begin(), v.end()));
}

TEST_CASE("exec_nodes_all_test", "[exec_nodes]") {
    std::vector<int> v;
    base::simple_task_runner tr;
    std::mutex mtx;
    auto node = base::exec_nodes::when_all(
        [&] {
            std::unique_lock lck{mtx};
            v.push_back(1);
        },
        [&] {
            std::unique_lock lck{mtx};
            v.push_back(2);
        },
        [&] {
            std::unique_lock lck{mtx};
            v.push_back(3);
        },
        [&] {
            std::unique_lock lck{mtx};
            v.push_back(4);
        });

    base::exec_nodes::sync_execute(&tr, &node, [&](auto) { v.push_back(5); });
    std::vector<int> eta(5);
    std::iota(eta.begin(), eta.end(), 1);
    std::sort(v.begin(), v.end());
    REQUIRE(std::equal(v.begin(), v.end(), eta.begin()));
}

TEST_CASE("exec_nodes_all_big_test", "[exec_nodes]") {
    DBG_OUT("exec_nodes_all_big_test start");
    std::vector<int> v;
    base::simple_task_runner tr;
    std::mutex mtx;

    auto node = base::exec_nodes::when_all(
                    [&] {
                        std::unique_lock lck{mtx};
                        v.push_back(0);
                    },
                    [&] {
                        std::unique_lock lck{mtx};
                        v.push_back(0);
                    },
                    [&] {
                        std::unique_lock lck{mtx};
                        v.push_back(0);
                    },
                    [&] {
                        std::unique_lock lck{mtx};
                        v.push_back(0);
                    })
                    .then([&](auto) { v.push_back(1); })
                    .then(base::exec_nodes::when_all(
                        [&](base::nothing) {
                            std::unique_lock lck{mtx};
                            v.push_back(0);
                        },
                        [&](base::nothing) {
                            std::unique_lock lck{mtx};
                            v.push_back(0);
                        },
                        [&](base::nothing) {
                            std::unique_lock lck{mtx};
                            v.push_back(0);
                        },
                        [&](base::nothing) {
                            std::unique_lock lck{mtx};
                            v.push_back(0);
                        }))
                    .then([&](auto) { v.push_back(1); });

    base::exec_nodes::sync_execute(&tr, &node, [](base::nothing) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    REQUIRE(std::count(v.begin(), v.end(), 1) == 2);
    DBG_OUT("exec_nodes_all_big_test finish");
}

TEST_CASE("exec_nodes_eval_test", "[exec_nodes]") {
    base::simple_task_runner tr;

    auto node =
        base::exec_nodes::all{
            base::exec_nodes::leaf{[] { return 1; }},
            base::exec_nodes::leaf{[] { return 2; }},
            base::exec_nodes::leaf{[] { return 3; }},
            base::exec_nodes::leaf{[] { return 4; }},
            base::exec_nodes::leaf{[] { return 5; }},
        }
            .then([](auto res) {
                return std::apply([](auto... vals) { return (... + vals); },
                                  res);
            });

    int result;
    base::exec_nodes::sync_execute(&tr, &node,
                                   [&result](int val) { result = val; });
    REQUIRE(result == 1 + 2 + 3 + 4 + 5);
}

TEST_CASE("exec_nodes_eval2_test", "[exec_nodes]") {
    base::simple_task_runner tr;

    auto simple_func = [](int val) {
        return val * 2 + val + 4 + val - 3 + val / 2 + val + 3;
    };

    auto node = base::exec_nodes::when_all([](int val) { return val * 2; },
                                           [](int val) { return val + 4; },
                                           [](int val) { return val - 3; },
                                           [](int val) { return val / 2; },
                                           [](int val) { return val + 3; })
                    .then([](auto res) {
                        return std::apply(
                            [](auto... vals) { return (... + vals); }, res);
                    });

    int result;
    auto setter = [&result](int val) { result = val; };

    base::exec_nodes::sync_execute(&tr, &node, setter, 5);
    REQUIRE(result == simple_func(5));

    base::exec_nodes::sync_execute(&tr, &node, setter, 25);
    REQUIRE(result == simple_func(25));

    base::exec_nodes::sync_execute(&tr, &node, setter, 0);
    REQUIRE(result == simple_func(0));
}

TEST_CASE("exec_nodes_async_simple", "[exec_nodes]") {
    base::simple_task_runner tr;

    auto node = base::exec_nodes::leaf{[](int a) { return a * 5; }}.then(
        [](int b) { return b + 2; });

    auto future = base::exec_nodes::async_execute(&tr, &node, 5);
    REQUIRE(future.valid());
    REQUIRE(future.get() == 5 * 5 + 2);
    REQUIRE(!future.valid());
}
