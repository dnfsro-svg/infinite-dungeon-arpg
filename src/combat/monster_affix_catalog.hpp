#pragma once

#include "combat/combat_types.hpp"
#include "combat/monster_affix_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace arpg::combat {

enum class MonsterAffixDanger : std::uint8_t { low, medium, high };

struct MonsterAffixTierValues final {
    std::int32_t primary_bp{};
    std::int32_t secondary_bp{};
    std::uint16_t interval_ticks{};
    std::uint16_t duration_ticks{};
    std::int32_t damage{};
    float radius{};
    std::uint8_t projectile_count{};
};

struct MonsterAffixDefinition final {
    MonsterAffixId id{MonsterAffixId::mighty};
    std::string_view name{};
    std::string_view short_name{};
    MonsterAffixDanger danger{MonsterAffixDanger::low};
    std::uint16_t weight{};
    std::uint16_t required_tags{};
    std::uint16_t forbidden_tags{};
    std::uint16_t conflict_mask{};
    std::array<MonsterAffixTierValues,
        static_cast<std::size_t>(MonsterAffixTier::count)> tiers{};
};

using MonsterAffixCatalog = std::array<MonsterAffixDefinition,
    static_cast<std::size_t>(MonsterAffixId::count)>;

[[nodiscard]] const MonsterAffixDefinition* monster_affix_definition(
    MonsterAffixId id) noexcept;
[[nodiscard]] const MonsterAffixCatalog& monster_affix_catalog() noexcept;
[[nodiscard]] bool monster_affix_catalog_valid() noexcept;

}  // namespace arpg::combat
