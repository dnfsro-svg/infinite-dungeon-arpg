#include "test_framework.hpp"

arpg::test::TestSuite settings_types_suite() noexcept;
arpg::test::TestSuite settings_codec_suite() noexcept;
arpg::test::TestSuite settings_store_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        settings_types_suite(),
        settings_codec_suite(),
        settings_store_suite()};
    return arpg::test::run_suites(suites, 35, "stage 11b reserved V invariant");
}
