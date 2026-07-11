#include "test_framework.hpp"

arpg::test::TestSuite room_generation_suite() noexcept;
arpg::test::TestSuite dungeon_lifecycle_suite() noexcept;
arpg::test::TestSuite dungeon_navigation_suite() noexcept;
arpg::test::TestSuite dungeon_stress_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        room_generation_suite(),
        dungeon_lifecycle_suite(),
        dungeon_navigation_suite(),
        dungeon_stress_suite(),
    };

    return arpg::test::run_suites(suites, 20, "stage 2 dungeon");
}
