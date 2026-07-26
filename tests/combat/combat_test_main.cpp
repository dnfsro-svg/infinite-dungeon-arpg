#include "test_framework.hpp"

#include <cstdlib>

arpg::test::TestSuite active_skill_timeline_suite() noexcept;
arpg::test::TestSuite attack_catalog_suite() noexcept;
arpg::test::TestSuite abyss_environment_suite() noexcept;
arpg::test::TestSuite attack_state_suite() noexcept;
arpg::test::TestSuite break_stress_suite() noexcept;
arpg::test::TestSuite combat_config_suite() noexcept;
arpg::test::TestSuite combat_query_suite() noexcept;
arpg::test::TestSuite draw_slash_skill_suite() noexcept;
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
arpg::test::TestSuite storm_swords_skill_suite() noexcept;

namespace {

bool active_skill_timeline_only_enabled() noexcept {
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(
        &value, &length, "ARPG_ACTIVE_SKILL_TIMELINE_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
#else
    return std::getenv("ARPG_ACTIVE_SKILL_TIMELINE_ONLY") != nullptr;
#endif
}

bool draw_slash_only_enabled() noexcept {
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(
        &value, &length, "ARPG_STAGE17_DRAW_SLASH_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
#else
    return std::getenv("ARPG_STAGE17_DRAW_SLASH_ONLY") != nullptr;
#endif
}

bool storm_swords_only_enabled() noexcept {
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(
        &value, &length, "ARPG_STAGE17_STORM_SWORDS_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
#else
    return std::getenv("ARPG_STAGE17_STORM_SWORDS_ONLY") != nullptr;
#endif
}

}  // namespace

int main() {
    if (active_skill_timeline_only_enabled()) {
        const arpg::test::TestSuite active_skill_timeline_suites[] = {
            active_skill_timeline_suite(),
        };
        return arpg::test::run_suites(
            active_skill_timeline_suites, 4, "active skill timeline");
    }

    if (storm_swords_only_enabled()) {
        const arpg::test::TestSuite storm_swords_suites[] = {
            storm_swords_skill_suite(),
        };
        return arpg::test::run_suites(
            storm_swords_suites, 9, "stage 17 task 5 storm swords");
    }

    if (draw_slash_only_enabled()) {
        const arpg::test::TestSuite draw_slash_suites[] = {
            draw_slash_skill_suite(),
        };
        return arpg::test::run_suites(
            draw_slash_suites, 13, "stage 17 task 4 draw slash");
    }

    const arpg::test::TestSuite suites[] = {
        active_skill_timeline_suite(),
        attack_catalog_suite(),
        abyss_environment_suite(),
        attack_state_suite(),
        break_stress_suite(),
        combat_config_suite(),
        combat_query_suite(),
        draw_slash_skill_suite(),
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
        storm_swords_skill_suite(),
    };

    return arpg::test::run_suites(suites, 243, "stage 18 combat queries");
}
