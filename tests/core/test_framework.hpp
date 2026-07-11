#pragma once

#include <cmath>
#include <cstddef>

namespace arpg::test {

struct Failure final {
    const char* expression{};
    const char* file{};
    int line{};
};

using TestFunction = Failure (*)() noexcept;

struct TestCase final {
    const char* name;
    TestFunction function;
};

struct TestSuite final {
    const char* name;
    const TestCase* cases;
    std::size_t count;
};

[[nodiscard]] inline bool near(
    double lhs,
    double rhs,
    double tolerance = 1.0e-12) noexcept {
    return std::fabs(lhs - rhs) <= tolerance;
}

}  // namespace arpg::test

#define ARPG_REQUIRE(expression)                                      \
    do {                                                              \
        if (!(expression)) {                                          \
            return ::arpg::test::Failure{#expression, __FILE__, __LINE__}; \
        }                                                             \
    } while (false)
