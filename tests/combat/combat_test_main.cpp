#include "test_framework.hpp"

arpg::test::TestSuite attack_catalog_suite() noexcept;
arpg::test::TestSuite abyss_environment_suite() noexcept;
arpg::test::TestSuite attack_state_suite() noexcept;
arpg::test::TestSuite break_stress_suite() noexcept;
arpg::test::TestSuite combat_config_suite() noexcept;
arpg::test::TestSuite dummy_reaction_suite() noexcept;
arpg::test::TestSuite input_buffer_suite() noexcept;
arpg::test::TestSuite movement_jump_suite() noexcept;
arpg::test::TestSuite hit_resolution_suite() noexcept;
arpg::test::TestSuite monster_catalog_suite() noexcept;
arpg::test::TestSuite monster_affix_catalog_suite() noexcept;
arpg::test::TestSuite monster_affix_generation_suite() noexcept;
arpg::test::TestSuite monster_affix_runtime_suite() noexcept;
arpg::test::TestSuite monster_affix_trigger_suite() noexcept;
arpg::test::TestSuite monster_melee_suite() noexcept;
arpg::test::TestSuite monster_ranged_suite() noexcept;
arpg::test::TestSuite monster_special_suite() noexcept;
arpg::test::TestSuite monster_pool_suite() noexcept;
arpg::test::TestSuite player_health_suite() noexcept;
arpg::test::TestSuite player_build_suite() noexcept;
arpg::test::TestSuite player_damage_history_suite() noexcept;
arpg::test::TestSuite player_death_snapshot_suite() noexcept;
arpg::test::TestSuite player_defense_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        attack_catalog_suite(),
        abyss_environment_suite(),
        attack_state_suite(),
        break_stress_suite(),
        combat_config_suite(),
        dummy_reaction_suite(),
        input_buffer_suite(),
        movement_jump_suite(),
        hit_resolution_suite(),
        monster_catalog_suite(),
        monster_affix_catalog_suite(),
        monster_affix_generation_suite(),
        monster_affix_runtime_suite(),
        monster_affix_trigger_suite(),
        monster_melee_suite(),
        monster_ranged_suite(),
        monster_special_suite(),
        monster_pool_suite(),
        player_health_suite(),
        player_build_suite(),
        player_damage_history_suite(),
        player_death_snapshot_suite(),
        player_defense_suite(),
    };

    return arpg::test::run_suites(suites, 203, "stage 11-a task 2 death snapshot");
}
