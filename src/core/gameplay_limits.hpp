#pragma once

#include <cstddef>

namespace arpg::limits {

inline constexpr std::size_t kRoomMonsterCapacity = 1152U;
inline constexpr std::size_t kActiveMonsterCapacity = 128U;
inline constexpr std::size_t kProjectileCapacity = 512U;
inline constexpr std::size_t kHazardCapacity = 128U;
inline constexpr std::size_t kCombatEventCapacity = 512U;
inline constexpr std::size_t kDefeatLedgerCapacity = 128U;
inline constexpr std::size_t kRoomEquipmentClaimWords = 18U;
inline constexpr std::size_t kRoomSecondaryClaimWords = 37U;

static_assert((kRoomMonsterCapacity + 63U) / 64U
    == kRoomEquipmentClaimWords);
static_assert((kRoomMonsterCapacity * 2U + 16U + 63U) / 64U
    == kRoomSecondaryClaimWords);

}  // namespace arpg::limits
