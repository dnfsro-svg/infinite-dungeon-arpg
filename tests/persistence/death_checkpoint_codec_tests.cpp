#include "test_framework.hpp"

#include "persistence/checkpoint_codec.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
namespace persistence = arpg::persistence;

void write_u32(std::vector<std::uint8_t>& bytes,
    std::size_t offset, std::uint32_t value) noexcept {
    for (std::size_t index = 0U; index < 4U; ++index)
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
}

void refresh_crc(std::vector<std::uint8_t>& bytes) noexcept {
    const auto payload_size = static_cast<std::size_t>(bytes[24U])
        | static_cast<std::size_t>(bytes[25U]) << 8U
        | static_cast<std::size_t>(bytes[26U]) << 16U
        | static_cast<std::size_t>(bytes[27U]) << 24U;
    auto checksum = persistence::crc32_update(0U, bytes.data() + 8U, 20U);
    checksum = persistence::crc32_update(
        checksum, bytes.data() + persistence::kCheckpointHeaderSize, payload_size);
    write_u32(bytes, 28U, checksum);
}

std::vector<std::uint8_t> as_v5_fixture(
    const std::vector<std::uint8_t>& v6) {
    auto v5 = v6;
    v5.erase(v5.begin() + 236U, v5.begin() + 460U);
    const std::array<std::uint8_t, 8U> magic{{'I','A','R','P','G','S','0','6'}};
    std::copy(magic.begin(), magic.end(), v5.begin());
    write_u32(v5, 8U, 5U);
    write_u32(v5, 24U, static_cast<std::uint32_t>(v5.size() - 32U));
    refresh_crc(v5);
    return v5;
}

checkpoint::DungeonRunState make_state() noexcept {
    checkpoint::DungeonRunState state{};
    state.root_seed = 0x1020304050607080ULL;
    state.commit_generation = 9U;
    state.current_room = {17U, 0x123456789ABCDEF0ULL, 5U, 7U,
        checkpoint::EntrySide::left, checkpoint::DungeonElement::chaos,
        true, false};
    state.progression = {1U, 0U, 0U, 0U};
    state.passive_tree.allocated_bits = 1ULL;
    return state;
}

checkpoint::DeathCheckpoint make_pending_death() noexcept {
    checkpoint::DeathCheckpoint death{};
    death.lifecycle = checkpoint::DeathLifecycle::pending_continue;
    death.data_version = checkpoint::kDeathCheckpointDataVersion;
    death.death_depth = 5U;
    death.death_floor_room_index = 7U;
    death.death_ecology = checkpoint::DungeonElement::chaos;
    death.death_was_abyss = true;
    death.source_kind = checkpoint::DeathSourceKind::ground_hazard;
    death.source_monster_id = 3U;
    death.source_detail_id = 41U;
    death.damage_type = checkpoint::DeathDamageType::lightning;
    death.raw_damage = 90U;
    death.barrier_loss = 10U;
    death.health_loss = 40U;
    death.final_damage = 50U;
    death.recent_damage = {{1U, 2U, 3U, 50U, 5U}};
    death.hp = 0;
    death.max_hp = 100;
    death.barrier = 0;
    death.max_barrier = 20;
    death.armor = 1234;
    death.evasion = 5678;
    death.armor_reduction_bp = 2345;
    death.evasion_rate_bp = 3456;
    death.damage_reduction = {{-1000, 1000, 2000, 3000}};
    death.damage_reduction_cap = {{7500, 8000, 8500, 9000}};
    death.target_room = {18U, 0x0FEDCBA987654321ULL, 4U, 0U,
        checkpoint::EntrySide::initial, checkpoint::DungeonElement::water,
        true, false};
    return death;
}

bool same_death(const checkpoint::DeathCheckpoint& left,
    const checkpoint::DeathCheckpoint& right) noexcept {
    return left.lifecycle == right.lifecycle
        && left.data_version == right.data_version
        && left.death_depth == right.death_depth
        && left.death_floor_room_index == right.death_floor_room_index
        && left.death_ecology == right.death_ecology
        && left.death_was_abyss == right.death_was_abyss
        && left.source_kind == right.source_kind
        && left.source_monster_id == right.source_monster_id
        && left.source_detail_id == right.source_detail_id
        && left.damage_type == right.damage_type
        && left.raw_damage == right.raw_damage
        && left.barrier_loss == right.barrier_loss
        && left.health_loss == right.health_loss
        && left.final_damage == right.final_damage
        && left.recent_damage == right.recent_damage
        && left.hp == right.hp && left.max_hp == right.max_hp
        && left.barrier == right.barrier && left.max_barrier == right.max_barrier
        && left.armor == right.armor && left.evasion == right.evasion
        && left.armor_reduction_bp == right.armor_reduction_bp
        && left.evasion_rate_bp == right.evasion_rate_bp
        && left.damage_reduction == right.damage_reduction
        && left.damage_reduction_cap == right.damage_reduction_cap
        && left.target_room.index == right.target_room.index
        && left.target_room.seed == right.target_room.seed
        && left.target_room.depth == right.target_room.depth
        && left.target_room.floor_room_index == right.target_room.floor_room_index
        && left.target_room.entry == right.target_room.entry
        && left.target_room.ecology == right.target_room.ecology
        && left.target_room.has_hole == right.target_room.has_hole
        && left.target_room.is_abyss == right.target_room.is_abyss;
}

arpg::test::Failure v6_layout_and_full_round_trip() noexcept {
    static_assert(persistence::kV6DeathPayloadSize == 224U);
    static_assert(persistence::kV6BasePayloadSize == 428U);
    static_assert(persistence::kV6BaseEncodedCheckpointSize == 460U);
    auto state = make_state();
    state.death_sequence = 12U;
    state.death = make_pending_death();
    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    ARPG_REQUIRE(encoded->size() == 460U);
    const std::array<std::uint8_t, 8U> magic{{'A','R','P','G','S','V','6','\0'}};
    ARPG_REQUIRE(std::equal(magic.begin(), magic.end(), encoded->begin()));
    ARPG_REQUIRE((*encoded)[8U] == 6U);
    ARPG_REQUIRE((*encoded)[24U] == 0xACU && (*encoded)[25U] == 0x01U);
    ARPG_REQUIRE((*encoded)[236U] == 12U);
    ARPG_REQUIRE((*encoded)[244U] == 1U);
    ARPG_REQUIRE((*encoded)[249U] == 0U && (*encoded)[254U] == 0U
        && (*encoded)[255U] == 0U);
    ARPG_REQUIRE(std::all_of(encoded->begin() + 452U,
        encoded->begin() + 460U, [](std::uint8_t value) { return value == 0U; }));
    const auto decoded = persistence::decode_checkpoint(encoded->data(), encoded->size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(!decoded.migrated);
    ARPG_REQUIRE(decoded.state.death_sequence == state.death_sequence);
    ARPG_REQUIRE(same_death(decoded.state.death, state.death));

    auto none_state = make_state();
    none_state.death_sequence = 77U;
    const auto none_encoded = persistence::encode_checkpoint(none_state);
    ARPG_REQUIRE(none_encoded.has_value());
    const auto none_decoded = persistence::decode_checkpoint(
        none_encoded->data(), none_encoded->size());
    ARPG_REQUIRE(none_decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(none_decoded.state.death_sequence == 77U);
    ARPG_REQUIRE(same_death(none_decoded.state.death, checkpoint::DeathCheckpoint{}));
    return {};
}

arpg::test::Failure v5_fixture_migrates_to_canonical_none() noexcept {
    auto state = make_state();
    arpg::items::ItemInstance item{};
    item.id = 17U;
    item.base_id = 1U;
    item.rarity = arpg::items::ItemRarity::normal;
    item.item_level = 1U;
    item.required_level = 1U;
    state.item_ownership.items.push_back(item);
    state.item_ownership.equipment.equipped_ids[0] = item.id;
    state.item_ownership.claimed_drop_bits = {{1U, 2U, 4U}};
    state.item_ownership.next_item_sequence = 18U;
    const auto current = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(current.has_value());
    const auto v5 = as_v5_fixture(*current);
    ARPG_REQUIRE(v5.size() == 276U);
    const auto decoded = persistence::decode_checkpoint(v5.data(), v5.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.migrated);
    ARPG_REQUIRE(decoded.state.root_seed == state.root_seed);
    ARPG_REQUIRE(decoded.state.current_room.seed == state.current_room.seed);
    ARPG_REQUIRE(decoded.state.item_ownership.items.size() == 1U);
    ARPG_REQUIRE(decoded.state.item_ownership.items[0].id == item.id);
    ARPG_REQUIRE(decoded.state.item_ownership.equipment.equipped_ids[0] == item.id);
    ARPG_REQUIRE(decoded.state.item_ownership.claimed_drop_bits
        == state.item_ownership.claimed_drop_bits);
    ARPG_REQUIRE(decoded.state.item_ownership.next_item_sequence == 18U);
    ARPG_REQUIRE(decoded.state.death_sequence == 0U);
    ARPG_REQUIRE(same_death(decoded.state.death, checkpoint::DeathCheckpoint{}));
    return {};
}

arpg::test::Failure v6_death_enum_boolean_reserved_and_state_errors() noexcept {
    auto state = make_state();
    state.death_sequence = 1U;
    state.death = make_pending_death();
    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    const auto rejects = [&encoded](std::size_t offset, std::uint8_t value,
        persistence::CodecError expected) noexcept {
        auto bytes = *encoded;
        bytes[offset] = value;
        refresh_crc(bytes);
        return persistence::decode_checkpoint(bytes.data(), bytes.size()).error == expected;
    };
    ARPG_REQUIRE(rejects(244U, 2U, persistence::CodecError::invalid_enum));
    ARPG_REQUIRE(rejects(246U, 6U, persistence::CodecError::invalid_enum));
    ARPG_REQUIRE(rejects(247U, 5U, persistence::CodecError::invalid_enum));
    ARPG_REQUIRE(rejects(253U, 4U, persistence::CodecError::invalid_enum));
    ARPG_REQUIRE(rejects(448U, 5U, persistence::CodecError::invalid_enum));
    ARPG_REQUIRE(rejects(449U, 4U, persistence::CodecError::invalid_enum));
    ARPG_REQUIRE(rejects(252U, 2U, persistence::CodecError::invalid_boolean));
    ARPG_REQUIRE(rejects(450U, 2U, persistence::CodecError::invalid_boolean));
    ARPG_REQUIRE(rejects(451U, 2U, persistence::CodecError::invalid_boolean));
    ARPG_REQUIRE(rejects(249U, 1U, persistence::CodecError::invalid_state));
    ARPG_REQUIRE(rejects(254U, 1U, persistence::CodecError::invalid_state));
    ARPG_REQUIRE(rejects(452U, 1U, persistence::CodecError::invalid_state));
    ARPG_REQUIRE(rejects(296U, 49U, persistence::CodecError::invalid_state));
    ARPG_REQUIRE(rejects(248U, 0xFFU, persistence::CodecError::invalid_state));
    ARPG_REQUIRE(rejects(348U, 0U, persistence::CodecError::invalid_state));
    ARPG_REQUIRE(rejects(367U, 0x80U, persistence::CodecError::invalid_state));
    ARPG_REQUIRE(rejects(377U, 0xFFU, persistence::CodecError::invalid_state));
    ARPG_REQUIRE(rejects(440U, 1U, persistence::CodecError::invalid_state));
    auto recent_overflow = *encoded;
    std::fill(recent_overflow.begin() + 304U,
        recent_overflow.begin() + 344U, static_cast<std::uint8_t>(0xFFU));
    refresh_crc(recent_overflow);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        recent_overflow.data(), recent_overflow.size()).error
        == persistence::CodecError::invalid_state);

    auto invalid = state;
    invalid.death.max_hp = -1;
    ARPG_REQUIRE(!persistence::encode_checkpoint(invalid).has_value());
    invalid = state;
    invalid.death.target_room.depth = 5U;
    ARPG_REQUIRE(!persistence::encode_checkpoint(invalid).has_value());
    return {};
}

arpg::test::Failure v6_lengths_crc_and_magic_are_rejected() noexcept {
    const auto encoded = persistence::encode_checkpoint(make_state());
    ARPG_REQUIRE(encoded.has_value());
    for (std::size_t size = 236U; size < 460U; ++size) {
        ARPG_REQUIRE(persistence::decode_checkpoint(encoded->data(), size).error
            == persistence::CodecError::bad_payload_length);
    }
    auto trailing = *encoded;
    trailing.push_back(0U);
    ARPG_REQUIRE(persistence::decode_checkpoint(trailing.data(), trailing.size()).error
        == persistence::CodecError::bad_payload_length);
    auto bad_length = *encoded;
    write_u32(bad_length, 24U, 427U);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        bad_length.data(), bad_length.size()).error
        == persistence::CodecError::bad_payload_length);
    auto crc = *encoded;
    crc[236U] ^= 1U;
    ARPG_REQUIRE(persistence::decode_checkpoint(crc.data(), crc.size()).error
        == persistence::CodecError::bad_crc);
    auto magic = *encoded;
    magic[0U] ^= 1U;
    ARPG_REQUIRE(persistence::decode_checkpoint(magic.data(), magic.size()).error
        == persistence::CodecError::bad_magic);
    auto mismatch = *encoded;
    write_u32(mismatch, 8U, 5U);
    refresh_crc(mismatch);
    ARPG_REQUIRE(persistence::decode_checkpoint(mismatch.data(), mismatch.size()).error
        == persistence::CodecError::unsupported_format);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"v6 layout and full round trip", &v6_layout_and_full_round_trip},
    {"v5 fixture migrates to canonical none", &v5_fixture_migrates_to_canonical_none},
    {"v6 death enum boolean reserved and state errors", &v6_death_enum_boolean_reserved_and_state_errors},
    {"v6 lengths crc and magic are rejected", &v6_lengths_crc_and_magic_are_rejected},
};

}  // namespace

arpg::test::TestSuite death_checkpoint_codec_suite() noexcept {
    return arpg::test::make_suite("death_checkpoint_codec", kCases);
}
