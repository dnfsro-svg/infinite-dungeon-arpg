#include "platform/settings/settings_codec.hpp"

#include "core/crc32.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::settings {
namespace {

constexpr std::array<std::uint8_t, 8> magic{
    'A', 'R', 'P', 'G', 'S', 'E', 'T', '1'};
constexpr std::uint16_t format = 1U;
constexpr std::uint16_t payload_size = 20U;
constexpr std::size_t crc_covered_offset = 8U;
constexpr std::size_t crc_covered_size = 32U;
constexpr std::size_t crc_offset = 40U;

void write_u16(
    std::array<std::uint8_t, kSettingsEncodedSize>& bytes,
    std::size_t offset,
    std::uint16_t value) noexcept {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void write_u32(
    std::array<std::uint8_t, kSettingsEncodedSize>& bytes,
    std::size_t offset,
    std::uint32_t value) noexcept {
    for (std::size_t index = 0U; index < 4U; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value & 0xFFU);
        value >>= 8U;
    }
}

void write_u64(
    std::array<std::uint8_t, kSettingsEncodedSize>& bytes,
    std::size_t offset,
    std::uint64_t value) noexcept {
    for (std::size_t index = 0U; index < 8U; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value & 0xFFU);
        value >>= 8U;
    }
}

[[nodiscard]] std::uint16_t read_u16(
    const std::uint8_t* bytes, std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(bytes[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U);
}

[[nodiscard]] std::uint32_t read_u32(
    const std::uint8_t* bytes, std::size_t offset) noexcept {
    std::uint32_t value = 0U;
    for (std::size_t index = 0U; index < 4U; ++index) {
        value |= static_cast<std::uint32_t>(bytes[offset + index]) << (index * 8U);
    }
    return value;
}

[[nodiscard]] std::uint64_t read_u64(
    const std::uint8_t* bytes, std::size_t offset) noexcept {
    std::uint64_t value = 0U;
    for (std::size_t index = 0U; index < 8U; ++index) {
        value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8U);
    }
    return value;
}

[[nodiscard]] SettingsDecodeResult decode_error(SettingsCodecError error) noexcept {
    return {error, {}};
}

}  // namespace

std::array<std::uint8_t, kSettingsEncodedSize>
encode_settings(const SettingsData& settings) noexcept {
    std::array<std::uint8_t, kSettingsEncodedSize> bytes{};
    for (std::size_t index = 0U; index < magic.size(); ++index) {
        bytes[index] = magic[index];
    }
    write_u16(bytes, 8U, format);
    write_u16(bytes, 10U, payload_size);
    write_u64(bytes, 12U, settings.revision);
    bytes[20] = settings.master_sfx_percent;
    bytes[21] = static_cast<std::uint8_t>(settings.window_mode);
    bytes[22] = settings.vsync_enabled ? 1U : 0U;
    for (std::size_t index = 0U; index < settings.bindings.size(); ++index) {
        bytes[24U + index] = static_cast<std::uint8_t>(settings.bindings[index]);
    }
    const std::uint32_t checksum =
        core::crc32(bytes.data() + crc_covered_offset, crc_covered_size);
    write_u32(bytes, crc_offset, checksum);
    return bytes;
}

SettingsDecodeResult decode_settings(
    const std::uint8_t* bytes, std::size_t size) noexcept {
    if (bytes == nullptr || size != kSettingsEncodedSize) {
        return decode_error(SettingsCodecError::wrong_size);
    }
    for (std::size_t index = 0U; index < magic.size(); ++index) {
        if (bytes[index] != magic[index]) {
            return decode_error(SettingsCodecError::wrong_magic);
        }
    }
    if (read_u16(bytes, 8U) != format) {
        return decode_error(SettingsCodecError::wrong_format);
    }
    if (read_u16(bytes, 10U) != payload_size) {
        return decode_error(SettingsCodecError::wrong_payload_size);
    }
    const std::uint32_t expected_crc =
        core::crc32(bytes + crc_covered_offset, crc_covered_size);
    if (read_u32(bytes, crc_offset) != expected_crc) {
        return decode_error(SettingsCodecError::bad_crc);
    }
    if (bytes[23] != 0U) {
        return decode_error(SettingsCodecError::reserved_nonzero);
    }
    for (std::size_t offset = 34U; offset < crc_offset; ++offset) {
        if (bytes[offset] != 0U) {
            return decode_error(SettingsCodecError::reserved_nonzero);
        }
    }
    if (bytes[22] > 1U) {
        return decode_error(SettingsCodecError::invalid_settings);
    }

    SettingsData settings{};
    settings.revision = read_u64(bytes, 12U);
    settings.master_sfx_percent = bytes[20];
    settings.window_mode = static_cast<WindowMode>(bytes[21]);
    settings.vsync_enabled = bytes[22] != 0U;
    for (std::size_t index = 0U; index < settings.bindings.size(); ++index) {
        settings.bindings[index] = static_cast<StableKey>(bytes[24U + index]);
    }
    if (validate_settings(settings) != SettingsValidationError::none) {
        return decode_error(SettingsCodecError::invalid_settings);
    }
    return {SettingsCodecError::none, settings};
}

}  // namespace arpg::settings
