#include "test_framework.hpp"

#include <cstdlib>

#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#include <cstdlib>
#endif

arpg::test::TestSuite room_generation_suite() noexcept;
arpg::test::TestSuite death_checkpoint_suite() noexcept;
arpg::test::TestSuite dungeon_lifecycle_suite() noexcept;
arpg::test::TestSuite dungeon_navigation_suite() noexcept;
arpg::test::TestSuite dungeon_stress_suite() noexcept;
arpg::test::TestSuite dungeon_rules_suite() noexcept;
arpg::test::TestSuite dungeon_progression_suite() noexcept;
arpg::test::TestSuite dungeon_passive_tree_suite() noexcept;
arpg::test::TestSuite dungeon_item_transaction_suite() noexcept;
arpg::test::TestSuite dungeon_equipment_stress_suite() noexcept;
arpg::test::TestSuite dungeon_loot_drop_suite() noexcept;
arpg::test::TestSuite dungeon_transaction_suite() noexcept;
arpg::test::TestSuite dungeon_abyss_reward_suite() noexcept;
arpg::test::TestSuite dungeon_abyss_stress_suite() noexcept;
arpg::test::TestSuite encounter_director_suite() noexcept;
arpg::test::TestSuite dungeon_wave_suite() noexcept;
arpg::test::TestSuite dungeon_progression_reward_suite() noexcept;
arpg::test::TestSuite dungeon_affix_reward_suite() noexcept;
arpg::test::TestSuite dungeon_affix_stress_suite() noexcept;

namespace {

bool stage9_affix_stress_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_STAGE9_AFFIX_STRESS_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool stage10_abyss_reward_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_STAGE10_ABYSS_REWARD_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool stage10_abyss_stress_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_STAGE10_ABYSS_STRESS_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

}  // namespace

int main() {
#if defined(_WIN32) && defined(_DEBUG)
    _set_error_mode(_OUT_TO_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(_WRITE_ABORT_MSG,
        _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    const arpg::test::TestSuite suites[] = {
        room_generation_suite(),
        death_checkpoint_suite(),
        dungeon_lifecycle_suite(),
        dungeon_navigation_suite(),
        dungeon_stress_suite(),
        dungeon_rules_suite(),
        dungeon_progression_suite(),
        dungeon_passive_tree_suite(),
        dungeon_item_transaction_suite(),
        dungeon_equipment_stress_suite(),
        dungeon_loot_drop_suite(),
        dungeon_transaction_suite(),
        dungeon_abyss_reward_suite(),
        dungeon_abyss_stress_suite(),
        encounter_director_suite(),
        dungeon_wave_suite(),
        dungeon_progression_reward_suite(),
        dungeon_affix_reward_suite(),
        dungeon_affix_stress_suite(),
    };

    if (stage9_affix_stress_only()) {
        const arpg::test::TestSuite stress_only[] = {
            dungeon_affix_stress_suite(),
        };
        return arpg::test::run_suites(stress_only, 2,
            "stage 9 task 10 affix stress");
    }

    if (stage10_abyss_reward_only()) {
        const arpg::test::TestSuite reward_only[] = {
            dungeon_abyss_reward_suite(),
        };
        return arpg::test::run_suites(reward_only, 29,
            "stage 10 task 9 abyss rewards");
    }


    if (stage10_abyss_stress_only()) {
        const arpg::test::TestSuite stress_only[] = {
            dungeon_abyss_stress_suite(),
        };
        return arpg::test::run_suites(stress_only, 2,
            "stage 10 task 12 abyss stress");
    }

    return arpg::test::run_suites(suites, 223,
        "stage 10 task 12 abyss stress validation");
}
