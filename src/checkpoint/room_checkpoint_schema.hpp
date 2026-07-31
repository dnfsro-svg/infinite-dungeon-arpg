#pragma once

#include "core/gameplay_limits.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::checkpoint {

inline constexpr std::size_t kRoomEquipmentGroundCapacity =
    limits::kRoomMonsterCapacity;
inline constexpr std::size_t kRoomSecondaryGroundCapacity =
    limits::kRoomMonsterCapacity * 2U + 16U;

inline constexpr std::uint16_t kAbyssMaterialOrdinalBegin = 384U;
inline constexpr std::uint16_t kOrdinarySecondaryOrdinalEnd = 768U;
inline constexpr std::uint16_t kAbyssSecondaryOrdinalBegin = 2304U;
inline constexpr std::size_t kHealthPotionGroundCapacity = 192U;

inline constexpr std::size_t kMonsterOrdinalWordCount =
    (limits::kRoomMonsterCapacity + 63U) / 64U;
inline constexpr std::size_t kRoomEnvironmentCellCount = 400U;
inline constexpr std::size_t kFireRoomCrateCapacity = 2U;
inline constexpr std::size_t kPlayerDamageHistoryTicks = 300U;

inline constexpr float kRoomHalfExtent = 81.24038696F;
inline constexpr float kRoomMinX = -kRoomHalfExtent;
inline constexpr float kRoomMaxX = kRoomHalfExtent;
inline constexpr float kRoomMinY = -kRoomHalfExtent;
inline constexpr float kRoomMaxY = kRoomHalfExtent;
inline constexpr float kRoomWidth = kRoomMaxX - kRoomMinX;
inline constexpr float kRoomDepth = kRoomMaxY - kRoomMinY;
inline constexpr float kRoomMinZ = 0.0F;
inline constexpr float kRoomMaxZ = 32.0F;

inline constexpr std::array<std::uint16_t, 5U> kAttackActiveTicks{{
    3U,
    3U,
    4U,
    4U,
    5U,
}};

struct CheckpointAffixTiming final {
    std::uint16_t interval_ticks{};
    std::uint16_t duration_ticks{};
};

inline constexpr std::array<CheckpointAffixTiming, 3U>
    kBurningGroundTimings{{
        {180U, 120U},
        {150U, 180U},
        {120U, 240U},
    }};

inline constexpr std::array<CheckpointAffixTiming, 3U>
    kBlinkAssaultTimings{{
        {480U, 42U},
        {360U, 36U},
        {240U, 30U},
    }};

[[nodiscard]] constexpr std::uint16_t checkpoint_material_ordinal(
    const std::uint16_t material_ordinal) noexcept {
    return material_ordinal >= kAbyssMaterialOrdinalBegin
        ? static_cast<std::uint16_t>(kAbyssSecondaryOrdinalBegin
            + material_ordinal - kAbyssMaterialOrdinalBegin)
        : static_cast<std::uint16_t>(material_ordinal * 2U);
}

[[nodiscard]] constexpr std::uint16_t material_ordinal_from_checkpoint(
    const std::uint16_t checkpoint_ordinal) noexcept {
    if (checkpoint_ordinal >= kAbyssSecondaryOrdinalBegin) {
        return static_cast<std::uint16_t>(kAbyssMaterialOrdinalBegin
            + checkpoint_ordinal - kAbyssSecondaryOrdinalBegin);
    }
    return checkpoint_ordinal < kOrdinarySecondaryOrdinalEnd
        ? static_cast<std::uint16_t>(checkpoint_ordinal / 2U)
        : std::uint16_t{0xFFFFU};
}

[[nodiscard]] constexpr std::uint16_t health_potion_claim_ordinal(
    const std::uint16_t spawn_ordinal) noexcept {
    return static_cast<std::uint16_t>(spawn_ordinal * 2U + 1U);
}

[[nodiscard]] constexpr std::uint32_t required_kills(
    const std::uint32_t generated_monsters) noexcept {
    return (generated_monsters + 3U) / 4U;
}

static_assert(kRoomSecondaryGroundCapacity == 2320U);
static_assert(kMonsterOrdinalWordCount == limits::kRoomEquipmentClaimWords);
static_assert(kRoomEnvironmentCellCount == 400U);
static_assert(kFireRoomCrateCapacity == 2U);
static_assert(kRoomWidth == kRoomDepth);

}  // namespace arpg::checkpoint
