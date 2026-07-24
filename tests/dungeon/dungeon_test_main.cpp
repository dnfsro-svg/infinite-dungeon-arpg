#include "test_framework.hpp"

#include <cstdlib>

#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#include <cstdlib>
#endif

arpg::test::TestSuite room_generation_suite() noexcept;
arpg::test::TestSuite death_checkpoint_suite() noexcept;
arpg::test::TestSuite dungeon_death_lifecycle_suite() noexcept;
arpg::test::TestSuite dungeon_death_stress_suite() noexcept;
arpg::test::TestSuite dungeon_lifecycle_suite() noexcept;
arpg::test::TestSuite dungeon_navigation_suite() noexcept;
arpg::test::TestSuite dungeon_stress_suite() noexcept;
arpg::test::TestSuite dungeon_rules_suite() noexcept;
arpg::test::TestSuite dungeon_progression_suite() noexcept;
arpg::test::TestSuite dungeon_query_suite() noexcept;
arpg::test::TestSuite dungeon_passive_tree_suite() noexcept;
arpg::test::TestSuite dungeon_item_transaction_suite() noexcept;
arpg::test::TestSuite dungeon_crafting_transaction_suite() noexcept;
arpg::test::TestSuite dungeon_skill_loadout_transaction_suite() noexcept;
arpg::test::TestSuite dungeon_skill_cast_suite() noexcept;
arpg::test::TestSuite dungeon_equipment_stress_suite() noexcept;
arpg::test::TestSuite dungeon_loot_drop_suite() noexcept;
arpg::test::TestSuite dungeon_material_loot_suite() noexcept;
arpg::test::TestSuite dungeon_health_potion_suite() noexcept;
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

bool stage8_equipment_stress_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_STAGE8_EQUIPMENT_STRESS_ONLY");
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

bool stage11_death_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_STAGE11_DEATH_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool stage11_death_stress_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_STAGE11_DEATH_STRESS_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool stage16_material_loot_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_STAGE16_MATERIAL_LOOT_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool stage16_crafting_transaction_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_STAGE16_CRAFTING_TRANSACTION_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool health_potion_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_HEALTH_POTION_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool stage17_skill_loadout_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_STAGE17_SKILL_LOADOUT_ONLY");
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
        dungeon_death_lifecycle_suite(),
        dungeon_lifecycle_suite(),
        dungeon_navigation_suite(),
        dungeon_stress_suite(),
        dungeon_rules_suite(),
        dungeon_progression_suite(),
        dungeon_query_suite(),
        dungeon_passive_tree_suite(),
        dungeon_item_transaction_suite(),
        dungeon_crafting_transaction_suite(),
        dungeon_skill_loadout_transaction_suite(),
        dungeon_skill_cast_suite(),
        dungeon_equipment_stress_suite(),
        dungeon_loot_drop_suite(),
        dungeon_material_loot_suite(),
        dungeon_health_potion_suite(),
        dungeon_transaction_suite(),
        dungeon_abyss_reward_suite(),
        dungeon_abyss_stress_suite(),
        encounter_director_suite(),
        dungeon_wave_suite(),
        dungeon_progression_reward_suite(),
        dungeon_affix_reward_suite(),
        dungeon_affix_stress_suite(),
    };

    if (stage8_equipment_stress_only()) {
        const arpg::test::TestSuite stress_only[] = {
            dungeon_equipment_stress_suite(),
        };
        return arpg::test::run_suites(stress_only, 2,
            "stage 8 equipment stress");
    }

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

    if (stage11_death_only()) {
        const arpg::test::TestSuite death_only[] = {
            dungeon_death_lifecycle_suite(),
        };
        return arpg::test::run_suites(death_only, 31,
            "stage 11a task 8 focused death lifecycle");
    }

    if (stage11_death_stress_only()) {
        const arpg::test::TestSuite stress_only[] = {
            dungeon_death_stress_suite(),
        };
        return arpg::test::run_suites(stress_only, 2,
            "stage 11a task 11 death stress");
    }

    if (stage16_material_loot_only()) {
        const arpg::test::TestSuite material_only[] = {
            dungeon_material_loot_suite(),
        };
        return arpg::test::run_suites(material_only, 12,
            "stage 16 task 4 material loot");
    }

    if (stage16_crafting_transaction_only()) {
        const arpg::test::TestSuite crafting_only[] = {
            dungeon_crafting_transaction_suite(),
        };
        return arpg::test::run_suites(crafting_only, 6,
            "stage 16 task 7 crafting and reinforcement transactions");
    }

    if (health_potion_only()) {
        const arpg::test::TestSuite potion_only[] = {
            dungeon_health_potion_suite(),
        };
        return arpg::test::run_suites(potion_only, 7,
            "task 5 health potion ground drops");
    }

    if (stage17_skill_loadout_only()) {
        const arpg::test::TestSuite loadout_only[] = {
            dungeon_skill_loadout_transaction_suite(),
        };
        return arpg::test::run_suites(loadout_only, 9,
            "stage 17 task 3 skill loadout transactions");
    }

    return arpg::test::run_suites(suites, 294,
        "stage 18 dungeon queries");
}
