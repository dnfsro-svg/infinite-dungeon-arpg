#include "test_framework.hpp"

arpg::test::TestSuite checkpoint_schema_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        checkpoint_schema_suite(),
    };
    return arpg::test::run_suites(suites, 8, "neutral checkpoint schema");
}
