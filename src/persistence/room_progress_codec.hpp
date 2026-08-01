#pragma once

#include "checkpoint/room_progress_checkpoint.hpp"
#include "persistence/checkpoint_codec.hpp"

#include <cstddef>
#include <cstdint>

namespace arpg::persistence {

inline constexpr std::size_t kMaximumEncodedCheckpointBytes =
    8U * 1024U * 1024U;
inline constexpr std::uint32_t kCheckpointFormatVersionV9 = 9U;

// Exact worst-case size derived from the field-wise V9 schema. Counted records
// use their declared fixed capacities; the embedded durable image uses the V8
// maximum inventory cardinality.
inline constexpr std::size_t kV9VecBytes = 3U * sizeof(std::uint32_t);
inline constexpr std::size_t kV9SourceBytes = 1U + 1U + 2U;
inline constexpr std::size_t kV9ModifierBytes =
    4U + 2U + 1U + 8U + 8U + 8U + 8U + 2U + 2U;
inline constexpr std::size_t kV9ActiveEffectBytes =
    4U + 4U + 1U + 1U + 1U + 8U + kV9ModifierBytes
        + 1U + 1U + 8U + 1U;
inline constexpr std::size_t kV9EffectCommandBytes = 1U + 8U + 4U;
inline constexpr std::size_t kV9EffectSetBytes =
    modifiers::kEffectCapacity * kV9ActiveEffectBytes
        + modifiers::kEffectCommandCapacity * kV9EffectCommandBytes
        + 1U + 1U + 4U + 4U;
inline constexpr std::size_t kV9PlayerBytes =
    2U * kV9VecBytes + 4U + 4U * 4U
        + 2U * modifiers::kElementCount * 4U + 2U * 8U + 2U * 4U
        + 2U * 2U + 4U + 2U + 4U + 2U + 1U + kV9SourceBytes
        + skills::kActiveSkillCount * 2U;
inline constexpr std::size_t kV9AttackBytes =
    1U + 3U * 2U + 2U
        + checkpoint::kMonsterOrdinalWordCount * sizeof(std::uint64_t);
inline constexpr std::size_t kV9MonsterBytes =
    2U + 1U + (1U + 3U * 2U)
        + (3U * 4U + 2U + 5U * 4U) + 1U
        + 3U * kV9VecBytes + 3U + 2U + 1U + 2U + 4U + 1U
        + 6U * 4U + 7U * 2U + 1U + 2U * kV9VecBytes + 1U + 2U
        + kV9EffectSetBytes + 1U;
inline constexpr std::size_t kV9ObstacleBytes =
    3U * 2U + 8U + 1U + kV9EffectSetBytes;
inline constexpr std::size_t kV9FireCrateBytes = kV9VecBytes + 8U + 1U;
inline constexpr std::size_t kV9HistoryBytes =
    1U + 8U + checkpoint::kPlayerDamageHistoryTicks
        * modifiers::kDamageTypeCount * sizeof(std::uint64_t);
inline constexpr std::size_t kV9AbyssRuntimeBytes =
    1U + 2U + 2U + kV9VecBytes + 1U + 1U + 1U;
inline constexpr std::size_t kV9DefenseBytes =
    4U * 4U + 2U * 8U + 2U * 4U
        + 2U * modifiers::kElementCount * 4U;
inline constexpr std::size_t kV9DeathBytes =
    8U + kV9SourceBytes + 1U + 4U * 8U
        + modifiers::kDamageTypeCount * 8U + kV9DefenseBytes;
inline constexpr std::size_t kV9CombatBytes =
    8U + 4U * 8U + kV9PlayerBytes + kV9AbyssRuntimeBytes
        + kV9AttackBytes + 2U
        + limits::kRoomMonsterCapacity * kV9MonsterBytes + 2U
        + checkpoint::kRoomEnvironmentCellCount * kV9ObstacleBytes + 1U
        + checkpoint::kFireRoomCrateCapacity * kV9FireCrateBytes
        + kV9HistoryBytes + 1U + kV9DeathBytes;
inline constexpr std::size_t kV9RoomPrefixBytes =
    1U + 2U * 8U + 2U * (4U + 8U) + 3U * 4U + 3U
        + (2U * limits::kRoomEquipmentClaimWords
            + limits::kRoomSecondaryClaimWords) * sizeof(std::uint64_t);
inline constexpr std::size_t kV9EquipmentGroundBytes =
    2U + 1U + 1U + kV9VecBytes + kV7ItemRecordSize;
inline constexpr std::size_t kV9SecondaryGroundBytes =
    1U + 2U + 1U + kV9VecBytes + 1U;
inline constexpr std::size_t kV9MaximumRoomProgressBytes =
    kV9RoomPrefixBytes + kV9CombatBytes + 2U
        + limits::kRoomMonsterCapacity * kV9EquipmentGroundBytes + 2U
        + checkpoint::kRoomSecondaryGroundCapacity
            * kV9SecondaryGroundBytes;
inline constexpr std::size_t kV8MaximumEncodedBytes =
    kV8BaseEncodedCheckpointSize
        + kMaximumCheckpointItemCount * kV7ItemRecordSize;
inline constexpr std::size_t kV9MaximumEncodedBytes =
    32U + 4U + kV8MaximumEncodedBytes + kV9MaximumRoomProgressBytes + 1U;
static_assert(kV9MaximumEncodedBytes <= kMaximumEncodedCheckpointBytes);

[[nodiscard]] CodecError encode_checkpoint_v9_into(
    const checkpoint::SaveCheckpointSlot& source,
    std::uint8_t* bytes,
    std::size_t capacity,
    std::size_t& written) noexcept;

[[nodiscard]] CodecError decode_checkpoint_v9_into(
    const std::uint8_t* bytes,
    std::size_t size,
    checkpoint::SaveCheckpointSlot& destination,
    bool& migrated) noexcept;

// Allocation-free worker readback verification. Both the embedded V8 durable
// state and the V9 room payload are visited field by field and compared with
// the immutable job slot after the canonical byte stream is checked.
[[nodiscard]] CodecError verify_checkpoint_v9_readback(
    const std::uint8_t* bytes,
    std::size_t size,
    const checkpoint::SaveCheckpointSlot& expected,
    const std::uint8_t* canonical_bytes,
    std::size_t canonical_size) noexcept;

// Allocation-free outer-envelope inspection used by the worker's final A/B
// arbitration. It validates V9 magic/version/rules, length and CRC and returns
// the persistence revision without materializing checkpoint state.
[[nodiscard]] CodecError inspect_checkpoint_v9_envelope(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint64_t& revision) noexcept;

}  // namespace arpg::persistence
