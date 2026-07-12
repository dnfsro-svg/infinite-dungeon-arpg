#include "test_framework.hpp"

arpg::test::TestSuite checkpoint_codec_suite() noexcept;
arpg::test::TestSuite save_store_suite() noexcept;
arpg::test::TestSuite save_store_fault_suite() noexcept;
arpg::test::TestSuite dungeon_save_integration_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        checkpoint_codec_suite(),
        save_store_suite(),
        save_store_fault_suite(),
        dungeon_save_integration_suite(),
    };

    return arpg::test::run_suites(suites, 30, "stage 5 persistence");
}
