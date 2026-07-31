#include "test_framework.hpp"
#include "v5_golden_fixture.hpp"

#include "items/item_catalog.hpp"
#include "persistence/checkpoint_codec.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace {

namespace checkpoint = arpg::checkpoint;
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
    death.death_depth = 0x0102030405060708ULL;
    death.death_floor_room_index = 0x1112131415161718ULL;
    death.death_ecology = checkpoint::DungeonElement::chaos;
    death.death_was_abyss = true;
    death.source_kind = checkpoint::DeathSourceKind::ground_hazard;
    death.source_monster_id = 0x4BU;
    death.source_detail_id = 0xA1B2U;
    death.damage_type = checkpoint::DeathDamageType::lightning;
    death.raw_damage = 0x2122232425262728ULL;
    death.barrier_loss = 0x0102U;
    death.health_loss = 0x0304U;
    death.final_damage = 0x0406U;
    death.recent_damage = {{
        0x0102030405060708ULL, 0x1112131415161718ULL,
        0x2122232425262728ULL, 0x3132333435363738ULL,
        0x4142434445464748ULL}};
    death.hp = 0;
    death.max_hp = 0x01020304;
    death.barrier = 0;
    death.max_barrier = 0x11121314;
    death.armor = 0x0102030405060708LL;
    death.evasion = 0x1112131415161718LL;
    death.armor_reduction_bp = 0x1234;
    death.evasion_rate_bp = 0x2345;
    death.damage_reduction = {{-0x0102, 0x0304, 0x0506, 0x0708}};
    death.damage_reduction_cap = {{7500, 8000, 8500, 9000}};
    death.target_room = {0x5152535455565758ULL,
        0x6162636465666768ULL, 0x0102030405060707ULL, 0U,
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

template <std::size_t Size>
bool bytes_at(const std::vector<std::uint8_t>& bytes, std::size_t offset,
    const std::array<std::uint8_t, Size>& expected) noexcept {
    return offset <= bytes.size() && bytes.size() - offset >= Size
        && std::equal(expected.begin(), expected.end(), bytes.begin() + offset);
}

arpg::test::Failure v8_layout_and_full_round_trip() noexcept {
    static_assert(persistence::kV6DeathPayloadSize == 224U);
    static_assert(persistence::kV6BasePayloadSize == 428U);
    static_assert(persistence::kV6BaseEncodedCheckpointSize == 460U);
    static_assert(persistence::kV7BasePayloadSize == 716U);
    static_assert(persistence::kV7BaseEncodedCheckpointSize == 748U);
    auto state = make_state();
    state.death_sequence = 12U;
    state.death = make_pending_death();
    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    ARPG_REQUIRE(encoded->size() == 788U);
    const std::array<std::uint8_t, 8U> magic{{'A','R','P','G','S','V','8','\0'}};
    ARPG_REQUIRE(std::equal(magic.begin(), magic.end(), encoded->begin()));
    ARPG_REQUIRE((*encoded)[8U] == 8U);
    ARPG_REQUIRE((*encoded)[24U] == 0xF4U && (*encoded)[25U] == 0x02U);
    ARPG_REQUIRE((*encoded)[236U] == 12U);
    ARPG_REQUIRE((*encoded)[244U] == 1U);
    ARPG_REQUIRE(bytes_at(*encoded, 245U,
        std::array<std::uint8_t, 9U>{{
            0x01U, 0x02U, 0x03U, 0x4BU, 0x00U,
            0xB2U, 0xA1U, 0x01U, 0x03U}}));
    ARPG_REQUIRE(bytes_at(*encoded, 254U,
        std::array<std::uint8_t, 2U>{{0x00U, 0x00U}}));
    ARPG_REQUIRE(bytes_at(*encoded, 256U,
        std::array<std::uint8_t, 48U>{{
            0x08U,0x07U,0x06U,0x05U,0x04U,0x03U,0x02U,0x01U,
            0x18U,0x17U,0x16U,0x15U,0x14U,0x13U,0x12U,0x11U,
            0x28U,0x27U,0x26U,0x25U,0x24U,0x23U,0x22U,0x21U,
            0x02U,0x01U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,
            0x04U,0x03U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,
            0x06U,0x04U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U}}));
    ARPG_REQUIRE(bytes_at(*encoded, 304U,
        std::array<std::uint8_t, 40U>{{
            0x08U,0x07U,0x06U,0x05U,0x04U,0x03U,0x02U,0x01U,
            0x18U,0x17U,0x16U,0x15U,0x14U,0x13U,0x12U,0x11U,
            0x28U,0x27U,0x26U,0x25U,0x24U,0x23U,0x22U,0x21U,
            0x38U,0x37U,0x36U,0x35U,0x34U,0x33U,0x32U,0x31U,
            0x48U,0x47U,0x46U,0x45U,0x44U,0x43U,0x42U,0x41U}}));
    ARPG_REQUIRE(bytes_at(*encoded, 344U,
        std::array<std::uint8_t, 40U>{{
            0x00U,0x00U,0x00U,0x00U, 0x04U,0x03U,0x02U,0x01U,
            0x00U,0x00U,0x00U,0x00U, 0x14U,0x13U,0x12U,0x11U,
            0x08U,0x07U,0x06U,0x05U,0x04U,0x03U,0x02U,0x01U,
            0x18U,0x17U,0x16U,0x15U,0x14U,0x13U,0x12U,0x11U,
            0x34U,0x12U,0x00U,0x00U, 0x45U,0x23U,0x00U,0x00U}}));
    ARPG_REQUIRE(bytes_at(*encoded, 384U,
        std::array<std::uint8_t, 32U>{{
            0xFEU,0xFEU,0xFFU,0xFFU, 0x04U,0x03U,0x00U,0x00U,
            0x06U,0x05U,0x00U,0x00U, 0x08U,0x07U,0x00U,0x00U,
            0x4CU,0x1DU,0x00U,0x00U, 0x40U,0x1FU,0x00U,0x00U,
            0x34U,0x21U,0x00U,0x00U, 0x28U,0x23U,0x00U,0x00U}}));
    ARPG_REQUIRE(bytes_at(*encoded, 416U,
        std::array<std::uint8_t, 36U>{{
            0x58U,0x57U,0x56U,0x55U,0x54U,0x53U,0x52U,0x51U,
            0x68U,0x67U,0x66U,0x65U,0x64U,0x63U,0x62U,0x61U,
            0x07U,0x07U,0x06U,0x05U,0x04U,0x03U,0x02U,0x01U,
            0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,
            0x00U,0x01U,0x01U,0x00U}}));
    ARPG_REQUIRE(std::all_of(encoded->begin() + 452U,
        encoded->begin() + 460U, [](std::uint8_t value) { return value == 0U; }));
    for (std::size_t index = 0U; index < arpg::items::kMaterialCount; ++index) {
        const std::size_t record = 460U
            + index * persistence::kV7MaterialRecordSize;
        ARPG_REQUIRE((*encoded)[record] == index + 1U);
        ARPG_REQUIRE(std::all_of(encoded->begin() + record + 1U,
            encoded->begin() + record + persistence::kV7MaterialRecordSize,
            [](std::uint8_t value) { return value == 0U; }));
    }
    ARPG_REQUIRE(std::all_of(encoded->begin() + 684U,
        encoded->begin() + 748U, [](std::uint8_t value) { return value == 0U; }));
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
    const auto& v5 = arpg::test::fixtures::kV5FullState;
    const auto decoded = persistence::decode_checkpoint(v5.data(), v5.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.migrated);
    const auto& state = decoded.state;
    ARPG_REQUIRE(state.root_seed == 0x0102030405060708ULL);
    ARPG_REQUIRE(state.commit_generation == 0x1112131415161718ULL);
    ARPG_REQUIRE((state.biases == std::array<std::uint32_t, 4U>{{
        0x21222324U, 0x25262728U, 0x292A2B2CU, 0x2D2E2F30U}}));
    ARPG_REQUIRE(state.current_room.index == 0x3132333435363738ULL);
    ARPG_REQUIRE(state.current_room.seed == 321U);
    ARPG_REQUIRE(state.current_room.depth == 40U);
    ARPG_REQUIRE(state.current_room.floor_room_index
        == 0x4142434445464748ULL);
    ARPG_REQUIRE(state.current_room.entry == checkpoint::EntrySide::right);
    ARPG_REQUIRE(state.current_room.ecology == checkpoint::DungeonElement::chaos);
    ARPG_REQUIRE(state.current_room.has_hole && state.current_room.is_abyss);
    ARPG_REQUIRE(state.last_transition == checkpoint::TransitionKind::door);
    ARPG_REQUIRE(state.last_direction == checkpoint::ExitDirection::left);
    ARPG_REQUIRE(state.progression.level == 10U);
    ARPG_REQUIRE(state.progression.experience == 0U);
    ARPG_REQUIRE(state.progression.earned_passive_points == 9U);
    ARPG_REQUIRE(state.progression.unspent_passive_points == 6U);
    ARPG_REQUIRE(state.passive_tree.allocated_bits == 0x701ULL);
    ARPG_REQUIRE(state.abyss.lifecycle == arpg::abyss::AbyssLifecycle::cleared);
    ARPG_REQUIRE(state.abyss.danger == arpg::abyss::AbyssDanger::high);
    ARPG_REQUIRE(state.abyss.rule == arpg::abyss::AbyssRuleId::chaos_expansion);
    ARPG_REQUIRE(state.abyss.rules_version == 1U);
    ARPG_REQUIRE(state.abyss.reward_total == 3U);
    ARPG_REQUIRE(state.abyss.generated_mask == 3U);
    ARPG_REQUIRE(state.abyss.claimed_mask == 1U);
    ARPG_REQUIRE(state.abyss.abandoned_mask == 4U);
    ARPG_REQUIRE(state.abyss.reward_revision == 0x11223344U);
    ARPG_REQUIRE(state.last_abyss_resolution.valid);
    ARPG_REQUIRE(state.last_abyss_resolution.room_seed == 321U);
    ARPG_REQUIRE(state.last_abyss_resolution.rule
        == arpg::abyss::AbyssRuleId::chaos_expansion);
    ARPG_REQUIRE(state.last_abyss_resolution.total == 3U);
    ARPG_REQUIRE(state.last_abyss_resolution.generated == 2U);
    ARPG_REQUIRE(state.last_abyss_resolution.claimed == 1U);
    ARPG_REQUIRE(state.last_abyss_resolution.abandoned == 1U);

    ARPG_REQUIRE(state.item_ownership.items.size() == 3U);
    ARPG_REQUIRE((state.item_ownership.materials
        == std::array<std::uint64_t, arpg::items::kMaterialCount>{}));
    ARPG_REQUIRE(state.item_ownership.material_discovery_bits == 0U);
    ARPG_REQUIRE(state.item_ownership.next_item_sequence
        == 0x8182838485868788ULL);
    ARPG_REQUIRE((state.item_ownership.claimed_drop_bits
        == std::array<std::uint64_t, 3U>{{
            0x9192939495969798ULL, 0xA1A2A3A4A5A6A7A8ULL,
            0xB1B2B3B4B5B6B7B8ULL}}));
    ARPG_REQUIRE((state.item_ownership.equipment.equipped_ids
        == std::array<std::uint64_t, 6U>{{
            0x6162636465666768ULL, 0U, 0U, 0U, 0U,
            0x7172737475767778ULL}}));
    const auto& normal = state.item_ownership.items[0];
    ARPG_REQUIRE(normal.id == 0x5152535455565758ULL && normal.base_id == 2U);
    ARPG_REQUIRE(normal.rarity == arpg::items::ItemRarity::normal
        && normal.item_level == 1U && normal.required_level == 1U
        && normal.affix_count == 0U);
    const auto& magic = state.item_ownership.items[1];
    ARPG_REQUIRE(magic.id == 0x6162636465666768ULL && magic.base_id == 1U);
    ARPG_REQUIRE(magic.rarity == arpg::items::ItemRarity::magic
        && magic.item_level == 1U && magic.required_level == 1U
        && magic.affix_count == 1U);
    ARPG_REQUIRE(magic.affixes[0].affix_id == 1U
        && magic.affixes[0].tier == 8U && magic.affixes[0].variant == 0xFFU
        && magic.affixes[0].value_roll_bp
            == arpg::items::kAffixValueRollCanonicalBp);
    const auto& rare = state.item_ownership.items[2];
    ARPG_REQUIRE(rare.id == 0x7172737475767778ULL && rare.base_id == 6U);
    ARPG_REQUIRE(rare.rarity == arpg::items::ItemRarity::rare
        && rare.item_level == 1U && rare.required_level == 1U
        && rare.affix_count == 3U);
    ARPG_REQUIRE(rare.affixes[0].affix_id == 7U
        && rare.affixes[0].tier == 8U && rare.affixes[0].variant == 0xFFU);
    ARPG_REQUIRE(rare.affixes[1].affix_id == 111U
        && rare.affixes[1].tier == 8U && rare.affixes[1].variant == 0xFFU);
    ARPG_REQUIRE(rare.affixes[2].affix_id == 112U
        && rare.affixes[2].tier == 8U && rare.affixes[2].variant == 3U
        && rare.affixes[2].value_roll_bp
            == arpg::items::kAffixValueRollCanonicalBp);
    for (const auto& roll : normal.affixes) {
        ARPG_REQUIRE(roll.affix_id == 0U
            && roll.tier == 0U && roll.variant == 0U);
    }
    for (std::size_t index = 1U; index < magic.affixes.size(); ++index) {
        ARPG_REQUIRE(magic.affixes[index].affix_id == 0U
            && magic.affixes[index].tier == 0U
            && magic.affixes[index].variant == 0U);
    }
    for (std::size_t index = 3U; index < rare.affixes.size(); ++index) {
        ARPG_REQUIRE(rare.affixes[index].affix_id == 0U
            && rare.affixes[index].tier == 0U
            && rare.affixes[index].variant == 0U);
    }
    ARPG_REQUIRE(std::all_of(normal.reserved.begin(), normal.reserved.end(),
        [](std::uint8_t value) noexcept { return value == 0U; }));
    ARPG_REQUIRE(std::all_of(magic.reserved.begin(), magic.reserved.end(),
        [](std::uint8_t value) noexcept { return value == 0U; }));
    ARPG_REQUIRE(std::all_of(rare.reserved.begin(), rare.reserved.end(),
        [](std::uint8_t value) noexcept { return value == 0U; }));
    ARPG_REQUIRE(decoded.state.death_sequence == 0U);
    ARPG_REQUIRE(same_death(decoded.state.death, checkpoint::DeathCheckpoint{}));

    const auto& v6 = arpg::test::fixtures::kV6FullState;
    const auto decoded_v6 = persistence::decode_checkpoint(v6.data(), v6.size());
    ARPG_REQUIRE(decoded_v6.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded_v6.migrated);
    ARPG_REQUIRE(decoded_v6.state.item_ownership.items.size() == 3U);
    ARPG_REQUIRE(decoded_v6.state.item_ownership.items[1].affixes[0].value_roll_bp
        == arpg::items::kAffixValueRollCanonicalBp);
    ARPG_REQUIRE(decoded_v6.state.item_ownership.items[1].reinforcement == 0U);
    ARPG_REQUIRE((decoded_v6.state.item_ownership.materials
        == std::array<std::uint64_t, arpg::items::kMaterialCount>{}));
    ARPG_REQUIRE(decoded_v6.state.item_ownership.material_discovery_bits == 0U);
    ARPG_REQUIRE(same_death(
        decoded_v6.state.death, checkpoint::DeathCheckpoint{}));
    return {};
}

arpg::test::Failure v8_death_enum_boolean_reserved_and_state_errors() noexcept {
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
    for (const std::size_t offset : std::array<std::size_t, 11U>{{
             249U, 254U, 255U, 452U, 453U, 454U, 455U,
             456U, 457U, 458U, 459U}}) {
        ARPG_REQUIRE(rejects(
            offset, 1U, persistence::CodecError::invalid_state));
    }
    ARPG_REQUIRE(rejects(296U, 49U, persistence::CodecError::invalid_state));
    ARPG_REQUIRE(rejects(248U, 0xFFU, persistence::CodecError::invalid_state));
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
    auto max_hp_zero = *encoded;
    std::fill(max_hp_zero.begin() + 348U, max_hp_zero.begin() + 352U,
        static_cast<std::uint8_t>(0U));
    refresh_crc(max_hp_zero);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        max_hp_zero.data(), max_hp_zero.size()).error
        == persistence::CodecError::invalid_state);

    auto invalid = state;
    invalid.death.max_hp = -1;
    ARPG_REQUIRE(!persistence::encode_checkpoint(invalid).has_value());
    invalid = state;
    invalid.death.target_room.depth = 5U;
    ARPG_REQUIRE(!persistence::encode_checkpoint(invalid).has_value());
    return {};
}

arpg::test::Failure v8_lengths_crc_and_magic_are_rejected() noexcept {
    const auto encoded = persistence::encode_checkpoint(make_state());
    ARPG_REQUIRE(encoded.has_value());
    for (std::size_t size = 236U; size < 788U; ++size) {
        ARPG_REQUIRE(persistence::decode_checkpoint(encoded->data(), size).error
            == persistence::CodecError::bad_payload_length);
    }
    auto trailing = *encoded;
    trailing.push_back(0U);
    ARPG_REQUIRE(persistence::decode_checkpoint(trailing.data(), trailing.size()).error
        == persistence::CodecError::bad_payload_length);
    auto bad_length = *encoded;
    write_u32(bad_length, 24U, 755U);
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
    {"v8 layout and full round trip", &v8_layout_and_full_round_trip},
    {"v5 fixture migrates to canonical none", &v5_fixture_migrates_to_canonical_none},
    {"v8 death enum boolean reserved and state errors", &v8_death_enum_boolean_reserved_and_state_errors},
    {"v8 lengths crc and magic are rejected", &v8_lengths_crc_and_magic_are_rejected},
};

}  // namespace

arpg::test::TestSuite death_checkpoint_codec_suite() noexcept {
    return arpg::test::make_suite("death_checkpoint_codec", kCases);
}
