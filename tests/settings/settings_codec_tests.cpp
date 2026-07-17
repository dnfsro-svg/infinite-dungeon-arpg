#include "test_framework.hpp"

#include "platform/settings/settings_codec.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {

using arpg::settings::SettingsCodecError;
using arpg::settings::SettingsData;
using arpg::settings::StableKey;
using arpg::settings::WindowMode;

constexpr std::size_t crc_offset = 40U;

[[nodiscard]] std::uint32_t test_crc32(
    const std::uint8_t* bytes, std::size_t size) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= bytes[index];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask =
                0U - static_cast<std::uint32_t>(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

void refresh_crc(
    std::array<std::uint8_t, arpg::settings::kSettingsEncodedSize>& bytes) noexcept {
    const std::uint32_t crc = test_crc32(bytes.data() + 8U, 32U);
    bytes[40] = static_cast<std::uint8_t>(crc & 0xFFU);
    bytes[41] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);
    bytes[42] = static_cast<std::uint8_t>((crc >> 16U) & 0xFFU);
    bytes[43] = static_cast<std::uint8_t>((crc >> 24U) & 0xFFU);
}

[[nodiscard]] SettingsData golden_settings() noexcept {
    SettingsData settings = arpg::settings::default_settings();
    settings.master_sfx_percent = 95U;
    settings.window_mode = WindowMode::fullscreen;
    settings.vsync_enabled = false;
    settings.revision = 0x0102030405060708ULL;
    return settings;
}

[[nodiscard]] bool same_settings(
    const SettingsData& lhs, const SettingsData& rhs) noexcept {
    return lhs.master_sfx_percent == rhs.master_sfx_percent &&
        lhs.window_mode == rhs.window_mode &&
        lhs.vsync_enabled == rhs.vsync_enabled &&
        lhs.bindings == rhs.bindings &&
        lhs.revision == rhs.revision;
}

arpg::test::Failure fixed_layout_matches_golden_bytes() noexcept {
    static_assert(arpg::settings::kSettingsEncodedSize == 44U,
        "settings record format 1 must remain exactly 44 bytes");
    const auto encoded = arpg::settings::encode_settings(golden_settings());
    constexpr std::array<std::uint8_t, 44> expected{
        0x41, 0x52, 0x50, 0x47, 0x53, 0x45, 0x54, 0x31,
        0x01, 0x00, 0x14, 0x00,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x5F, 0x01, 0x00, 0x00,
        0x16, 0x12, 0x00, 0x03, 0x09, 0x0A, 0x0B, 0x04, 0x08, 0x0F,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xE1, 0xE9, 0x38, 0x1C};
    ARPG_REQUIRE(encoded == expected);
    return {};
}

arpg::test::Failure valid_records_round_trip() noexcept {
    const SettingsData expected = golden_settings();
    const auto encoded = arpg::settings::encode_settings(expected);
    const auto decoded = arpg::settings::decode_settings(encoded.data(), encoded.size());
    ARPG_REQUIRE(decoded.error == SettingsCodecError::none);
    ARPG_REQUIRE(same_settings(decoded.settings, expected));
    return {};
}

arpg::test::Failure null_truncated_and_trailing_records_are_rejected() noexcept {
    const auto encoded = arpg::settings::encode_settings(golden_settings());
    ARPG_REQUIRE(arpg::settings::decode_settings(nullptr, encoded.size()).error ==
        SettingsCodecError::wrong_size);
    for (std::size_t size = 0U; size < encoded.size(); ++size) {
        ARPG_REQUIRE(arpg::settings::decode_settings(encoded.data(), size).error ==
            SettingsCodecError::wrong_size);
    }
    ARPG_REQUIRE(arpg::settings::decode_settings(encoded.data(), encoded.size() + 1U).error ==
        SettingsCodecError::wrong_size);
    return {};
}

arpg::test::Failure every_single_byte_mutation_is_rejected() noexcept {
    const auto encoded = arpg::settings::encode_settings(golden_settings());
    for (std::size_t offset = 0U; offset < encoded.size(); ++offset) {
        auto mutated = encoded;
        mutated[offset] ^= 0x80U;
        ARPG_REQUIRE(arpg::settings::decode_settings(mutated.data(), mutated.size()).error !=
            SettingsCodecError::none);
    }
    return {};
}

arpg::test::Failure header_and_crc_errors_are_classified() noexcept {
    const auto valid = arpg::settings::encode_settings(golden_settings());

    auto bytes = valid;
    bytes[0] ^= 0x01U;
    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
        SettingsCodecError::wrong_magic);

    bytes = valid;
    bytes[8] = 2U;
    refresh_crc(bytes);
    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
        SettingsCodecError::wrong_format);

    bytes = valid;
    bytes[10] = 19U;
    refresh_crc(bytes);
    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
        SettingsCodecError::wrong_payload_size);

    bytes = valid;
    bytes[crc_offset] ^= 0x01U;
    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
        SettingsCodecError::bad_crc);
    return {};
}

arpg::test::Failure every_reserved_byte_must_be_zero() noexcept {
    const auto valid = arpg::settings::encode_settings(golden_settings());
    constexpr std::array<std::size_t, 7> reserved_offsets{23U, 34U, 35U, 36U, 37U, 38U, 39U};
    for (const std::size_t offset : reserved_offsets) {
        auto bytes = valid;
        bytes[offset] = 1U;
        refresh_crc(bytes);
        ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
            SettingsCodecError::reserved_nonzero);
    }
    return {};
}

arpg::test::Failure invalid_scalar_fields_are_rejected() noexcept {
    const auto valid = arpg::settings::encode_settings(golden_settings());
    constexpr std::array<std::pair<std::size_t, std::uint8_t>, 4> mutations{{
        {20U, static_cast<std::uint8_t>(101U)},
        {20U, static_cast<std::uint8_t>(99U)},
        {21U, static_cast<std::uint8_t>(2U)},
        {22U, static_cast<std::uint8_t>(2U)}}};
    for (const auto& mutation : mutations) {
        auto bytes = valid;
        bytes[mutation.first] = mutation.second;
        refresh_crc(bytes);
        ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
            SettingsCodecError::invalid_settings);
    }
    return {};
}

arpg::test::Failure invalid_and_duplicate_bindings_are_rejected() noexcept {
    const auto valid = arpg::settings::encode_settings(golden_settings());

    auto bytes = valid;
    bytes[24] = static_cast<std::uint8_t>(StableKey::count);
    refresh_crc(bytes);
    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
        SettingsCodecError::invalid_settings);

    bytes = valid;
    bytes[24] = bytes[25];
    refresh_crc(bytes);
    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
        SettingsCodecError::invalid_settings);
    return {};
}

constexpr arpg::test::TestCase cases[] = {
    {"fixed layout matches golden bytes and CRC", fixed_layout_matches_golden_bytes},
    {"valid records round trip", valid_records_round_trip},
    {"null, truncated, and trailing records are rejected", null_truncated_and_trailing_records_are_rejected},
    {"all 44 single-byte mutations are rejected", every_single_byte_mutation_is_rejected},
    {"header and CRC errors are classified", header_and_crc_errors_are_classified},
    {"every reserved byte must be zero", every_reserved_byte_must_be_zero},
    {"invalid scalar fields are rejected", invalid_scalar_fields_are_rejected},
    {"invalid and duplicate bindings are rejected", invalid_and_duplicate_bindings_are_rejected}};

}  // namespace

arpg::test::TestSuite settings_codec_suite() noexcept {
    return arpg::test::make_suite("settings.codec", cases);
}
