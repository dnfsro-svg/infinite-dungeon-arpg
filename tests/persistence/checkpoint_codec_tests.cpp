#include "test_framework.hpp"

#include "persistence/checkpoint_codec.hpp"

#include <algorithm>
#include <array>
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
        && lhs.last_transition == rhs.last_transition
        && lhs.last_direction == rhs.last_direction;
}

bool encoded_fixture(
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize>& bytes) noexcept {
    return persistence::encode_checkpoint(make_fixture(), bytes);
}

void refresh_crc(
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize>& bytes) noexcept {
    std::array<std::uint8_t, 84U> covered{};
    std::copy_n(bytes.begin() + 8U, 20U, covered.begin());
    std::copy_n(bytes.begin() + 32U, 64U, covered.begin() + 20U);
    const auto checksum = persistence::crc32(covered.data(), covered.size());
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes[28U + index] = static_cast<std::uint8_t>(checksum >> (index * 8U));
    }
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
    static_assert(persistence::kCheckpointPayloadSize == 64U);
    static_assert(persistence::kEncodedCheckpointSize == 96U);

    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
    ARPG_REQUIRE(encoded_fixture(bytes));
    ARPG_REQUIRE(bytes.size() == 96U);
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
    bytes[8] = 2U;
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

constexpr arpg::test::TestCase kCases[] = {
    {"all nonzero fields round trip", &all_nonzero_fields_round_trip},
    {"encoded sizes and generation are little endian", &encoded_sizes_and_generation_are_little_endian},
    {"crc32 known value and covered flip are detected", &crc32_known_value_and_covered_flip_are_detected},
    {"wrong magic is rejected", &wrong_magic_is_rejected},
    {"unsupported format and rules are rejected", &unsupported_format_and_rules_are_rejected},
    {"invalid payload length is rejected", &invalid_payload_length_is_rejected},
    {"invalid enum values are rejected", &invalid_enum_values_are_rejected},
    {"invalid booleans and zero state are rejected", &invalid_booleans_and_zero_state_are_rejected},
};

}  // namespace

arpg::test::TestSuite checkpoint_codec_suite() noexcept {
    return arpg::test::make_suite("checkpoint_codec", kCases);
}
