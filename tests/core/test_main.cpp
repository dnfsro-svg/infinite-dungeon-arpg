#include "test_framework.hpp"

arpg::test::TestSuite fixed_step_suite() noexcept;
arpg::test::TestSuite deterministic_rng_suite() noexcept;
arpg::test::TestSuite fixed_pool_suite() noexcept;
arpg::test::TestSuite bounded_queue_suite() noexcept;
arpg::test::TestSuite allocation_probe_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        fixed_step_suite(),
        deterministic_rng_suite(),
        fixed_pool_suite(),
        bounded_queue_suite(),
        allocation_probe_suite(),
    };

    return arpg::test::run_suites(suites, 28, "checkpoint v9 core");
}
