#include "dungeon/health_potion_loot.hpp"

#include "core/deterministic_rng.hpp"

namespace arpg::dungeon {
namespace {

constexpr std::uint64_t kHealthPotionChanceDomain = 0x48505F504F544E31ULL;

}  // namespace

bool roll_health_potion_drop(
    std::uint64_t room_seed, std::uint16_t spawn_ordinal) noexcept {
    auto ordinal = core::DeterministicRng::derive_stream(
        room_seed, static_cast<std::uint64_t>(spawn_ordinal));
    auto chance = core::DeterministicRng::derive_stream(
        ordinal.next_u64(), kHealthPotionChanceDomain);
    return chance.next_bounded(10000U).value_or(9999U)
        < kHealthPotionDropChanceBp;
}

bool health_potion_auto_use_eligible(int hp, int max_hp) noexcept {
    if (hp <= 0 || max_hp <= 0 || hp > max_hp) return false;
    return static_cast<std::int64_t>(hp) * 10000
        <= static_cast<std::int64_t>(max_hp)
            * kHealthPotionAutoUseThresholdBp;
}

}  // namespace arpg::dungeon
