#pragma once

#include "dungeon/dungeon_checkpoint.hpp"
#include "persistence/crc32.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::persistence {

inline constexpr std::size_t kCheckpointHeaderSize = 32U;
inline constexpr std::size_t kLegacyCheckpointPayloadSize = 64U;
inline constexpr std::size_t kLegacyEncodedCheckpointSize = 96U;
inline constexpr std::size_t kPreviousCheckpointPayloadSize = 80U;
inline constexpr std::size_t kPreviousEncodedCheckpointSize = 112U;
inline constexpr std::size_t kCheckpointPayloadSize = 88U;
inline constexpr std::size_t kEncodedCheckpointSize = 120U;
inline constexpr std::uint32_t kLegacyCheckpointFormatVersion = 1U;
inline constexpr std::uint32_t kPreviousCheckpointFormatVersion = 2U;
inline constexpr std::uint32_t kCheckpointFormatVersion = 3U;
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
    invalid_state
};

struct DecodeResult final {
    CodecError error{CodecError::none};
    dungeon::checkpoint::DungeonRunState state{};
};

[[nodiscard]] bool encode_checkpoint(
    const dungeon::checkpoint::DungeonRunState& state,
    std::array<std::uint8_t, kEncodedCheckpointSize>& out) noexcept;

[[nodiscard]] DecodeResult decode_checkpoint(
    const std::uint8_t* bytes, std::size_t size) noexcept;

}  // namespace arpg::persistence
