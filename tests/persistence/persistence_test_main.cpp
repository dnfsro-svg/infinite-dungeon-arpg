#include "test_framework.hpp"

arpg::test::TestSuite checkpoint_codec_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        checkpoint_codec_suite(),
    };

    return arpg::test::run_suites(suites, 8, "persistence");
}
