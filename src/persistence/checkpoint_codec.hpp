#pragma once

#include "checkpoint/dungeon_run_state.hpp"
#include "persistence/crc32.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace arpg::persistence {

inline constexpr std::size_t kCheckpointHeaderSize = 32U;
inline constexpr std::size_t kLegacyCheckpointPayloadSize = 64U;
inline constexpr std::size_t kLegacyEncodedCheckpointSize = 96U;
inline constexpr std::size_t kPreviousCheckpointPayloadSize = 80U;
inline constexpr std::size_t kPreviousEncodedCheckpointSize = 112U;
inline constexpr std::size_t kCheckpointPayloadSize = 88U;
inline constexpr std::size_t kEncodedCheckpointSize = 120U;
inline constexpr std::size_t kV4BasePayloadSize = 172U;
inline constexpr std::size_t kV4ItemRecordSize = 40U;
inline constexpr std::size_t kV4BaseEncodedCheckpointSize = 204U;
inline constexpr std::size_t kV5AbyssPayloadSize = 32U;
inline constexpr std::size_t kV5BasePayloadSize = 204U;
inline constexpr std::size_t kV5BaseEncodedCheckpointSize = 236U;
inline constexpr std::size_t kV6DeathPayloadSize = 224U;
inline constexpr std::size_t kV6BasePayloadSize = 428U;
inline constexpr std::size_t kV6BaseEncodedCheckpointSize = 460U;
inline constexpr std::size_t kV7MaterialRecordSize = 16U;
inline constexpr std::size_t kV7MaterialPayloadSize = 224U;
inline constexpr std::size_t kV7DiscoveryPayloadSize = 8U;
inline constexpr std::size_t kV7MaterialClaimPayloadSize = 56U;
inline constexpr std::size_t kV7ItemRecordSize = 64U;
inline constexpr std::size_t kV7BasePayloadSize = 716U;
inline constexpr std::size_t kV7BaseEncodedCheckpointSize = 748U;
inline constexpr std::size_t kV8SkillLoadoutPayloadSize = 40U;
inline constexpr std::size_t kV8BasePayloadSize = 756U;
inline constexpr std::size_t kV8BaseEncodedCheckpointSize = 788U;
inline constexpr std::size_t kMaximumCheckpointItemCount = 65535U;
inline constexpr std::uint32_t kLegacyCheckpointFormatVersion = 1U;
inline constexpr std::uint32_t kPreviousCheckpointFormatVersion = 2U;
inline constexpr std::uint32_t kThirdCheckpointFormatVersion = 3U;
inline constexpr std::uint32_t kFourthCheckpointFormatVersion = 4U;
inline constexpr std::uint32_t kFifthCheckpointFormatVersion = 5U;
inline constexpr std::uint32_t kSixthCheckpointFormatVersion = 6U;
inline constexpr std::uint32_t kSeventhCheckpointFormatVersion = 7U;
inline constexpr std::uint32_t kCheckpointFormatVersion = 8U;
inline constexpr std::uint32_t kCheckpointRulesVersion = 1U;

enum class CodecError : std::uint8_t {
    none,
    wrong_size,
    bad_magic,
    unsupported_format,
    unsupported_rules,
    bad_payload_length,
    bad_crc,
    invalid_enum,
    invalid_boolean,
    invalid_state,
    allocation_failure
};

struct DecodeResult final {
    CodecError error{CodecError::none};
    checkpoint::DungeonRunState state{};
    bool migrated{};
};

using EncodedCheckpoint = std::vector<std::uint8_t>;

[[nodiscard]] std::optional<EncodedCheckpoint> encode_checkpoint(
    const checkpoint::DungeonRunState& state) noexcept;

[[nodiscard]] CodecError encode_checkpoint_into(
    const checkpoint::DungeonRunState& state,
    std::uint8_t* bytes,
    std::size_t capacity,
    std::size_t& written) noexcept;

// V9-only durable-image entry point. It validates the V9-owned resolution
// lifecycle while producing a fully canonical embedded V8 image whose
// reserved byte 150 remains zero; V9 stores that field in its outer payload.
[[nodiscard]] CodecError encode_checkpoint_v9_durable_into(
    const checkpoint::DungeonRunState& state,
    std::uint8_t* bytes,
    std::size_t capacity,
    std::size_t& written) noexcept;

// Allocation-free field visitor used by the save worker's V9 readback scan.
// This accepts only the canonical V8 durable payload embedded by V9 and
// compares every encoded authoritative field with the immutable job slot.
[[nodiscard]] CodecError verify_checkpoint_v8_readback_fields(
    const std::uint8_t* bytes,
    std::size_t size,
    const checkpoint::DungeonRunState& expected) noexcept;

[[nodiscard]] CodecError verify_checkpoint_v9_durable_readback_fields(
    const std::uint8_t* bytes,
    std::size_t size,
    const checkpoint::DungeonRunState& expected) noexcept;

[[nodiscard]] DecodeResult decode_checkpoint(
    const std::uint8_t* bytes, std::size_t size) noexcept;

}  // namespace arpg::persistence
