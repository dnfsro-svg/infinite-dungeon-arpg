#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

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

[[nodiscard]] inline bool trace_enabled() noexcept {
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length, "ARPG_TEST_TRACE");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
#else
    return std::getenv("ARPG_TEST_TRACE") != nullptr;
#endif
}

template <std::size_t CaseCount>
[[nodiscard]] constexpr TestSuite make_suite(
    const char* name,
    const TestCase (&cases)[CaseCount]) noexcept {
    return {name, cases, CaseCount};
}

template <std::size_t SuiteCount>
int run_suites(
    const TestSuite (&suites)[SuiteCount],
    int expected_case_count,
    const char* case_count_label) noexcept {
    int failures = 0;
    int checks = 0;
    const bool trace = trace_enabled();
    for (const auto& suite : suites) {
        for (std::size_t index = 0; index < suite.count; ++index) {
            ++checks;
            if (trace) {
                std::fprintf(stderr, "[RUN] %s.%s\n",
                    suite.name, suite.cases[index].name);
                std::fflush(stderr);
            }
            const auto failure = suite.cases[index].function();
            if (failure.expression != nullptr) {
                ++failures;
                std::fprintf(stderr, "[FAIL] %s.%s: %s (%s:%d)\n",
                    suite.name, suite.cases[index].name,
                    failure.expression, failure.file, failure.line);
            }
        }
    }
    if (checks != expected_case_count) {
        ++failures;
        std::fprintf(stderr, "[FAIL] %s case count: actual %d != expected %d\n",
            case_count_label, checks, expected_case_count);
    }
    std::printf("%d cases, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

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
