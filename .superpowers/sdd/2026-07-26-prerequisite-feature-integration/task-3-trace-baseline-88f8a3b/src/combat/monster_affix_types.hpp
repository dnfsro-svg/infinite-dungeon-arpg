#pragma once

#include <array>
#include <cstdint>

namespace arpg::combat {

enum class MonsterAffixId : std::uint8_t {
    mighty, frenzy, swift, armored, shielding, multishot, burning_ground,
    chilling, chain_lightning, chaos_corrosion, blink_assault, death_blast,
    count,
};

enum class MonsterAffixTier : std::uint8_t { m1, m2, m3, count };

struct MonsterAffixInstance final {
    MonsterAffixId id{MonsterAffixId::mighty};
    MonsterAffixTier tier{MonsterAffixTier::m1};

    friend bool operator==(
        MonsterAffixInstance left, MonsterAffixInstance right) noexcept {
        return left.id == right.id && left.tier == right.tier;
    }
};

struct MonsterAffixSet final {
    std::array<MonsterAffixInstance, 3> values{};
    std::uint8_t count{};

    friend bool operator==(
        const MonsterAffixSet& left, const MonsterAffixSet& right) noexcept {
        return left.count == right.count && left.values == right.values;
    }
};

}  // namespace arpg::combat
