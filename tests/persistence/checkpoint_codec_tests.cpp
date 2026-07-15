#include "test_framework.hpp"

#include "persistence/checkpoint_codec.hpp"
#include "items/item_types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

namespace {
bool gFailNextAllocation = false;
}

void* operator new(std::size_t size) {
    if (gFailNextAllocation) {
        gFailNextAllocation = false;
        throw std::bad_alloc{};
    }
    if (void* memory = std::malloc(size == 0U ? 1U : size))
        return memory;
    throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
namespace persistence = arpg::persistence;
namespace items = arpg::items;

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

items::ItemInstance normal_item(std::uint64_t id, std::uint8_t base_id) noexcept {
    items::ItemInstance item{};
    item.id = id;
    item.base_id = base_id;
    item.rarity = items::ItemRarity::normal;
    item.item_level = 1U;
    item.required_level = 1U;
    return item;
}

items::ItemInstance magic_weapon(std::uint64_t id) noexcept {
    auto item = normal_item(id, 1U);
    item.rarity = items::ItemRarity::magic;
    item.affix_count = 1U;
    item.affixes[0] = {1U, 8U, 0xFFU};
    return item;
}

items::ItemInstance rare_accessory(std::uint64_t id) noexcept {
    auto item = normal_item(id, 6U);
    item.rarity = items::ItemRarity::rare;
    item.affix_count = 3U;
    item.affixes[0] = {7U, 8U, 0xFFU};
    item.affixes[1] = {111U, 8U, 0xFFU};
    item.affixes[2] = {112U, 8U, 3U};
    return item;
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes,
    std::size_t offset) noexcept {
    std::uint32_t value{};
    for (std::size_t index = 0U; index < 4U; ++index)
        value |= static_cast<std::uint32_t>(bytes[offset + index])
            << (index * 8U);
    return value;
}

std::uint64_t read_u64(const std::vector<std::uint8_t>& bytes,
    std::size_t offset) noexcept {
    std::uint64_t value{};
    for (std::size_t index = 0U; index < 8U; ++index)
        value |= static_cast<std::uint64_t>(bytes[offset + index])
            << (index * 8U);
    return value;
}

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
    std::uint32_t value) noexcept {
    for (std::size_t index = 0U; index < 4U; ++index)
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
}

arpg::test::Failure v4_golden_layout_and_items_round_trip() noexcept {
    auto state = make_fixture();
    state.item_ownership.items = {
        normal_item(0x0102030405060708ULL, 2U),
        magic_weapon(0x1112131415161718ULL),
        rare_accessory(0x2122232425262728ULL)};
    state.item_ownership.next_item_sequence = 0x3132333435363738ULL;
    state.item_ownership.claimed_drop_bits = {{
        0x4142434445464748ULL, 0x5152535455565758ULL,
        0x6162636465666768ULL}};
    state.item_ownership.equipment.equipped_ids[0] =
        state.item_ownership.items[1].id;
    state.item_ownership.equipment.equipped_ids[5] =
        state.item_ownership.items[2].id;

    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    const auto& bytes = *encoded;
    ARPG_REQUIRE(bytes.size() == 324U);
    ARPG_REQUIRE(std::equal(bytes.begin(), bytes.begin() + 8U,
        std::array<std::uint8_t, 8U>{{'I','A','R','P','G','S','0','5'}}.begin()));
    ARPG_REQUIRE(read_u32(bytes, 8U) == 4U);
    ARPG_REQUIRE(read_u32(bytes, 12U) == 1U);
    ARPG_REQUIRE(read_u32(bytes, 24U) == 292U);
    ARPG_REQUIRE(read_u32(bytes, 120U) == 3U);
    ARPG_REQUIRE(read_u64(bytes, 124U) == state.item_ownership.next_item_sequence);
    ARPG_REQUIRE(read_u64(bytes, 132U) == state.item_ownership.claimed_drop_bits[0]);
    ARPG_REQUIRE(read_u64(bytes, 156U) == state.item_ownership.equipment.equipped_ids[0]);
    ARPG_REQUIRE(read_u64(bytes, 196U) == state.item_ownership.equipment.equipped_ids[5]);

    constexpr std::size_t kRecord = 204U;
    ARPG_REQUIRE(read_u64(bytes, kRecord) == state.item_ownership.items[0].id);
    ARPG_REQUIRE(bytes[kRecord + 8U] == 2U);
    ARPG_REQUIRE(bytes[kRecord + 9U] == 0U);
    ARPG_REQUIRE(bytes[kRecord + 12U] == 0U);
    ARPG_REQUIRE(bytes[kRecord + 13U] == 0U
        && bytes[kRecord + 14U] == 0U && bytes[kRecord + 15U] == 0U);
    constexpr std::size_t kRareRecord = kRecord + 80U;
    ARPG_REQUIRE(bytes[kRareRecord + 12U] == 3U);
    ARPG_REQUIRE(bytes[kRareRecord + 16U] == 7U);
    ARPG_REQUIRE(bytes[kRareRecord + 18U] == 8U
        && bytes[kRareRecord + 19U] == 0xFFU);
    ARPG_REQUIRE(bytes[kRareRecord + 24U] == 112U
        && bytes[kRareRecord + 25U] == 0U);
    ARPG_REQUIRE(bytes[kRareRecord + 26U] == 8U
        && bytes[kRareRecord + 27U] == 3U);
    ARPG_REQUIRE(std::all_of(bytes.begin() + kRareRecord + 28U,
        bytes.begin() + kRareRecord + 40U,
        [](std::uint8_t value) noexcept { return value == 0U; }));

    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.state.item_ownership.items.size() == 3U);
    ARPG_REQUIRE(decoded.state.item_ownership.items[2].affixes[2].affix_id == 112U);
    ARPG_REQUIRE(decoded.state.item_ownership.items[2].affixes[2].variant == 3U);
    ARPG_REQUIRE(decoded.state.item_ownership.next_item_sequence
        == state.item_ownership.next_item_sequence);
    return {};
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

bool encoded_fixture(std::vector<std::uint8_t>& bytes) noexcept {
    const auto encoded = persistence::encode_checkpoint(make_fixture());
    if (!encoded.has_value())
        return false;
    bytes = *encoded;
    return true;
}

void refresh_crc(std::vector<std::uint8_t>& bytes) noexcept {
    const auto payload_size = read_u32(bytes, 24U);
    auto checksum = persistence::crc32_update(0U, bytes.data() + 8U, 20U);
    checksum = persistence::crc32_update(
        checksum, bytes.data() + 32U, payload_size);
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
    const auto bytes = persistence::encode_checkpoint(make_fixture());
    ARPG_REQUIRE(bytes.has_value());
    ARPG_REQUIRE(bytes->size() == 204U);
    ARPG_REQUIRE((*bytes)[0U] == 0x49U && (*bytes)[7U] == 0x35U);
    ARPG_REQUIRE((*bytes)[106U] == 0x01U && (*bytes)[107U] == 0x00U);

    const auto decoded = persistence::decode_checkpoint(
        kBaselineBytes.data(), kBaselineBytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(same_state(decoded.state, make_fixture()));
    ARPG_REQUIRE(decoded.state.passive_tree.allocated_bits == 1ULL);

    std::vector<std::uint8_t> format_three(bytes->begin(), bytes->begin() + 120U);
    format_three[7U] = '4';
    write_u32(format_three, 8U, 3U);
    write_u32(format_three, 24U, 88U);
    refresh_crc(format_three);
    const auto migrated_three = persistence::decode_checkpoint(
        format_three.data(), format_three.size());
    ARPG_REQUIRE(migrated_three.error == persistence::CodecError::none);
    ARPG_REQUIRE(migrated_three.state.item_ownership.items.empty());
    ARPG_REQUIRE(migrated_three.state.item_ownership.next_item_sequence == 1U);
    return {};
}

arpg::test::Failure all_nonzero_fields_round_trip() noexcept {
    std::vector<std::uint8_t> bytes;
    ARPG_REQUIRE(encoded_fixture(bytes));
    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(same_state(decoded.state, make_fixture()));
    return {};
}

arpg::test::Failure encoded_sizes_and_generation_are_little_endian() noexcept {
    static_assert(persistence::kCheckpointHeaderSize == 32U);
    static_assert(persistence::kV4BasePayloadSize == 172U);
    static_assert(persistence::kV4BaseEncodedCheckpointSize == 204U);

    std::vector<std::uint8_t> bytes;
    ARPG_REQUIRE(encoded_fixture(bytes));
    ARPG_REQUIRE(bytes.size() == 204U);
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
    const auto* input = reinterpret_cast<const std::uint8_t*>(kInput);
    ARPG_REQUIRE(persistence::crc32(input, 9U) == 0xCBF43926U);
    const auto prefix = persistence::crc32_update(0U, input, 4U);
    ARPG_REQUIRE(persistence::crc32_update(prefix, input + 4U, 5U)
        == persistence::crc32(input, 9U));
    ARPG_REQUIRE(persistence::crc32_update(prefix, nullptr, 5U) == prefix);
    ARPG_REQUIRE(persistence::crc32_update(prefix, input, 0U) == prefix);
    ARPG_REQUIRE(persistence::crc32(nullptr, 9U) == 0U);

    std::vector<std::uint8_t> bytes;
    ARPG_REQUIRE(encoded_fixture(bytes));
    bytes[32] ^= 0x01U;
    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::bad_crc);
    return {};
}

arpg::test::Failure wrong_magic_is_rejected() noexcept {
    std::vector<std::uint8_t> bytes;
    ARPG_REQUIRE(encoded_fixture(bytes));
    bytes[0] ^= 0x01U;
    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::bad_magic);
    return {};
}

arpg::test::Failure unsupported_format_and_rules_are_rejected() noexcept {
    std::vector<std::uint8_t> bytes;
    ARPG_REQUIRE(encoded_fixture(bytes));
    bytes[8] = 5U;
    auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::unsupported_format);

    ARPG_REQUIRE(encoded_fixture(bytes));
    bytes[12] = 2U;
    decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::unsupported_rules);
    return {};
}

arpg::test::Failure invalid_payload_length_is_rejected() noexcept {
    std::vector<std::uint8_t> bytes;
    ARPG_REQUIRE(encoded_fixture(bytes));
    bytes[24] = 63U;
    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::bad_payload_length);
    return {};
}

arpg::test::Failure invalid_enum_values_are_rejected() noexcept {
    constexpr std::array<std::size_t, 4> kOffsets{{88U, 89U, 92U, 93U}};
    for (const auto offset : kOffsets) {
        std::vector<std::uint8_t> bytes;
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
        std::vector<std::uint8_t> bytes;
        ARPG_REQUIRE(encoded_fixture(bytes));
        bytes[offset] = 2U;
        refresh_crc(bytes);
        const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
        ARPG_REQUIRE(decoded.error == persistence::CodecError::invalid_boolean);
    }

    for (const auto offset : std::array<std::size_t, 3>{{16U, 72U, 80U}}) {
        std::vector<std::uint8_t> bytes;
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
    const auto current = persistence::encode_checkpoint(legacy_state);
    if (!current.has_value())
        return {};
    std::array<std::uint8_t, persistence::kLegacyEncodedCheckpointSize> legacy{};
    std::copy_n(current->begin(), legacy.size(), legacy.begin());
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
    std::vector<std::uint8_t> bytes;
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
    ARPG_REQUIRE(decoded.state.passive_tree.allocated_bits == 1ULL);
    ARPG_REQUIRE(decoded.state.item_ownership.items.empty());
    ARPG_REQUIRE(decoded.state.item_ownership.equipment.equipped_ids
        == items::EquipmentState{}.equipped_ids);
    const std::array<std::uint64_t, 3U> no_claimed_drops{};
    ARPG_REQUIRE(decoded.state.item_ownership.claimed_drop_bits
        == no_claimed_drops);
    ARPG_REQUIRE(decoded.state.item_ownership.next_item_sequence == 1U);
    return {};
}

checkpoint::DungeonRunState make_owned_fixture() {
    auto state = make_fixture();
    state.item_ownership.items = {
        normal_item(101U, 2U), magic_weapon(102U), rare_accessory(103U)};
    state.item_ownership.equipment.equipped_ids[0] = 102U;
    state.item_ownership.equipment.equipped_ids[5] = 103U;
    state.item_ownership.claimed_drop_bits = {{1U, 2U, 4U}};
    state.item_ownership.next_item_sequence = 104U;
    return state;
}

arpg::test::Failure v4_length_count_crc_and_capacity_are_bounded() noexcept {
    const auto encoded = persistence::encode_checkpoint(make_owned_fixture());
    ARPG_REQUIRE(encoded.has_value());

    auto truncated = *encoded;
    truncated.pop_back();
    ARPG_REQUIRE(persistence::decode_checkpoint(
        truncated.data(), truncated.size()).error
        == persistence::CodecError::bad_payload_length);

    auto trailing = *encoded;
    trailing.push_back(0U);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        trailing.data(), trailing.size()).error
        == persistence::CodecError::bad_payload_length);

    auto bad_crc = *encoded;
    bad_crc[204U] ^= 1U;
    ARPG_REQUIRE(persistence::decode_checkpoint(
        bad_crc.data(), bad_crc.size()).error == persistence::CodecError::bad_crc);

    auto excessive_count = *encoded;
    write_u32(excessive_count, 120U, 65536U);
    refresh_crc(excessive_count);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        excessive_count.data(), excessive_count.size()).error
        == persistence::CodecError::bad_payload_length);

    auto overflowing_length = *encoded;
    write_u32(overflowing_length, 24U, 0xFFFFFFFFU);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        overflowing_length.data(), overflowing_length.size()).error
        == persistence::CodecError::bad_payload_length);

    auto maximum = make_fixture();
    maximum.item_ownership.items.reserve(65535U);
    for (std::uint32_t id = 1U; id <= 65535U; ++id)
        maximum.item_ownership.items.push_back(normal_item(id, 1U));
    maximum.item_ownership.next_item_sequence = 65536U;
    const auto maximum_encoded = persistence::encode_checkpoint(maximum);
    ARPG_REQUIRE(maximum_encoded.has_value());
    ARPG_REQUIRE(maximum_encoded->size() == 2621604U);
    const auto maximum_decoded = persistence::decode_checkpoint(
        maximum_encoded->data(), maximum_encoded->size());
    ARPG_REQUIRE(maximum_decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(maximum_decoded.state.item_ownership.items.size() == 65535U);

    auto too_many = maximum;
    too_many.item_ownership.items.push_back(normal_item(65536U, 1U));
    ARPG_REQUIRE(!persistence::encode_checkpoint(too_many).has_value());
    return {};
}

arpg::test::Failure v4_corrupt_item_semantics_are_rejected() noexcept {
    const auto encoded = persistence::encode_checkpoint(make_owned_fixture());
    ARPG_REQUIRE(encoded.has_value());
    const auto rejects = [&encoded](std::size_t offset,
        std::uint8_t value) noexcept {
        auto bytes = *encoded;
        bytes[offset] = value;
        refresh_crc(bytes);
        return persistence::decode_checkpoint(bytes.data(), bytes.size()).error
            == persistence::CodecError::invalid_state;
    };

    ARPG_REQUIRE(rejects(217U, 1U));
    ARPG_REQUIRE(rejects(220U, 1U));
    ARPG_REQUIRE(rejects(204U, 0U));
    ARPG_REQUIRE(rejects(244U, 101U));
    ARPG_REQUIRE(rejects(212U, 0U));
    ARPG_REQUIRE(rejects(214U, 0U));
    ARPG_REQUIRE(rejects(215U, 96U));
    ARPG_REQUIRE(rejects(304U, 7U));
    ARPG_REQUIRE(rejects(261U, 7U));
    ARPG_REQUIRE(rejects(311U, 4U));

    auto dangling = *encoded;
    dangling[156U] = 0xFEU;
    refresh_crc(dangling);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        dangling.data(), dangling.size()).error
        == persistence::CodecError::invalid_state);

    auto wrong_slot = *encoded;
    std::copy_n(wrong_slot.begin() + 156U, 8U, wrong_slot.begin() + 164U);
    refresh_crc(wrong_slot);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        wrong_slot.data(), wrong_slot.size()).error
        == persistence::CodecError::invalid_state);

    auto duplicate_equipment = *encoded;
    std::copy_n(duplicate_equipment.begin() + 156U, 8U,
        duplicate_equipment.begin() + 196U);
    refresh_crc(duplicate_equipment);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        duplicate_equipment.data(), duplicate_equipment.size()).error
        == persistence::CodecError::invalid_state);

    auto zero_sequence = *encoded;
    std::fill(zero_sequence.begin() + 124U, zero_sequence.begin() + 132U,
        static_cast<std::uint8_t>(0U));
    refresh_crc(zero_sequence);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        zero_sequence.data(), zero_sequence.size()).error
        == persistence::CodecError::invalid_state);
    return {};
}

arpg::test::Failure codec_allocation_failures_do_not_escape_noexcept() noexcept {
    const auto encoded = persistence::encode_checkpoint(make_owned_fixture());
    ARPG_REQUIRE(encoded.has_value());

    auto excessive_count = *encoded;
    write_u32(excessive_count, 120U, 65536U);
    refresh_crc(excessive_count);
    gFailNextAllocation = true;
    const auto rejected_before_allocation = persistence::decode_checkpoint(
        excessive_count.data(), excessive_count.size());
    ARPG_REQUIRE(rejected_before_allocation.error
        == persistence::CodecError::bad_payload_length);
    ARPG_REQUIRE(gFailNextAllocation);
    gFailNextAllocation = false;

    gFailNextAllocation = true;
    const auto decoded = persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::allocation_failure);
    ARPG_REQUIRE(!gFailNextAllocation);

    gFailNextAllocation = true;
    const auto failed_encode = persistence::encode_checkpoint(make_fixture());
    ARPG_REQUIRE(!failed_encode.has_value());
    ARPG_REQUIRE(!gFailNextAllocation);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"v4 golden layout and items round trip", &v4_golden_layout_and_items_round_trip},
    {"v4 length count crc and capacity are bounded", &v4_length_count_crc_and_capacity_are_bounded},
    {"v4 corrupt item semantics are rejected", &v4_corrupt_item_semantics_are_rejected},
    {"codec allocation failures do not escape noexcept", &codec_allocation_failures_do_not_escape_noexcept},
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
