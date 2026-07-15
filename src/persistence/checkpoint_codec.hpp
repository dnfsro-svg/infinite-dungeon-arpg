#pragma once

#include "dungeon/dungeon_checkpoint.hpp"
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
inline constexpr std::size_t kMaximumCheckpointItemCount = 65535U;
inline constexpr std::uint32_t kLegacyCheckpointFormatVersion = 1U;
inline constexpr std::uint32_t kPreviousCheckpointFormatVersion = 2U;
inline constexpr std::uint32_t kThirdCheckpointFormatVersion = 3U;
inline constexpr std::uint32_t kCheckpointFormatVersion = 4U;
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
    dungeon::checkpoint::DungeonRunState state{};
};

using EncodedCheckpoint = std::vector<std::uint8_t>;

[[nodiscard]] std::optional<EncodedCheckpoint> encode_checkpoint(
    const dungeon::checkpoint::DungeonRunState& state) noexcept;

[[nodiscard]] DecodeResult decode_checkpoint(
    const std::uint8_t* bytes, std::size_t size) noexcept;

}  // namespace arpg::persistence
