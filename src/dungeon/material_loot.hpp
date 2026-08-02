#pragma once

#include "abyss/abyss_types.hpp"
#include "checkpoint/room_checkpoint_schema.hpp"
#include "combat/combat_types.hpp"
#include "items/material_catalog.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::dungeon {

inline constexpr std::size_t kGroundMaterialCapacity = 400U;
inline constexpr std::size_t kMaterialDropBitWordCount = 7U;
inline constexpr std::uint16_t kAbyssMaterialOrdinalBegin =
    ::arpg::checkpoint::kAbyssMaterialOrdinalBegin;
inline constexpr std::uint16_t kCheckpointOrdinarySecondaryOrdinalEnd =
    ::arpg::checkpoint::kOrdinarySecondaryOrdinalEnd;
inline constexpr std::uint16_t kCheckpointAbyssSecondaryOrdinalBegin =
    ::arpg::checkpoint::kAbyssSecondaryOrdinalBegin;

static_assert(kAbyssMaterialOrdinalBegin
    == ::arpg::checkpoint::kAbyssMaterialOrdinalBegin);
static_assert(kCheckpointOrdinarySecondaryOrdinalEnd
    == ::arpg::checkpoint::kOrdinarySecondaryOrdinalEnd);
static_assert(kCheckpointAbyssSecondaryOrdinalBegin
    == ::arpg::checkpoint::kAbyssSecondaryOrdinalBegin);

[[nodiscard]] constexpr bool legacy_secondary_claim_representable(
    const std::uint16_t ordinal) noexcept {
    return ordinal < kGroundMaterialCapacity;
}

static_assert(legacy_secondary_claim_representable(399U));
static_assert(!legacy_secondary_claim_representable(400U));

[[nodiscard]] constexpr std::uint16_t checkpoint_material_ordinal(
    const std::uint16_t material_ordinal) noexcept {
    return ::arpg::checkpoint::checkpoint_material_ordinal(material_ordinal);
}

[[nodiscard]] constexpr std::uint16_t material_ordinal_from_checkpoint(
    const std::uint16_t checkpoint_ordinal) noexcept {
    return ::arpg::checkpoint::material_ordinal_from_checkpoint(
        checkpoint_ordinal);
}

enum class GroundMaterialSource : std::uint8_t {
    monster_common,
    monster_coupon,
    abyss_reward,
};

struct GroundMaterial final {
    bool active{};
    std::uint16_t ordinal{};
    GroundMaterialSource source{GroundMaterialSource::monster_common};
    combat::Vec3 position{};
    items::MaterialId material{items::MaterialId::count};
};

struct GroundMaterialSnapshot final {
    std::uint16_t ordinal{};
    GroundMaterialSource source{GroundMaterialSource::monster_common};
    combat::Vec3 position{};
    items::MaterialId material{items::MaterialId::count};
};

[[nodiscard]] std::uint16_t material_drop_chance_bp(
    std::uint16_t danger_score) noexcept;
[[nodiscard]] bool coupon_eligible(
    items::MaterialId coupon,
    std::uint64_t depth,
    std::uint16_t danger_score) noexcept;
[[nodiscard]] std::optional<items::MaterialId> roll_material_drop(
    std::uint64_t room_seed,
    std::uint16_t monster_spawn_ordinal,
    std::uint64_t depth,
    std::uint16_t danger_score) noexcept;
[[nodiscard]] std::optional<items::MaterialId> roll_coupon_drop(
    std::uint64_t room_seed,
    std::uint16_t monster_spawn_ordinal,
    std::uint64_t depth,
    std::uint16_t danger_score,
    bool abyss_monster) noexcept;
[[nodiscard]] std::uint8_t abyss_material_reward_count(
    abyss::AbyssDanger danger) noexcept;
[[nodiscard]] std::optional<items::MaterialId> roll_abyss_material(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint8_t reward_ordinal) noexcept;

}  // namespace arpg::dungeon
