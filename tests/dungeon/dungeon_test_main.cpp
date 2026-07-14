#include "test_framework.hpp"

arpg::test::TestSuite room_generation_suite() noexcept;
arpg::test::TestSuite dungeon_lifecycle_suite() noexcept;
arpg::test::TestSuite dungeon_navigation_suite() noexcept;
arpg::test::TestSuite dungeon_stress_suite() noexcept;
arpg::test::TestSuite dungeon_rules_suite() noexcept;
arpg::test::TestSuite dungeon_progression_suite() noexcept;
arpg::test::TestSuite dungeon_passive_tree_suite() noexcept;
arpg::test::TestSuite dungeon_transaction_suite() noexcept;
arpg::test::TestSuite encounter_director_suite() noexcept;
arpg::test::TestSuite dungeon_wave_suite() noexcept;
arpg::test::TestSuite dungeon_progression_reward_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        room_generation_suite(),
        dungeon_lifecycle_suite(),
        dungeon_navigation_suite(),
        dungeon_stress_suite(),
        dungeon_rules_suite(),
        dungeon_progression_suite(),
        dungeon_passive_tree_suite(),
        dungeon_transaction_suite(),
        encounter_director_suite(),
        dungeon_wave_suite(),
        dungeon_progression_reward_suite(),
    };

    return arpg::test::run_suites(suites, 76, "stage 7 task 5 dungeon");
}
