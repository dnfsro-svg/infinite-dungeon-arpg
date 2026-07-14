#include "test_framework.hpp"

#include "persistence/checkpoint_codec.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
namespace persistence = arpg::persistence;

checkpoint::DungeonRunState make_fixture() noexcept {
    checkpoint::DungeonRunState state{};
    state.root_seed = 0x0102030405060708ULL;
    state.commit_generation = 0x1112131415161718ULL;
    state.biases = {{0x21222324U, 0x25262728U, 0x292A2B2CU, 0x2D2E2F30U}};
    state.current_room.index = 0x3132333435363738ULL;
    state.current_room.seed = 0x4142434445464748ULL;
    state.current_room.depth = 2U;
    state.current_room.floor_room_index = 3U;
    state.current_room.entry = checkpoint::EntrySide::right;
    state.current_room.ecology = checkpoint::DungeonElement::chaos;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = true;
    state.last_transition = checkpoint::TransitionKind::descent;
    state.last_direction = checkpoint::ExitDirection::left;
    state.progression = {37U, 42U, 36U, 36U};
    return state;
}

bool same_state(
    const checkpoint::DungeonRunState& lhs,
    const checkpoint::DungeonRunState& rhs) noexcept {
    return lhs.root_seed == rhs.root_seed
        && lhs.commit_generation == rhs.commit_generation
        && lhs.biases == rhs.biases
        && lhs.current_room.index == rhs.current_room.index
        && lhs.current_room.seed == rhs.current_room.seed
        && lhs.current_room.depth == rhs.current_room.depth
        && lhs.current_room.floor_room_index == rhs.current_room.floor_room_index
        && lhs.current_room.entry == rhs.current_room.entry
        && lhs.current_room.ecology == rhs.current_room.ecology
        && lhs.current_room.has_hole == rhs.current_room.has_hole
        && lhs.current_room.is_abyss == rhs.current_room.is_abyss
        && lhs.progression.level == rhs.progression.level
        && lhs.progression.experience == rhs.progression.experience
        && lhs.progression.earned_passive_points
            == rhs.progression.earned_passive_points
        && lhs.progression.unspent_passive_points
            == rhs.progression.unspent_passive_points
        && lhs.passive_tree.allocated_bits == rhs.passive_tree.allocated_bits
        && lhs.last_transition == rhs.last_transition
        && lhs.last_direction == rhs.last_direction;
}

bool encoded_fixture(
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize>& bytes) noexcept {
    return persistence::encode_checkpoint(make_fixture(), bytes);
}

void refresh_crc(
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize>& bytes) noexcept {
    std::array<std::uint8_t, 108U> covered{};
    std::copy_n(bytes.begin() + 8U, 20U, covered.begin());
    std::copy_n(bytes.begin() + 32U, persistence::kCheckpointPayloadSize,
        covered.begin() + 20U);
    const auto checksum = persistence::crc32(covered.data(), covered.size());
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes[28U + index] = static_cast<std::uint8_t>(checksum >> (index * 8U));
    }
}

arpg::test::Failure baseline_checkpoint_bytes_are_preserved() noexcept {
    constexpr std::array<std::uint8_t, persistence::kPreviousEncodedCheckpointSize>
        kBaselineBytes{{
            0x49U, 0x41U, 0x52U, 0x50U, 0x47U, 0x53U, 0x30U, 0x33U,
            0x02U, 0x00U, 0x00U, 0x00U, 0x01U, 0x00U, 0x00U, 0x00U,
            0x18U, 0x17U, 0x16U, 0x15U, 0x14U, 0x13U, 0x12U, 0x11U,
            0x50U, 0x00U, 0x00U, 0x00U, 0xC7U, 0x37U, 0x66U, 0x35U,
            0x08U, 0x07U, 0x06U, 0x05U, 0x04U, 0x03U, 0x02U, 0x01U,
            0x24U, 0x23U, 0x22U, 0x21U, 0x28U, 0x27U, 0x26U, 0x25U,
            0x2CU, 0x2BU, 0x2AU, 0x29U, 0x30U, 0x2FU, 0x2EU, 0x2DU,
            0x38U, 0x37U, 0x36U, 0x35U, 0x34U, 0x33U, 0x32U, 0x31U,
            0x48U, 0x47U, 0x46U, 0x45U, 0x44U, 0x43U, 0x42U, 0x41U,
            0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
            0x03U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
            0x04U, 0x03U, 0x01U, 0x01U, 0x01U, 0x02U, 0x25U, 0x24U,
            0x24U, 0x00U, 0x2AU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        }};
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
    ARPG_REQUIRE(persistence::encode_checkpoint(make_fixture(), bytes));
    ARPG_REQUIRE(bytes.size() == 120U);
    ARPG_REQUIRE(bytes[0U] == 0x49U && bytes[7U] == 0x34U);
    ARPG_REQUIRE(bytes[106U] == 0x01U && bytes[107U] == 0x00U);

    const auto decoded = persistence::decode_checkpoint(
        kBaselineBytes.data(), kBaselineBytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(same_state(decoded.state, make_fixture()));
    const auto legacy_decoded = persistence::decode_checkpoint(
        kBaselineBytes.data(), kBaselineBytes.size());
    ARPG_REQUIRE(legacy_decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(legacy_decoded.state.passive_tree.allocated_bits == 1ULL);
    return {};
}

arpg::test::Failure all_nonzero_fields_round_trip() noexcept {
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
    ARPG_REQUIRE(persistence::encode_checkpoint(make_fixture(), bytes));
    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(same_state(decoded.state, make_fixture()));
    return {};
}

arpg::test::Failure encoded_sizes_and_generation_are_little_endian() noexcept {
    static_assert(persistence::kCheckpointHeaderSize == 32U);
    static_assert(persistence::kCheckpointPayloadSize == 88U);
    static_assert(persistence::kEncodedCheckpointSize == 120U);

    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
    ARPG_REQUIRE(encoded_fixture(bytes));
    ARPG_REQUIRE(bytes.size() == 120U);
    ARPG_REQUIRE(bytes[16] == 0x18U);
    ARPG_REQUIRE(bytes[17] == 0x17U);
    ARPG_REQUIRE(bytes[18] == 0x16U);
    ARPG_REQUIRE(bytes[19] == 0x15U);
    ARPG_REQUIRE(bytes[20] == 0x14U);
    ARPG_REQUIRE(bytes[21] == 0x13U);
    ARPG_REQUIRE(bytes[22] == 0x12U);
    ARPG_REQUIRE(bytes[23] == 0x11U);
    return {};
}

arpg::test::Failure crc32_known_value_and_covered_flip_are_detected() noexcept {
    constexpr char kInput[] = "123456789";
    ARPG_REQUIRE(persistence::crc32(
        reinterpret_cast<const std::uint8_t*>(kInput), 9U) == 0xCBF43926U);

    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
    ARPG_REQUIRE(encoded_fixture(bytes));
    bytes[32] ^= 0x01U;
    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::bad_crc);
    return {};
}

arpg::test::Failure wrong_magic_is_rejected() noexcept {
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
    ARPG_REQUIRE(encoded_fixture(bytes));
    bytes[0] ^= 0x01U;
    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::bad_magic);
    return {};
}

arpg::test::Failure unsupported_format_and_rules_are_rejected() noexcept {
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
    ARPG_REQUIRE(encoded_fixture(bytes));
    bytes[8] = 4U;
    auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::unsupported_format);

    ARPG_REQUIRE(encoded_fixture(bytes));
    bytes[12] = 2U;
    decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::unsupported_rules);
    return {};
}

arpg::test::Failure invalid_payload_length_is_rejected() noexcept {
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
    ARPG_REQUIRE(encoded_fixture(bytes));
    bytes[24] = 63U;
    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::bad_payload_length);
    return {};
}

arpg::test::Failure invalid_enum_values_are_rejected() noexcept {
    constexpr std::array<std::size_t, 4> kOffsets{{88U, 89U, 92U, 93U}};
    for (const auto offset : kOffsets) {
        std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
        ARPG_REQUIRE(encoded_fixture(bytes));
        bytes[offset] = 0xFEU;
        refresh_crc(bytes);
        const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
        ARPG_REQUIRE(decoded.error == persistence::CodecError::invalid_enum);
    }
    return {};
}

arpg::test::Failure invalid_booleans_and_zero_state_are_rejected() noexcept {
    for (const auto offset : std::array<std::size_t, 2>{{90U, 91U}}) {
        std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
        ARPG_REQUIRE(encoded_fixture(bytes));
        bytes[offset] = 2U;
        refresh_crc(bytes);
        const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
        ARPG_REQUIRE(decoded.error == persistence::CodecError::invalid_boolean);
    }

    for (const auto offset : std::array<std::size_t, 3>{{16U, 72U, 80U}}) {
        std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
        ARPG_REQUIRE(encoded_fixture(bytes));
        std::fill(
            bytes.begin() + offset,
            bytes.begin() + offset + 8U,
            static_cast<std::uint8_t>(0U));
        refresh_crc(bytes);
        const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
        ARPG_REQUIRE(decoded.error == persistence::CodecError::invalid_state);
    }
    return {};
}

std::array<std::uint8_t, persistence::kLegacyEncodedCheckpointSize>
legacy_fixture() noexcept {
    checkpoint::DungeonRunState legacy_state = make_fixture();
    legacy_state.progression = {};
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> current{};
    static_cast<void>(persistence::encode_checkpoint(legacy_state, current));
    std::array<std::uint8_t, persistence::kLegacyEncodedCheckpointSize> legacy{};
    std::copy_n(current.begin(), legacy.size(), legacy.begin());
    legacy[0U] = 'I'; legacy[1U] = 'A'; legacy[2U] = 'R'; legacy[3U] = 'P';
    legacy[4U] = 'G'; legacy[5U] = 'S'; legacy[6U] = '0'; legacy[7U] = '3';
    legacy[8U] = 1U;
    legacy[9U] = legacy[10U] = legacy[11U] = 0U;
    legacy[24U] = 64U;
    legacy[25U] = legacy[26U] = legacy[27U] = 0U;
    legacy[94U] = legacy[95U] = 0U;
    std::array<std::uint8_t, 84U> covered{};
    std::copy_n(legacy.begin() + 8U, 20U, covered.begin());
    std::copy_n(legacy.begin() + 32U, 64U, covered.begin() + 20U);
    const auto checksum = persistence::crc32(covered.data(), covered.size());
    for (std::size_t index = 0; index < 4U; ++index) {
        legacy[28U + index] = static_cast<std::uint8_t>(
            checksum >> (index * 8U));
    }
    return legacy;
}

arpg::test::Failure invalid_progression_is_rejected() noexcept {
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
    ARPG_REQUIRE(encoded_fixture(bytes));
    bytes[94U] = 0U;
    refresh_crc(bytes);
    ARPG_REQUIRE(persistence::decode_checkpoint(bytes.data(), bytes.size()).error
        == persistence::CodecError::invalid_state);

    ARPG_REQUIRE(encoded_fixture(bytes));
    bytes[95U] = 35U;
    refresh_crc(bytes);
    ARPG_REQUIRE(persistence::decode_checkpoint(bytes.data(), bytes.size()).error
        == persistence::CodecError::invalid_state);
    return {};
}

arpg::test::Failure version_one_migrates_to_new_character_progression() noexcept {
    const auto bytes = legacy_fixture();
    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.state.progression.level == 1U);
    ARPG_REQUIRE(decoded.state.progression.experience == 0U);
    ARPG_REQUIRE(decoded.state.progression.earned_passive_points == 0U);
    ARPG_REQUIRE(decoded.state.progression.unspent_passive_points == 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"baseline checkpoint bytes are preserved", &baseline_checkpoint_bytes_are_preserved},
    {"all nonzero fields round trip", &all_nonzero_fields_round_trip},
    {"encoded sizes and generation are little endian", &encoded_sizes_and_generation_are_little_endian},
    {"crc32 known value and covered flip are detected", &crc32_known_value_and_covered_flip_are_detected},
    {"wrong magic is rejected", &wrong_magic_is_rejected},
    {"unsupported format and rules are rejected", &unsupported_format_and_rules_are_rejected},
    {"invalid payload length is rejected", &invalid_payload_length_is_rejected},
    {"invalid enum values are rejected", &invalid_enum_values_are_rejected},
    {"invalid booleans and zero state are rejected", &invalid_booleans_and_zero_state_are_rejected},
    {"invalid progression is rejected", &invalid_progression_is_rejected},
    {"version one progression migration", &version_one_migrates_to_new_character_progression},
};

}  // namespace

arpg::test::TestSuite checkpoint_codec_suite() noexcept {
    return arpg::test::make_suite("checkpoint_codec", kCases);
}
