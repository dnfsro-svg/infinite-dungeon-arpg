#pragma once

#include "platform/settings/settings_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::settings {

inline constexpr std::size_t kSettingsEncodedSize = 44U;

enum class SettingsCodecError : std::uint8_t {
    none,
    wrong_size,
    wrong_magic,
    wrong_format,
    wrong_payload_size,
    bad_crc,
    reserved_nonzero,
    invalid_settings
};

struct SettingsDecodeResult final {
    SettingsCodecError error{SettingsCodecError::none};
    SettingsData settings{};
};

[[nodiscard]] std::array<std::uint8_t, kSettingsEncodedSize>
    encode_settings(const SettingsData& settings) noexcept;

[[nodiscard]] SettingsDecodeResult decode_settings(
    const std::uint8_t* bytes, std::size_t size) noexcept;

}  // namespace arpg::settings
