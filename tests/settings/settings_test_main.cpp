#include "test_framework.hpp"

arpg::test::TestSuite settings_types_suite() noexcept;
arpg::test::TestSuite settings_codec_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        settings_types_suite(),
        settings_codec_suite()};
    return arpg::test::run_suites(suites, 17, "stage 11b task 2 settings codec");
}
