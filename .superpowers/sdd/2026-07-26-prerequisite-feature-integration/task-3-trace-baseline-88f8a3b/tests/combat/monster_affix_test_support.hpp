#pragma once

#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_affix_types.hpp"
#include "combat/combat_types.hpp"
#include "dungeon/encounter_director.hpp"

#include <cstdint>
#include <optional>

namespace arpg::combat::test_support {

[[nodiscard]] bool monster_affix_catalog_valid(
    const MonsterAffixCatalog& catalog) noexcept;
[[nodiscard]] std::optional<MonsterAffixSet>
generate_monster_affixes_with_catalog(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index,
    const MonsterDefinition& monster,
    const MonsterAffixCatalog& catalog) noexcept;
[[nodiscard]] std::optional<MonsterAffixSet>
supplement_abyss_affixes_with_catalog(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index,
    const MonsterDefinition& monster, MonsterAffixSet normal,
    const MonsterAffixCatalog& catalog) noexcept;
[[nodiscard]] std::uint64_t monster_affix_context_seed(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index) noexcept;
[[nodiscard]] std::uint64_t monster_affix_count_seed(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index) noexcept;
[[nodiscard]] std::uint64_t monster_affix_selection_seed(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index) noexcept;
[[nodiscard]] std::uint64_t monster_affix_tier_seed(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index) noexcept;

}  // namespace arpg::combat::test_support

namespace arpg::dungeon::test_support {

[[nodiscard]] bool encounter_plan_legal_with_affix_catalog(
    const RoomEncounterPlan& plan,
    const EncounterDirectorConfig& config,
    const combat::MonsterAffixCatalog& catalog) noexcept;

}  // namespace arpg::dungeon::test_support
