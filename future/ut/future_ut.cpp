#include <iostream>
#include <sstream>

#include "catch2/catch_all.hpp"
#include "future/future.h"

TEST_CASE("test_then", "[advanced_future]") {
    auto future = my_async([](int a, int b) { return a + b; }, 3, 4)
                      .then([](int c, int d) { return c * d; }, 5);
    REQUIRE(future.get() == (3 + 4) * 5);
}

TEST_CASE("test_contraction", "[advanced_future]") {
    auto val =
        contraction(
            [](int a, int b, int c) { return a + b * c; },
            my_async([] { return 2; }),
            contraction([](int a, int b) { return a + b; },
                        my_async([](int a, int b) { return a * b; }, 5, 6),
                        my_async([] { return 2; })),
            my_async([] { return 2; }))
            .get();
    REQUIRE(val == 2 + (5 * 6 + 2) * 2);
}

TEST_CASE("test_validity", "[advanced_future]") {
    auto f1 = my_async([](int a, int b) { return a * b + 5; }, 2, 3);
    auto f2 = f1.then([](int a, int b) { return a + b; }, 25);
    REQUIRE(!f1.valid());
    REQUIRE(f2.get() == 2 * 3 + 5 + 25);
}

TEST_CASE("test_void_future", "[advanced_future]") {
    std::ostringstream os;
    auto f1 =
        my_async([&os](int a, int b) { os << a * b << std::endl; }, 2, 3)
            .then([&os](int a, int b) { os << a + b << std::endl; }, 4, 5);
    f1.get();
    REQUIRE(os.str() == "6\n9\n");
}

TEST_CASE("test_long_chain", "[advanced_future]") {
    auto f = my_async([] { return 1; })
                 .then([](auto a) { return a * 2; })
                 .then([](auto a) { return a + 3; })
                 .then([](auto a) { return a * a; });
    REQUIRE(f.get() == ((1 * 2) + 3) * ((1 * 2) + 3));
}
