#include <numeric>
#include <set>

#include "catch2/catch_all.hpp"
#include "task_runner/event.h"
#include "task_runner/task_graph.h"
#include "task_runner/task_runner.h"

namespace tg = base::task_graph;

TEST_CASE("task_graph_leaf_test", "[task_graph]") {
    std::vector<int> v;
    base::simple_task_runner tr;
    tg::leaf node{[&] { v.push_back(1); }};

    tg::sync_execute(&tr, &node, [&] { v.push_back(2); });
    REQUIRE(std::is_sorted(v.begin(), v.end()));
}

TEST_CASE("task_graph_seq_test", "[task_graph]") {
    std::vector<int> v;
    base::simple_task_runner tr;
    auto node = tg::leaf{[&] { v.push_back(1); }}.then([&] { v.push_back(2); });

    tg::sync_execute(&tr, &node, [&] { v.push_back(3); });
    REQUIRE(std::is_sorted(v.begin(), v.end()));
}

TEST_CASE("task_graph_all_test", "[task_graph]") {
    std::vector<int> v;
    base::simple_task_runner tr;
    std::mutex mtx;
    auto node = tg::when_all(
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

    tg::sync_execute(&tr, &node, [&] { v.push_back(5); });
    std::vector<int> eta(5);
    std::iota(eta.begin(), eta.end(), 1);
    std::sort(v.begin(), v.end());
    REQUIRE(std::equal(v.begin(), v.end(), eta.begin()));
}

TEST_CASE("task_graph_all_big_test", "[task_graph]") {
    std::vector<int> v;
    base::simple_task_runner tr;
    std::mutex mtx;

    auto node = tg::when_all(
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
                    .then([&] { v.push_back(1); })
                    .then(tg::when_all(
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
                        }))
                    .then([&] { v.push_back(1); });

    tg::sync_execute(&tr, &node, [] {});
    REQUIRE(std::count(v.begin(), v.end(), 1) == 2);
}

TEST_CASE("task_graph_eval_test", "[task_graph]") {
    base::simple_task_runner tr;

    auto node =
        tg::when_all([] { return 1; }, [] { return 2; }, [] { return 3; },
                     [] { return 4; }, [] { return 5; })
            .then([](auto... vals) { return (... + vals); });

    int result;
    tg::sync_execute(&tr, &node, [&result](int val) { result = val; });
    REQUIRE(result == 1 + 2 + 3 + 4 + 5);
}

TEST_CASE("task_graph_eval_partial_test", "[task_graph]") {
    base::simple_task_runner tr;

    auto node = tg::when_all([] { return 1; }, [] {}, [] { return 3; }, [] {},
                             [] { return 5; })
                    .then([](auto... vals) { return (... + vals); });

    int result;
    tg::sync_execute(&tr, &node, [&result](int val) { result = val; });
    REQUIRE(result == 1 + 3 + 5);
}

TEST_CASE("task_graph_eval2_test", "[task_graph]") {
    base::simple_task_runner tr;

    auto simple_func = [](int val) {
        return val * 2 + val + 4 + val - 3 + val / 2 + val + 3;
    };

    auto node = tg::when_all([](int val) { return val * 2; },
                             [](int val) { return val + 4; },
                             [](int val) { return val - 3; },
                             [](int val) { return val / 2; },
                             [](int val) { return val + 3; })
                    .then([](auto... vals) { return (... + vals); });

    int result;
    auto setter = [&result](int val) { result = val; };

    tg::sync_execute(&tr, &node, setter, 5);
    REQUIRE(result == simple_func(5));

    tg::sync_execute(&tr, &node, setter, 25);
    REQUIRE(result == simple_func(25));

    tg::sync_execute(&tr, &node, setter, 0);
    REQUIRE(result == simple_func(0));
}

TEST_CASE("task_graph_eval_unite_test", "[task_graph]") {
    base::simple_task_runner tr;

    auto simple_func = [](int val) {
        return val * 2 + val + 4 + val - 3 + val / 2 + val + 3;
    };

    auto node = tg::when_all(tg::when_all([](int val) { return val * 2; },
                                          [](int val) { return val + 4; },
                                          [](int val) { return val - 3; },
                                          [](int val) { return val / 2; },
                                          [](int val) { return val + 3; }),
                             tg::when_all([](int val) { return val * 2; },
                                          [](int val) { return val + 4; },
                                          [](int val) { return val - 3; },
                                          [](int val) { return val / 2; },
                                          [](int val) { return val + 3; }))
                    .then([](auto... vals) {
                        static_assert(sizeof...(vals) == 10);
                        return (... + vals);
                    });

    int result;
    auto setter = [&result](int val) { result = val; };

    tg::sync_execute(&tr, &node, setter, 5);
    REQUIRE(result == 2 * simple_func(5));

    tg::sync_execute(&tr, &node, setter, 25);
    REQUIRE(result == 2 * simple_func(25));

    tg::sync_execute(&tr, &node, setter, 0);
    REQUIRE(result == 2 * simple_func(0));
}
