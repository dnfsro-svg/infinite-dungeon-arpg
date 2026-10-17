#include "dungeon/health_potion_loot.hpp"
#include "core/deterministic_rng.hpp"

#include <cstdint>
#include <cstdio>

std::uint64_t raw_potion_roll(
    std::uint64_t seed, std::uint16_t spawn_ordinal) noexcept {
    constexpr std::uint64_t kDomain = 0x48505F504F544E31ULL;
    auto ordinal = arpg::core::DeterministicRng::derive_stream(
        seed, static_cast<std::uint64_t>(spawn_ordinal));
    auto chance = arpg::core::DeterministicRng::derive_stream(
        ordinal.next_u64(), kDomain);
    return chance.next_bounded(10000U).value_or(9999U);
}

int main() {
    using namespace arpg::dungeon;
    if (kHealthPotionDropChanceBp != 3000U
            || kHealthPotionRestoreBp != 2500U
            || kHealthPotionAutoUseThresholdBp != 7500U
            || kGroundHealthPotionCapacity != 192U
            || health_potion_claim_ordinal(0U) != 1U
            || health_potion_claim_ordinal(191U) != 383U) {
        return 1;
    }
    if (!health_potion_auto_use_eligible(750, 1000)
            || health_potion_auto_use_eligible(751, 1000)
            || !health_potion_auto_use_eligible(3, 4)
            || health_potion_auto_use_eligible(4, 4)) {
        return 2;
    }
    bool found_drop = false;
    bool found_miss = false;
    std::uint64_t seed_2999 = 0U;
    std::uint64_t seed_3000 = 0U;
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        const bool first = roll_health_potion_drop(seed, 37U);
        const bool second = roll_health_potion_drop(seed, 37U);
        if (first != second) return 3;
        found_drop = found_drop || first;
        found_miss = found_miss || !first;
    }
    for (std::uint64_t seed = 0U;
         seed < 1000000U && (seed_2999 == 0U || seed_3000 == 0U); ++seed) {
        const std::uint64_t raw = raw_potion_roll(seed, 37U);
        if (raw == 2999U) seed_2999 = seed;
        if (raw == 3000U) seed_3000 = seed;
    }
    std::printf("seed2999=%llu seed3000=%llu raw0=%llu raw1=%llu raw2=%llu raw3=%llu\n",
        static_cast<unsigned long long>(seed_2999),
        static_cast<unsigned long long>(seed_3000),
        static_cast<unsigned long long>(raw_potion_roll(0U, 37U)),
        static_cast<unsigned long long>(raw_potion_roll(1U, 37U)),
        static_cast<unsigned long long>(raw_potion_roll(2U, 37U)),
        static_cast<unsigned long long>(raw_potion_roll(3U, 37U)));
    if (seed_2999 == 0U || seed_3000 == 0U) return 5;
    return found_drop && found_miss ? 0 : 4;
}
