#pragma once

#include "checkpoint/room_checkpoint_schema.hpp"
#include "combat/combat_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::dungeon {

inline constexpr std::uint16_t kHealthPotionDropChanceBp = 3000U;
inline constexpr std::uint16_t kHealthPotionRestoreBp = 2500U;
inline constexpr std::uint16_t kHealthPotionAutoUseThresholdBp = 7500U;
inline constexpr std::size_t kGroundHealthPotionCapacity =
    ::arpg::checkpoint::kHealthPotionGroundCapacity;
inline constexpr std::size_t kPendingHealthPotionClaimCapacity = 4U;

static_assert(kGroundHealthPotionCapacity
    == ::arpg::checkpoint::kHealthPotionGroundCapacity);

[[nodiscard]] constexpr std::uint16_t health_potion_claim_ordinal(
    std::uint16_t spawn_ordinal) noexcept {
    return ::arpg::checkpoint::health_potion_claim_ordinal(spawn_ordinal);
}

struct GroundHealthPotion final {
    bool active{};
    std::uint16_t spawn_ordinal{};
    std::uint16_t claim_ordinal{};
    combat::Vec3 position{};
};

struct GroundHealthPotionSnapshot final {
    std::uint16_t spawn_ordinal{};
    std::uint16_t claim_ordinal{};
    combat::Vec3 position{};
};

struct HealthPotionPickupReceipt final {
    bool valid{};
    bool room_clear{};
    std::uint16_t consumed_count{};
    std::uint64_t commit_generation{};
    int restored_hp{};
};

struct PendingHealthPotionClaim final {
    std::array<std::uint16_t, kPendingHealthPotionClaimCapacity> spawn_ordinals{};
    std::uint8_t count{};
    int expected_hp{};
    int expected_max_hp{};
};

[[nodiscard]] bool roll_health_potion_drop(
    std::uint64_t room_seed, std::uint16_t spawn_ordinal) noexcept;
[[nodiscard]] bool health_potion_auto_use_eligible(
    int hp, int max_hp) noexcept;

}  // namespace arpg::dungeon
