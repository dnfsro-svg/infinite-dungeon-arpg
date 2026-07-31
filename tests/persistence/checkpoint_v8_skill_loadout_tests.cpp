#include "test_framework.hpp"

#include "persistence/checkpoint_codec.hpp"
#include "skills/skill_loadout.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

namespace checkpoint = arpg::checkpoint;
namespace persistence = arpg::persistence;
namespace skills = arpg::skills;

constexpr std::size_t kV8LoadoutOffset =
    persistence::kCheckpointHeaderSize + persistence::kV7BasePayloadSize;
constexpr std::size_t kOwnedBitsOffset = kV8LoadoutOffset;
constexpr std::size_t kActiveIdsOffset = kOwnedBitsOffset + 8U;
constexpr std::size_t kSupportIdsOffset = kActiveIdsOffset + 5U;
constexpr std::size_t kReservedOffset = kSupportIdsOffset + 25U;

void write_u32(std::vector<std::uint8_t>& bytes, const std::size_t offset,
    const std::uint32_t value) noexcept {
    for (std::size_t index = 0U; index < 4U; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(
            value >> (index * 8U));
    }
}

void write_u64(std::vector<std::uint8_t>& bytes, const std::size_t offset,
    const std::uint64_t value) noexcept {
    for (std::size_t index = 0U; index < 8U; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(
            value >> (index * 8U));
    }
}

void refresh_crc(std::vector<std::uint8_t>& bytes) noexcept {
    const std::uint32_t payload_size =
        static_cast<std::uint32_t>(bytes[24U])
        | (static_cast<std::uint32_t>(bytes[25U]) << 8U)
        | (static_cast<std::uint32_t>(bytes[26U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[27U]) << 24U);
    auto checksum = persistence::crc32_update(
        0U, bytes.data() + 8U, 20U);
    checksum = persistence::crc32_update(
        checksum, bytes.data() + persistence::kCheckpointHeaderSize,
        payload_size);
    write_u32(bytes, 28U, checksum);
}

checkpoint::DungeonRunState make_state() noexcept {
    checkpoint::DungeonRunState state{};
    state.root_seed = 0x0102030405060708ULL;
    state.commit_generation = 7U;
    state.current_room.index = 11U;
    state.current_room.seed = 13U;
    state.current_room.depth = 2U;
    state.current_room.floor_room_index = 3U;
    return state;
}

skills::SkillLoadoutState empty_loadout() noexcept {
    skills::SkillLoadoutState loadout{};
    for (auto& slot : loadout.slots) {
        slot.active = skills::ActiveSkillId::none;
        slot.supports.fill(skills::SupportSkillId::none);
    }
    return loadout;
}

bool same_loadout(const skills::SkillLoadoutState& lhs,
    const skills::SkillLoadoutState& rhs) noexcept {
    if (lhs.owned_active_bits != rhs.owned_active_bits)
        return false;
    for (std::size_t slot = 0U; slot < lhs.slots.size(); ++slot) {
        if (lhs.slots[slot].active != rhs.slots[slot].active
            || lhs.slots[slot].supports != rhs.slots[slot].supports) {
            return false;
        }
    }
    return true;
}

std::vector<std::uint8_t> encode_state(
    const checkpoint::DungeonRunState& state) {
    const auto encoded = persistence::encode_checkpoint(state);
    return encoded.has_value() ? *encoded : std::vector<std::uint8_t>{};
}

std::vector<std::uint8_t> v7_from_v8(
    const std::vector<std::uint8_t>& v8) {
    if (v8.size() < persistence::kV8BaseEncodedCheckpointSize)
        return {};
    std::vector<std::uint8_t> v7;
    v7.reserve(v8.size() - persistence::kV8SkillLoadoutPayloadSize);
    v7.insert(v7.end(), v8.begin(), v8.begin() + kV8LoadoutOffset);
    v7.insert(v7.end(), v8.begin() + persistence::kV8BaseEncodedCheckpointSize,
        v8.end());
    const std::array<std::uint8_t, 8U> magic{{
        'A', 'R', 'P', 'G', 'S', 'V', '7', '\0'}};
    std::copy(magic.begin(), magic.end(), v7.begin());
    write_u32(v7, 8U, persistence::kSeventhCheckpointFormatVersion);
    write_u32(v7, 24U, static_cast<std::uint32_t>(v7.size()
        - persistence::kCheckpointHeaderSize));
    refresh_crc(v7);
    return v7;
}

std::vector<std::uint8_t> fixed_v1_sample() {
    std::vector<std::uint8_t> bytes(
        persistence::kLegacyEncodedCheckpointSize, 0U);
    const std::array<std::uint8_t, 8U> magic{{
        'I', 'A', 'R', 'P', 'G', 'S', '0', '3'}};
    std::copy(magic.begin(), magic.end(), bytes.begin());
    write_u32(bytes, 8U, persistence::kLegacyCheckpointFormatVersion);
    write_u32(bytes, 12U, persistence::kCheckpointRulesVersion);
    write_u64(bytes, 16U, 1U);
    write_u32(bytes, 24U, persistence::kLegacyCheckpointPayloadSize);
    write_u64(bytes, 32U, 1U);
    write_u64(bytes, 56U, 1U);
    write_u64(bytes, 64U, 2U);
    write_u64(bytes, 72U, 1U);
    write_u64(bytes, 80U, 1U);
    bytes[92U] = static_cast<std::uint8_t>(checkpoint::TransitionKind::none);
    bytes[93U] = static_cast<std::uint8_t>(checkpoint::ExitDirection::none);
    refresh_crc(bytes);
    return bytes;
}

arpg::test::Failure v8_all_five_slot_arrangements_round_trip() noexcept {
    static_assert(persistence::kCheckpointFormatVersion == 8U);
    static_assert(persistence::kV8SkillLoadoutPayloadSize == 40U);
    static_assert(persistence::kV8BasePayloadSize == 756U);
    static_assert(persistence::kV8BaseEncodedCheckpointSize == 788U);

    for (std::size_t draw_slot = 0U;
            draw_slot < skills::kActiveSkillSlotCount; ++draw_slot) {
        for (std::size_t storm_slot = 0U;
                storm_slot < skills::kActiveSkillSlotCount; ++storm_slot) {
            if (draw_slot == storm_slot)
                continue;
            auto state = make_state();
            state.skill_loadout = empty_loadout();
            state.skill_loadout.owned_active_bits = 0x3U;
            state.skill_loadout.slots[draw_slot].active =
                skills::ActiveSkillId::draw_slash;
            state.skill_loadout.slots[storm_slot].active =
                skills::ActiveSkillId::storm_swords;

            const auto encoded = persistence::encode_checkpoint(state);
            ARPG_REQUIRE(encoded.has_value());
            ARPG_REQUIRE(encoded->size()
                == persistence::kV8BaseEncodedCheckpointSize);
            const auto decoded = persistence::decode_checkpoint(
                encoded->data(), encoded->size());
            ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
            ARPG_REQUIRE(!decoded.migrated);
            ARPG_REQUIRE(same_loadout(
                decoded.state.skill_loadout, state.skill_loadout));
        }
    }
    return {};
}

arpg::test::Failure v8_empty_slots_and_owned_bits_round_trip() noexcept {
    auto state = make_state();
    state.skill_loadout = empty_loadout();
    state.skill_loadout.owned_active_bits = 0x2U;
    state.skill_loadout.slots[4U].active =
        skills::ActiveSkillId::storm_swords;
    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    const auto decoded = persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(same_loadout(decoded.state.skill_loadout, state.skill_loadout));

    state.skill_loadout = empty_loadout();
    const auto all_empty = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(all_empty.has_value());
    const auto decoded_empty = persistence::decode_checkpoint(
        all_empty->data(), all_empty->size());
    ARPG_REQUIRE(decoded_empty.error == persistence::CodecError::none);
    ARPG_REQUIRE(same_loadout(
        decoded_empty.state.skill_loadout, state.skill_loadout));
    return {};
}

arpg::test::Failure v7_base_golden_migrates_to_default_loadout() noexcept {
    const auto current = encode_state(make_state());
    ARPG_REQUIRE(current.size() == persistence::kV8BaseEncodedCheckpointSize);
    const auto v7 = v7_from_v8(current);
    ARPG_REQUIRE(v7.size() == persistence::kV7BaseEncodedCheckpointSize);
    ARPG_REQUIRE(v7[6U] == '7');
    ARPG_REQUIRE(v7[8U] == 7U);
    ARPG_REQUIRE(std::equal(v7.begin() + persistence::kCheckpointHeaderSize,
        v7.end(), current.begin() + persistence::kCheckpointHeaderSize));

    const auto decoded = persistence::decode_checkpoint(v7.data(), v7.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.migrated);
    ARPG_REQUIRE(same_loadout(
        decoded.state.skill_loadout, skills::default_skill_loadout()));
    return {};
}

arpg::test::Failure fixed_v1_migrates_to_default_loadout() noexcept {
    const auto v1 = fixed_v1_sample();
    const auto decoded = persistence::decode_checkpoint(v1.data(), v1.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.migrated);
    ARPG_REQUIRE(same_loadout(
        decoded.state.skill_loadout, skills::default_skill_loadout()));
    return {};
}

arpg::test::Failure every_active_and_support_id_is_validated() noexcept {
    const auto encoded = encode_state(make_state());
    ARPG_REQUIRE(encoded.size() == persistence::kV8BaseEncodedCheckpointSize);
    for (std::size_t slot = 0U; slot < skills::kActiveSkillSlotCount; ++slot) {
        auto invalid = encoded;
        invalid[kActiveIdsOffset + slot] =
            static_cast<std::uint8_t>(skills::ActiveSkillId::count);
        refresh_crc(invalid);
        ARPG_REQUIRE(persistence::decode_checkpoint(
            invalid.data(), invalid.size()).error
                == persistence::CodecError::invalid_enum);
    }
    for (std::size_t support = 0U;
            support < skills::kActiveSkillSlotCount
                * skills::kSupportSlotsPerActive; ++support) {
        auto invalid = encoded;
        invalid[kSupportIdsOffset + support] =
            static_cast<std::uint8_t>(skills::SupportSkillId::count);
        refresh_crc(invalid);
        ARPG_REQUIRE(persistence::decode_checkpoint(
            invalid.data(), invalid.size()).error
                == persistence::CodecError::invalid_enum);
    }
    return {};
}

arpg::test::Failure duplicate_non_owned_and_owned_high_bits_are_rejected()
    noexcept {
    const auto encoded = encode_state(make_state());
    ARPG_REQUIRE(encoded.size() == persistence::kV8BaseEncodedCheckpointSize);

    auto duplicate = encoded;
    duplicate[kActiveIdsOffset + 1U] = duplicate[kActiveIdsOffset];
    refresh_crc(duplicate);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        duplicate.data(), duplicate.size()).error
            == persistence::CodecError::invalid_state);

    auto not_owned = encoded;
    not_owned[kOwnedBitsOffset] = 0x1U;
    refresh_crc(not_owned);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        not_owned.data(), not_owned.size()).error
            == persistence::CodecError::invalid_state);

    auto high_owned_bit = encoded;
    high_owned_bit[kOwnedBitsOffset] |= 0x4U;
    refresh_crc(high_owned_bit);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        high_owned_bit.data(), high_owned_bit.size()).error
            == persistence::CodecError::invalid_state);
    return {};
}

arpg::test::Failure v8_reserved_bytes_and_invalid_encode_states_are_rejected()
    noexcept {
    const auto encoded = encode_state(make_state());
    ARPG_REQUIRE(encoded.size() == persistence::kV8BaseEncodedCheckpointSize);
    for (std::size_t index = 0U; index < 2U; ++index) {
        auto invalid = encoded;
        invalid[kReservedOffset + index] = 1U;
        refresh_crc(invalid);
        ARPG_REQUIRE(persistence::decode_checkpoint(
            invalid.data(), invalid.size()).error
                == persistence::CodecError::invalid_state);
    }

    auto state = make_state();
    state.skill_loadout.slots[1U].active = skills::ActiveSkillId::draw_slash;
    ARPG_REQUIRE(!persistence::encode_checkpoint(state).has_value());
    state = make_state();
    state.skill_loadout.owned_active_bits = 0x1U;
    ARPG_REQUIRE(!persistence::encode_checkpoint(state).has_value());
    state = make_state();
    state.skill_loadout.slots[0U].supports[0U] =
        static_cast<skills::SupportSkillId>(0U);
    ARPG_REQUIRE(!persistence::encode_checkpoint(state).has_value());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"v8 all five slot arrangements round trip",
        &v8_all_five_slot_arrangements_round_trip},
    {"v8 empty slots and owned bits round trip",
        &v8_empty_slots_and_owned_bits_round_trip},
    {"v7 base golden migrates to default loadout",
        &v7_base_golden_migrates_to_default_loadout},
    {"fixed v1 migrates to default loadout",
        &fixed_v1_migrates_to_default_loadout},
    {"every active and support id is validated",
        &every_active_and_support_id_is_validated},
    {"duplicate non owned and owned high bits are rejected",
        &duplicate_non_owned_and_owned_high_bits_are_rejected},
    {"v8 reserved bytes and invalid encode states are rejected",
        &v8_reserved_bytes_and_invalid_encode_states_are_rejected},
};

}  // namespace

arpg::test::TestSuite checkpoint_v8_skill_loadout_suite() noexcept {
    return arpg::test::make_suite("checkpoint_v8_skill_loadout", kCases);
}
