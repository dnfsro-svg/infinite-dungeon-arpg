#include "test_framework.hpp"

#include "abyss/abyss_rules.hpp"
#include "dungeon/abyss_checkpoint_migration.hpp"
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
int gAllocationsBeforeFailure = -1;

#if defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL != 0
constexpr int kVectorProxyAllocations = 1;
#else
constexpr int kVectorProxyAllocations = 0;
#endif

bool fail_test_allocation() noexcept {
    if (gAllocationsBeforeFailure < 0)
        return false;
    if (gAllocationsBeforeFailure == 0) {
        gAllocationsBeforeFailure = -1;
        return true;
    }
    --gAllocationsBeforeFailure;
    return false;
}

void* allocate_for_test(std::size_t size) {
    if (fail_test_allocation())
        throw std::bad_alloc{};
    if (void* memory = std::malloc(size == 0U ? 1U : size))
        return memory;
    throw std::bad_alloc{};
}
}

void* operator new(std::size_t size) {
    return allocate_for_test(size);
}

void* operator new[](std::size_t size) {
    return allocate_for_test(size);
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
namespace abyss = arpg::abyss;
namespace dungeon = arpg::dungeon;
namespace persistence = arpg::persistence;
namespace items = arpg::items;

checkpoint::DungeonRunState make_owned_fixture();
std::array<std::uint8_t, persistence::kLegacyEncodedCheckpointSize>
legacy_fixture() noexcept;

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
    state.current_room.is_abyss = false;
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

arpg::test::Failure v5_golden_layout_and_items_round_trip() noexcept {
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
    ARPG_REQUIRE(bytes.size() == 580U);
    ARPG_REQUIRE(std::equal(bytes.begin(), bytes.begin() + 8U,
        std::array<std::uint8_t, 8U>{{'A','R','P','G','S','V','6','\0'}}.begin()));
    ARPG_REQUIRE(read_u32(bytes, 8U) == 6U);
    ARPG_REQUIRE(read_u32(bytes, 12U) == 1U);
    ARPG_REQUIRE(read_u32(bytes, 24U) == 548U);
    ARPG_REQUIRE(read_u32(bytes, 152U) == 3U);
    ARPG_REQUIRE(read_u64(bytes, 156U) == state.item_ownership.next_item_sequence);
    ARPG_REQUIRE(read_u64(bytes, 164U) == state.item_ownership.claimed_drop_bits[0]);
    ARPG_REQUIRE(read_u64(bytes, 188U) == state.item_ownership.equipment.equipped_ids[0]);
    ARPG_REQUIRE(read_u64(bytes, 228U) == state.item_ownership.equipment.equipped_ids[5]);

    constexpr std::size_t kRecord = 460U;
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

arpg::test::Failure v5_complete_ownership_and_six_roll_record_are_golden() noexcept {
    auto state = make_fixture();
    auto item = normal_item(0x0102030405060708ULL, 6U);
    item.rarity = items::ItemRarity::rare;
    item.item_level = 95U;
    item.required_level = 95U;
    item.affix_count = 6U;
    item.affixes[0] = {7U, 1U, 0xFFU};
    item.affixes[1] = {8U, 2U, 0xFFU};
    item.affixes[2] = {11U, 3U, 0xFFU};
    item.affixes[3] = {101U, 4U, 0xFFU};
    item.affixes[4] = {111U, 5U, 0xFFU};
    item.affixes[5] = {112U, 6U, 2U};
    state.item_ownership.items = {item};
    state.item_ownership.next_item_sequence = 0x1112131415161718ULL;
    state.item_ownership.claimed_drop_bits = {{
        0x2122232425262728ULL,
        0x3132333435363738ULL,
        0x4142434445464748ULL}};
    state.item_ownership.equipment.equipped_ids[5] = item.id;

    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    ARPG_REQUIRE(encoded->size() == 500U);
    constexpr std::array<std::uint8_t, 84U> kExpectedOwnership{{
        0x01U, 0x00U, 0x00U, 0x00U,
        0x18U, 0x17U, 0x16U, 0x15U, 0x14U, 0x13U, 0x12U, 0x11U,
        0x28U, 0x27U, 0x26U, 0x25U, 0x24U, 0x23U, 0x22U, 0x21U,
        0x38U, 0x37U, 0x36U, 0x35U, 0x34U, 0x33U, 0x32U, 0x31U,
        0x48U, 0x47U, 0x46U, 0x45U, 0x44U, 0x43U, 0x42U, 0x41U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x08U, 0x07U, 0x06U, 0x05U, 0x04U, 0x03U, 0x02U, 0x01U,
    }};
    constexpr std::array<std::uint8_t, 40U> kExpectedRecord{{
        0x08U, 0x07U, 0x06U, 0x05U, 0x04U, 0x03U, 0x02U, 0x01U,
        0x06U, 0x02U, 0x5FU, 0x5FU, 0x06U, 0x00U, 0x00U, 0x00U,
        0x07U, 0x00U, 0x01U, 0xFFU,
        0x08U, 0x00U, 0x02U, 0xFFU,
        0x0BU, 0x00U, 0x03U, 0xFFU,
        0x65U, 0x00U, 0x04U, 0xFFU,
        0x6FU, 0x00U, 0x05U, 0xFFU,
        0x70U, 0x00U, 0x06U, 0x02U,
    }};
    ARPG_REQUIRE(std::equal(kExpectedOwnership.begin(), kExpectedOwnership.end(),
        encoded->begin() + 152U));
    ARPG_REQUIRE(std::equal(kExpectedRecord.begin(), kExpectedRecord.end(),
        encoded->begin() + 460U));
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

std::vector<std::uint8_t> as_v4_golden(
    const std::vector<std::uint8_t>& v6) {
    auto v5 = v6;
    v5.erase(v5.begin() + 236U, v5.begin() + 460U);
    v5[0U] = 'I'; v5[1U] = 'A'; v5[2U] = 'R'; v5[3U] = 'P';
    v5[4U] = 'G'; v5[5U] = 'S'; v5[6U] = '0'; v5[7U] = '6';
    write_u32(v5, 8U, 5U);
    write_u32(v5, 24U, static_cast<std::uint32_t>(v5.size() - 32U));
    refresh_crc(v5);
    std::vector<std::uint8_t> v4(v5.size() - 32U, 0U);
    std::copy_n(v5.begin(), 120U, v4.begin());
    std::copy(v5.begin() + 152U, v5.end(), v4.begin() + 120U);
    v4[7U] = '5';
    write_u32(v4, 8U, 4U);
    write_u32(v4, 24U, static_cast<std::uint32_t>(v4.size() - 32U));
    refresh_crc(v4);
    return v4;
}

std::uint8_t reward_total_for_test(abyss::AbyssDanger danger) noexcept;
checkpoint::DungeonRunState state_for_danger(
    abyss::AbyssDanger danger,
    abyss::AbyssLifecycle lifecycle) noexcept;

void set_abyss(checkpoint::DungeonRunState& state,
    abyss::AbyssLifecycle lifecycle) noexcept {
    while (!abyss::is_abyss_roll(state.current_room.seed)) {
        ++state.current_room.seed;
    }
    const auto selection = abyss::select_abyss_rule(
        state.current_room.seed, state.current_room.depth);
    if (!selection.has_value())
        return;
    state.current_room.is_abyss = true;
    state.current_room.entry = checkpoint::EntrySide::right;
    state.last_transition = checkpoint::TransitionKind::door;
    state.last_direction = checkpoint::ExitDirection::left;
    state.abyss.lifecycle = lifecycle;
    state.abyss.danger = selection->danger;
    state.abyss.rule = selection->rule;
    state.abyss.rules_version = selection->rules_version;
}

arpg::test::Failure v5_abyss_origin_requires_a_matching_door() noexcept {
    const auto valid = state_for_danger(
        abyss::AbyssDanger::low, abyss::AbyssLifecycle::available);
    ARPG_REQUIRE(persistence::encode_checkpoint(valid).has_value());

    for (const auto lifecycle : std::array<abyss::AbyssLifecycle, 3U>{{
             abyss::AbyssLifecycle::available,
             abyss::AbyssLifecycle::started,
             abyss::AbyssLifecycle::cleared}}) {
        auto initial = valid;
        set_abyss(initial, lifecycle);
        initial.current_room.entry = checkpoint::EntrySide::initial;
        initial.last_transition = checkpoint::TransitionKind::none;
        initial.last_direction = checkpoint::ExitDirection::none;
        if (lifecycle == abyss::AbyssLifecycle::cleared) {
            initial.abyss.reward_total = reward_total_for_test(
                initial.abyss.danger);
        }
        ARPG_REQUIRE(!persistence::encode_checkpoint(initial).has_value());

        auto descent = initial;
        descent.current_room.entry = checkpoint::EntrySide::initial;
        descent.last_transition = checkpoint::TransitionKind::descent;
        ARPG_REQUIRE(!persistence::encode_checkpoint(descent).has_value());

        auto mismatched = initial;
        mismatched.current_room.entry = checkpoint::EntrySide::left;
        mismatched.last_transition = checkpoint::TransitionKind::door;
        mismatched.last_direction = checkpoint::ExitDirection::left;
        ARPG_REQUIRE(!persistence::encode_checkpoint(mismatched).has_value());
    }

    auto none = make_fixture();
    none.current_room.is_abyss = true;
    ARPG_REQUIRE(!persistence::encode_checkpoint(none).has_value());

    auto failed = valid;
    failed.abyss.lifecycle = abyss::AbyssLifecycle::failed;
    failed.current_room.is_abyss = true;
    ARPG_REQUIRE(!persistence::encode_checkpoint(failed).has_value());

    const auto encoded = persistence::encode_checkpoint(valid);
    ARPG_REQUIRE(encoded.has_value());
    auto impossible_decode = *encoded;
    impossible_decode[88U] = static_cast<std::uint8_t>(
        checkpoint::EntrySide::initial);
    impossible_decode[92U] = static_cast<std::uint8_t>(
        checkpoint::TransitionKind::descent);
    impossible_decode[93U] = static_cast<std::uint8_t>(
        checkpoint::ExitDirection::none);
    refresh_crc(impossible_decode);
    ARPG_REQUIRE(persistence::decode_checkpoint(impossible_decode.data(),
        impossible_decode.size()).error == persistence::CodecError::invalid_state);
    return {};
}

std::uint64_t seed_for_danger(abyss::AbyssDanger danger,
    std::uint64_t depth = 40U) noexcept {
    for (std::uint64_t seed = 1U; seed < 100000U; ++seed) {
        const auto selection = abyss::select_abyss_rule(seed, depth);
        if (abyss::is_abyss_roll(seed)
                && selection.has_value() && selection->danger == danger)
            return seed;
    }
    return 0U;
}

std::uint8_t reward_total_for_test(abyss::AbyssDanger danger) noexcept {
    switch (danger) {
    case abyss::AbyssDanger::low: return 1U;
    case abyss::AbyssDanger::medium: return 2U;
    case abyss::AbyssDanger::high: return 3U;
    default: return 0U;
    }
}

checkpoint::DungeonRunState state_for_danger(
    abyss::AbyssDanger danger,
    abyss::AbyssLifecycle lifecycle) noexcept {
    auto state = make_fixture();
    state.current_room.depth = 40U;
    state.current_room.seed = seed_for_danger(danger, state.current_room.depth);
    set_abyss(state, lifecycle);
    return state;
}

arpg::test::Failure v5_full_abyss_state_and_resolution_round_trip() noexcept {
    auto state = state_for_danger(
        abyss::AbyssDanger::high, abyss::AbyssLifecycle::cleared);
    ARPG_REQUIRE(state.current_room.seed != 0U);
    state.abyss.reward_total = 3U;
    state.abyss.generated_mask = 0x03U;
    state.abyss.claimed_mask = 0x01U;
    state.abyss.abandoned_mask = 0x04U;
    state.abyss.reward_revision = 0x11223344U;
    state.last_abyss_resolution.valid = true;
    state.last_abyss_resolution.room_seed = state.current_room.seed;
    state.last_abyss_resolution.rule = state.abyss.rule;
    state.last_abyss_resolution.total = 3U;
    state.last_abyss_resolution.generated = 2U;
    state.last_abyss_resolution.claimed = 1U;
    state.last_abyss_resolution.abandoned = 1U;

    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    ARPG_REQUIRE(encoded->size() == persistence::kV6BaseEncodedCheckpointSize);
    ARPG_REQUIRE((*encoded)[7U] == '\0');
    ARPG_REQUIRE(read_u32(*encoded, 8U) == 6U);
    ARPG_REQUIRE(read_u32(*encoded, 24U) == persistence::kV6BasePayloadSize);
    ARPG_REQUIRE((*encoded)[120U]
        == static_cast<std::uint8_t>(abyss::AbyssLifecycle::cleared));
    ARPG_REQUIRE((*encoded)[121U]
        == static_cast<std::uint8_t>(state.abyss.danger));
    ARPG_REQUIRE((*encoded)[122U]
        == static_cast<std::uint8_t>(state.abyss.rule));
    ARPG_REQUIRE(read_u32(*encoded, 124U) == abyss::kAbyssRulesVersion);
    ARPG_REQUIRE((*encoded)[128U] == 3U && (*encoded)[129U] == 0x03U);
    ARPG_REQUIRE((*encoded)[130U] == 0x01U && (*encoded)[131U] == 0x04U);
    ARPG_REQUIRE(read_u32(*encoded, 132U) == 0x11223344U);
    ARPG_REQUIRE((*encoded)[136U] == 1U);
    ARPG_REQUIRE(read_u64(*encoded, 137U) == state.current_room.seed);
    ARPG_REQUIRE((*encoded)[145U] == static_cast<std::uint8_t>(state.abyss.rule));
    ARPG_REQUIRE((*encoded)[146U] == 3U && (*encoded)[147U] == 2U);
    ARPG_REQUIRE((*encoded)[148U] == 1U && (*encoded)[149U] == 1U);

    const auto decoded = persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(!decoded.migrated);
    ARPG_REQUIRE(decoded.state.abyss.lifecycle == state.abyss.lifecycle);
    ARPG_REQUIRE(decoded.state.abyss.rule == state.abyss.rule);
    ARPG_REQUIRE(decoded.state.abyss.reward_revision == 0x11223344U);
    ARPG_REQUIRE(decoded.state.last_abyss_resolution.valid);
    ARPG_REQUIRE(decoded.state.last_abyss_resolution.room_seed
        == state.current_room.seed);
    ARPG_REQUIRE(decoded.state.last_abyss_resolution.generated == 2U);
    return {};
}

arpg::test::Failure v5_cleared_reward_total_matches_each_danger() noexcept {
    constexpr std::array<abyss::AbyssDanger, 3U> kDangers{{
        abyss::AbyssDanger::low,
        abyss::AbyssDanger::medium,
        abyss::AbyssDanger::high}};
    for (const auto danger : kDangers) {
        auto state = state_for_danger(danger, abyss::AbyssLifecycle::cleared);
        ARPG_REQUIRE(state.current_room.seed != 0U);
        ARPG_REQUIRE(state.abyss.danger == danger);
        const auto expected_total = reward_total_for_test(danger);
        state.abyss.reward_total = expected_total;
        const auto encoded = persistence::encode_checkpoint(state);
        ARPG_REQUIRE(encoded.has_value());
        ARPG_REQUIRE(persistence::decode_checkpoint(
            encoded->data(), encoded->size()).error
            == persistence::CodecError::none);

        state.abyss.reward_total = static_cast<std::uint8_t>(
            expected_total == 3U ? 2U : expected_total + 1U);
        ARPG_REQUIRE(!persistence::encode_checkpoint(state).has_value());
        auto wrong_bytes = *encoded;
        wrong_bytes[128U] = state.abyss.reward_total;
        refresh_crc(wrong_bytes);
        ARPG_REQUIRE(persistence::decode_checkpoint(
            wrong_bytes.data(), wrong_bytes.size()).error
            == persistence::CodecError::invalid_state);
    }
    return {};
}

arpg::test::Failure v5_resolution_total_matches_each_rule_danger() noexcept {
    constexpr std::array<abyss::AbyssDanger, 3U> kDangers{{
        abyss::AbyssDanger::low,
        abyss::AbyssDanger::medium,
        abyss::AbyssDanger::high}};
    for (const auto danger : kDangers) {
        auto state = state_for_danger(danger, abyss::AbyssLifecycle::started);
        ARPG_REQUIRE(state.current_room.seed != 0U);
        ARPG_REQUIRE(state.abyss.danger == danger);
        const auto expected_total = reward_total_for_test(danger);
        state.last_abyss_resolution.valid = true;
        state.last_abyss_resolution.room_seed = state.current_room.seed + 1U;
        state.last_abyss_resolution.rule = state.abyss.rule;
        state.last_abyss_resolution.total = expected_total;
        state.last_abyss_resolution.generated = expected_total;
        const auto encoded = persistence::encode_checkpoint(state);
        ARPG_REQUIRE(encoded.has_value());
        ARPG_REQUIRE(persistence::decode_checkpoint(
            encoded->data(), encoded->size()).error
            == persistence::CodecError::none);

        const auto wrong_total = static_cast<std::uint8_t>(
            expected_total == 3U ? 2U : expected_total + 1U);
        state.last_abyss_resolution.total = wrong_total;
        state.last_abyss_resolution.generated = wrong_total;
        ARPG_REQUIRE(!persistence::encode_checkpoint(state).has_value());
        auto wrong_bytes = *encoded;
        wrong_bytes[146U] = wrong_total;
        wrong_bytes[147U] = wrong_total;
        refresh_crc(wrong_bytes);
        ARPG_REQUIRE(persistence::decode_checkpoint(
            wrong_bytes.data(), wrong_bytes.size()).error
            == persistence::CodecError::invalid_state);
    }
    return {};
}

arpg::test::Failure v5_started_state_survives_decode() noexcept {
    auto state = make_fixture();
    set_abyss(state, abyss::AbyssLifecycle::started);
    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    const auto decoded = persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.state.abyss.lifecycle == abyss::AbyssLifecycle::started);
    ARPG_REQUIRE(!decoded.migrated);
    return {};
}

arpg::test::Failure v5_abyss_enums_are_rejected_individually() noexcept {
    auto state = make_fixture();
    set_abyss(state, abyss::AbyssLifecycle::started);
    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    for (const auto offset : std::array<std::size_t, 4U>{{120U,121U,122U,145U}}) {
        auto bytes = *encoded;
        if (offset == 145U) {
            bytes[136U] = 1U;
            bytes[137U] = 1U;
            bytes[146U] = 1U;
            bytes[147U] = 1U;
        }
        bytes[offset] = 0xFEU;
        refresh_crc(bytes);
        ARPG_REQUIRE(persistence::decode_checkpoint(bytes.data(), bytes.size()).error
            == persistence::CodecError::invalid_enum);
    }
    return {};
}

arpg::test::Failure v5_resolution_boolean_is_rejected() noexcept {
    auto state = make_fixture();
    set_abyss(state, abyss::AbyssLifecycle::started);
    auto bytes = *persistence::encode_checkpoint(state);
    bytes[136U] = 2U;
    refresh_crc(bytes);
    ARPG_REQUIRE(persistence::decode_checkpoint(bytes.data(), bytes.size()).error
        == persistence::CodecError::invalid_boolean);
    return {};
}

arpg::test::Failure v5_reward_total_and_mask_bits_are_rejected() noexcept {
    auto state = state_for_danger(
        abyss::AbyssDanger::high, abyss::AbyssLifecycle::cleared);
    ARPG_REQUIRE(state.current_room.seed != 0U);
    state.abyss.reward_total = 3U;
    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    for (const auto mutation : std::array<std::array<std::uint8_t, 2U>, 2U>{{
            {{128U, 4U}}, {{129U, 8U}}}}) {
        auto bytes = *encoded;
        bytes[mutation[0]] = mutation[1];
        refresh_crc(bytes);
        ARPG_REQUIRE(persistence::decode_checkpoint(bytes.data(), bytes.size()).error
            == persistence::CodecError::invalid_state);
    }
    return {};
}

arpg::test::Failure v5_claimed_and_abandoned_masks_are_consistent() noexcept {
    auto state = state_for_danger(
        abyss::AbyssDanger::high, abyss::AbyssLifecycle::cleared);
    ARPG_REQUIRE(state.current_room.seed != 0U);
    state.abyss.reward_total = 3U;
    state.abyss.generated_mask = 1U;
    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    auto claimed = *encoded;
    claimed[130U] = 2U;
    refresh_crc(claimed);
    ARPG_REQUIRE(persistence::decode_checkpoint(claimed.data(), claimed.size()).error
        == persistence::CodecError::invalid_state);
    auto abandoned = *encoded;
    abandoned[131U] = 1U;
    refresh_crc(abandoned);
    ARPG_REQUIRE(persistence::decode_checkpoint(abandoned.data(), abandoned.size()).error
        == persistence::CodecError::invalid_state);
    return {};
}

arpg::test::Failure v5_rule_and_danger_must_match_deterministic_selection() noexcept {
    auto state = make_fixture();
    set_abyss(state, abyss::AbyssLifecycle::available);
    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    auto wrong_rule = *encoded;
    wrong_rule[122U] = static_cast<std::uint8_t>(
        state.abyss.rule == abyss::AbyssRuleId::thunderstorm
            ? abyss::AbyssRuleId::swift_pursuit
            : abyss::AbyssRuleId::thunderstorm);
    refresh_crc(wrong_rule);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        wrong_rule.data(), wrong_rule.size()).error
        == persistence::CodecError::invalid_state);
    auto wrong_danger = *encoded;
    wrong_danger[121U] = static_cast<std::uint8_t>(
        state.abyss.danger == abyss::AbyssDanger::low
            ? abyss::AbyssDanger::medium : abyss::AbyssDanger::low);
    refresh_crc(wrong_danger);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        wrong_danger.data(), wrong_danger.size()).error
        == persistence::CodecError::invalid_state);
    auto wrong_version = *encoded;
    write_u32(wrong_version, 124U, abyss::kAbyssRulesVersion + 1U);
    refresh_crc(wrong_version);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        wrong_version.data(), wrong_version.size()).error
        == persistence::CodecError::invalid_state);
    return {};
}

arpg::test::Failure v5_lifecycle_requires_matching_room_state() noexcept {
    auto state = make_fixture();
    state.current_room.is_abyss = false;
    set_abyss(state, abyss::AbyssLifecycle::available);
    state.current_room.is_abyss = false;
    ARPG_REQUIRE(!persistence::encode_checkpoint(state).has_value());
    state = make_fixture();
    state.abyss.rules_version = abyss::kAbyssRulesVersion;
    ARPG_REQUIRE(!persistence::encode_checkpoint(state).has_value());
    return {};
}

arpg::test::Failure v5_resolution_counts_are_validated() noexcept {
    auto state = state_for_danger(
        abyss::AbyssDanger::high, abyss::AbyssLifecycle::started);
    ARPG_REQUIRE(state.current_room.seed != 0U);
    state.last_abyss_resolution.valid = true;
    state.last_abyss_resolution.room_seed = 1U;
    state.last_abyss_resolution.rule = state.abyss.rule;
    state.last_abyss_resolution.total = 3U;
    state.last_abyss_resolution.generated = 2U;
    state.last_abyss_resolution.claimed = 1U;
    state.last_abyss_resolution.abandoned = 1U;
    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    auto impossible_total = *encoded;
    impossible_total[149U] = 0U;
    refresh_crc(impossible_total);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        impossible_total.data(), impossible_total.size()).error
        == persistence::CodecError::invalid_state);
    auto excessive_claimed = *encoded;
    excessive_claimed[148U] = 3U;
    refresh_crc(excessive_claimed);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        excessive_claimed.data(), excessive_claimed.size()).error
        == persistence::CodecError::invalid_state);
    return {};
}

arpg::test::Failure v5_crc_covers_abyss_fields() noexcept {
    auto state = make_fixture();
    set_abyss(state, abyss::AbyssLifecycle::started);
    auto bytes = *persistence::encode_checkpoint(state);
    bytes[132U] ^= 1U;
    ARPG_REQUIRE(persistence::decode_checkpoint(bytes.data(), bytes.size()).error
        == persistence::CodecError::bad_crc);
    return {};
}

arpg::test::Failure legacy_door_abyss_migrates_deterministically() noexcept {
    struct DoorCase final {
        checkpoint::ExitDirection direction{};
        checkpoint::EntrySide opposite{};
    };
    constexpr std::array<DoorCase, 4U> kDoors{{
        {checkpoint::ExitDirection::up, checkpoint::EntrySide::bottom},
        {checkpoint::ExitDirection::down, checkpoint::EntrySide::top},
        {checkpoint::ExitDirection::left, checkpoint::EntrySide::right},
        {checkpoint::ExitDirection::right, checkpoint::EntrySide::left}}};
    for (const auto& door : kDoors) {
        auto legacy = make_fixture();
        legacy.current_room.seed = 0x11E9U;
        ARPG_REQUIRE(abyss::is_abyss_roll(legacy.current_room.seed));
        legacy.current_room.entry = door.opposite;
        legacy.last_transition = checkpoint::TransitionKind::door;
        legacy.last_direction = door.direction;
        legacy.current_room.is_abyss = true;
        const auto migrated = dungeon::migrate_legacy_abyss_checkpoint(legacy);
        const auto expected = abyss::select_abyss_rule(
            legacy.current_room.seed, legacy.current_room.depth);
        ARPG_REQUIRE(expected.has_value());
        ARPG_REQUIRE(migrated.abyss.lifecycle
            == abyss::AbyssLifecycle::available);
        ARPG_REQUIRE(migrated.abyss.rule == expected->rule);
        ARPG_REQUIRE(migrated.abyss.danger == expected->danger);
        ARPG_REQUIRE(migrated.abyss.rules_version == abyss::kAbyssRulesVersion);
        ARPG_REQUIRE(migrated.current_room.is_abyss);
    }
    return {};
}

arpg::test::Failure legacy_door_abyss_requires_new_domain_roll() noexcept {
    auto legacy = make_owned_fixture();
    legacy.current_room.seed = 0x4142434445464748ULL;
    ARPG_REQUIRE(!abyss::is_abyss_roll(legacy.current_room.seed));
    legacy.current_room.entry = checkpoint::EntrySide::bottom;
    legacy.last_transition = checkpoint::TransitionKind::door;
    legacy.last_direction = checkpoint::ExitDirection::up;
    legacy.current_room.is_abyss = true;
    legacy.abyss.lifecycle = abyss::AbyssLifecycle::cleared;
    legacy.abyss.rule = abyss::AbyssRuleId::life_sacrifice;
    legacy.last_abyss_resolution.valid = true;

    auto expected = legacy;
    expected.current_room.is_abyss = false;
    expected.abyss = {};
    expected.last_abyss_resolution = {};
    const auto migrated = dungeon::migrate_legacy_abyss_checkpoint(legacy);
    ARPG_REQUIRE(same_state(migrated, expected));
    ARPG_REQUIRE(migrated.item_ownership.items.size()
        == expected.item_ownership.items.size());
    ARPG_REQUIRE(migrated.item_ownership.items[0].id
        == expected.item_ownership.items[0].id);
    ARPG_REQUIRE(migrated.item_ownership.equipment.equipped_ids
        == expected.item_ownership.equipment.equipped_ids);
    ARPG_REQUIRE(migrated.item_ownership.claimed_drop_bits
        == expected.item_ownership.claimed_drop_bits);
    ARPG_REQUIRE(migrated.item_ownership.next_item_sequence
        == expected.item_ownership.next_item_sequence);
    ARPG_REQUIRE(migrated.abyss.lifecycle == abyss::AbyssLifecycle::none);
    ARPG_REQUIRE(!migrated.last_abyss_resolution.valid);
    return {};
}

arpg::test::Failure legacy_door_migration_rejects_missing_or_mismatched_direction() noexcept {
    constexpr std::array<checkpoint::ExitDirection, 5U> kDirections{{
        checkpoint::ExitDirection::none,
        checkpoint::ExitDirection::up,
        checkpoint::ExitDirection::down,
        checkpoint::ExitDirection::left,
        checkpoint::ExitDirection::right}};
    constexpr std::array<checkpoint::EntrySide, 5U> kWrongEntries{{
        checkpoint::EntrySide::left,
        checkpoint::EntrySide::top,
        checkpoint::EntrySide::bottom,
        checkpoint::EntrySide::left,
        checkpoint::EntrySide::right}};
    for (std::size_t index = 0U; index < kDirections.size(); ++index) {
        auto legacy = make_fixture();
        legacy.current_room.is_abyss = true;
        legacy.last_transition = checkpoint::TransitionKind::door;
        legacy.last_direction = kDirections[index];
        legacy.current_room.entry = kWrongEntries[index];
        const auto migrated = dungeon::migrate_legacy_abyss_checkpoint(legacy);
        ARPG_REQUIRE(!migrated.current_room.is_abyss);
        ARPG_REQUIRE(migrated.abyss.lifecycle == abyss::AbyssLifecycle::none);
    }
    return {};
}

arpg::test::Failure legacy_initial_and_descent_abyss_are_cleared_without_drift() noexcept {
    for (const bool descent : std::array<bool, 2U>{{false, true}}) {
        auto legacy = make_owned_fixture();
        legacy.current_room.is_abyss = true;
        legacy.current_room.entry = descent
            ? checkpoint::EntrySide::left : checkpoint::EntrySide::initial;
        legacy.last_transition = descent
            ? checkpoint::TransitionKind::descent : checkpoint::TransitionKind::door;
        const auto migrated = dungeon::migrate_legacy_abyss_checkpoint(legacy);
        ARPG_REQUIRE(!migrated.current_room.is_abyss);
        ARPG_REQUIRE(migrated.abyss.lifecycle == abyss::AbyssLifecycle::none);
        ARPG_REQUIRE(migrated.root_seed == legacy.root_seed);
        ARPG_REQUIRE(migrated.commit_generation == legacy.commit_generation);
        ARPG_REQUIRE(migrated.biases == legacy.biases);
        ARPG_REQUIRE(migrated.current_room.seed == legacy.current_room.seed);
        ARPG_REQUIRE(migrated.current_room.index == legacy.current_room.index);
        ARPG_REQUIRE(migrated.current_room.depth == legacy.current_room.depth);
        ARPG_REQUIRE(migrated.current_room.floor_room_index
            == legacy.current_room.floor_room_index);
        ARPG_REQUIRE(migrated.current_room.ecology == legacy.current_room.ecology);
        ARPG_REQUIRE(migrated.current_room.has_hole == legacy.current_room.has_hole);
        ARPG_REQUIRE(migrated.progression.experience == legacy.progression.experience);
        ARPG_REQUIRE(migrated.progression.level == legacy.progression.level);
        ARPG_REQUIRE(migrated.progression.earned_passive_points
            == legacy.progression.earned_passive_points);
        ARPG_REQUIRE(migrated.progression.unspent_passive_points
            == legacy.progression.unspent_passive_points);
        ARPG_REQUIRE(migrated.passive_tree.allocated_bits
            == legacy.passive_tree.allocated_bits);
        ARPG_REQUIRE(migrated.item_ownership.items.size()
            == legacy.item_ownership.items.size());
        ARPG_REQUIRE(migrated.item_ownership.items[0].id
            == legacy.item_ownership.items[0].id);
        ARPG_REQUIRE(migrated.item_ownership.next_item_sequence
            == legacy.item_ownership.next_item_sequence);
    }
    auto ordinary = make_owned_fixture();
    ordinary.current_room.is_abyss = false;
    const auto migrated_ordinary = dungeon::migrate_legacy_abyss_checkpoint(
        ordinary);
    ARPG_REQUIRE(!migrated_ordinary.current_room.is_abyss);
    ARPG_REQUIRE(migrated_ordinary.abyss.lifecycle
        == abyss::AbyssLifecycle::none);
    return {};
}

arpg::test::Failure v1_through_v4_decode_as_migrated_but_v5_does_not() noexcept {
    const auto v1 = legacy_fixture();
    const auto decoded_v1 = persistence::decode_checkpoint(v1.data(), v1.size());
    ARPG_REQUIRE(decoded_v1.migrated);
    ARPG_REQUIRE(decoded_v1.state.death_sequence == 0U);
    ARPG_REQUIRE(checkpoint::valid_death_checkpoint_structural(decoded_v1.state.death));

    constexpr std::array<std::uint8_t, persistence::kPreviousEncodedCheckpointSize>
        v2{{
            0x49U,0x41U,0x52U,0x50U,0x47U,0x53U,0x30U,0x33U,
            0x02U,0x00U,0x00U,0x00U,0x01U,0x00U,0x00U,0x00U,
            0x18U,0x17U,0x16U,0x15U,0x14U,0x13U,0x12U,0x11U,
            0x50U,0x00U,0x00U,0x00U,0xC7U,0x37U,0x66U,0x35U,
            0x08U,0x07U,0x06U,0x05U,0x04U,0x03U,0x02U,0x01U,
            0x24U,0x23U,0x22U,0x21U,0x28U,0x27U,0x26U,0x25U,
            0x2CU,0x2BU,0x2AU,0x29U,0x30U,0x2FU,0x2EU,0x2DU,
            0x38U,0x37U,0x36U,0x35U,0x34U,0x33U,0x32U,0x31U,
            0x48U,0x47U,0x46U,0x45U,0x44U,0x43U,0x42U,0x41U,
            0x02U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,
            0x03U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,
            0x04U,0x03U,0x01U,0x01U,0x01U,0x02U,0x25U,0x24U,
            0x24U,0x00U,0x2AU,0x00U,0x00U,0x00U,0x00U,0x00U,
            0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U}};
    const auto decoded_v2 = persistence::decode_checkpoint(v2.data(), v2.size());
    ARPG_REQUIRE(decoded_v2.migrated);
    ARPG_REQUIRE(decoded_v2.state.death_sequence == 0U);
    ARPG_REQUIRE(checkpoint::valid_death_checkpoint_structural(decoded_v2.state.death));

    auto current = persistence::encode_checkpoint(make_fixture());
    ARPG_REQUIRE(current.has_value());
    auto v3 = std::vector<std::uint8_t>(current->begin(), current->begin() + 120U);
    const std::array<std::uint8_t, 8U> v3_magic{{'I','A','R','P','G','S','0','4'}};
    std::copy(v3_magic.begin(), v3_magic.end(), v3.begin());
    write_u32(v3, 8U, 3U);
    write_u32(v3, 24U, 88U);
    refresh_crc(v3);
    const auto decoded_v3 = persistence::decode_checkpoint(v3.data(), v3.size());
    ARPG_REQUIRE(decoded_v3.migrated);
    ARPG_REQUIRE(decoded_v3.state.death_sequence == 0U);
    ARPG_REQUIRE(checkpoint::valid_death_checkpoint_structural(decoded_v3.state.death));
    const auto v4 = as_v4_golden(*current);
    const auto decoded_v4 = persistence::decode_checkpoint(v4.data(), v4.size());
    ARPG_REQUIRE(decoded_v4.migrated);
    ARPG_REQUIRE(decoded_v4.state.death_sequence == 0U);
    ARPG_REQUIRE(checkpoint::valid_death_checkpoint_structural(decoded_v4.state.death));
    const auto current_owned = persistence::encode_checkpoint(make_owned_fixture());
    ARPG_REQUIRE(current_owned.has_value());
    const auto v4_owned = as_v4_golden(*current_owned);
    const auto decoded_v4_owned = persistence::decode_checkpoint(
        v4_owned.data(), v4_owned.size());
    ARPG_REQUIRE(decoded_v4_owned.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded_v4_owned.migrated);
    ARPG_REQUIRE(decoded_v4_owned.state.item_ownership.items.size() == 3U);
    ARPG_REQUIRE(!persistence::decode_checkpoint(
        current->data(), current->size()).migrated);
    return {};
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
    ARPG_REQUIRE(bytes->size() == 460U);
    ARPG_REQUIRE((*bytes)[0U] == 'A' && (*bytes)[7U] == 0U);
    ARPG_REQUIRE((*bytes)[106U] == 0x01U && (*bytes)[107U] == 0x00U);

    const auto decoded = persistence::decode_checkpoint(
        kBaselineBytes.data(), kBaselineBytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    auto legacy_expected = make_fixture();
    legacy_expected.current_room.is_abyss = true;
    ARPG_REQUIRE(same_state(decoded.state, legacy_expected));
    ARPG_REQUIRE(decoded.state.passive_tree.allocated_bits == 1ULL);

    std::vector<std::uint8_t> format_three(bytes->begin(), bytes->begin() + 120U);
    const std::array<std::uint8_t, 8U> v3_magic{{'I','A','R','P','G','S','0','4'}};
    std::copy(v3_magic.begin(), v3_magic.end(), format_three.begin());
    write_u32(format_three, 8U, 3U);
    write_u32(format_three, 24U, 88U);
    refresh_crc(format_three);
    const auto migrated_three = persistence::decode_checkpoint(
        format_three.data(), format_three.size());
    ARPG_REQUIRE(migrated_three.error == persistence::CodecError::none);
    ARPG_REQUIRE(migrated_three.state.item_ownership.items.empty());
    ARPG_REQUIRE(migrated_three.state.item_ownership.equipment.equipped_ids
        == items::EquipmentState{}.equipped_ids);
    const std::array<std::uint64_t, 3U> no_format_three_claims{};
    ARPG_REQUIRE(migrated_three.state.item_ownership.claimed_drop_bits
        == no_format_three_claims);
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
    static_assert(persistence::kV5BasePayloadSize == 204U);
    static_assert(persistence::kV5BaseEncodedCheckpointSize == 236U);
    static_assert(persistence::kV6BasePayloadSize == 428U);
    static_assert(persistence::kV6BaseEncodedCheckpointSize == 460U);

    std::vector<std::uint8_t> bytes;
    ARPG_REQUIRE(encoded_fixture(bytes));
    ARPG_REQUIRE(bytes.size() == 460U);
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
    bytes[8] = 7U;
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

arpg::test::Failure v5_length_count_crc_and_capacity_are_bounded() noexcept {
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
    bad_crc[236U] ^= 1U;
    ARPG_REQUIRE(persistence::decode_checkpoint(
        bad_crc.data(), bad_crc.size()).error == persistence::CodecError::bad_crc);

    auto excessive_count = *encoded;
    write_u32(excessive_count, 152U, 65536U);
    refresh_crc(excessive_count);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        excessive_count.data(), excessive_count.size()).error
        == persistence::CodecError::bad_payload_length);

    auto mismatched_count = *encoded;
    write_u32(mismatched_count, 152U, 2U);
    refresh_crc(mismatched_count);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        mismatched_count.data(), mismatched_count.size()).error
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
    ARPG_REQUIRE(maximum_encoded->size() == 2621860U);
    const auto maximum_decoded = persistence::decode_checkpoint(
        maximum_encoded->data(), maximum_encoded->size());
    ARPG_REQUIRE(maximum_decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(maximum_decoded.state.item_ownership.items.size() == 65535U);

    auto too_many = maximum;
    too_many.item_ownership.items.push_back(normal_item(65536U, 1U));
    ARPG_REQUIRE(!persistence::encode_checkpoint(too_many).has_value());
    return {};
}

arpg::test::Failure v5_corrupt_item_semantics_are_rejected() noexcept {
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

    ARPG_REQUIRE(rejects(473U, 1U));
    ARPG_REQUIRE(rejects(476U, 1U));
    ARPG_REQUIRE(rejects(460U, 0U));
    ARPG_REQUIRE(rejects(500U, 101U));
    ARPG_REQUIRE(rejects(468U, 0U));
    ARPG_REQUIRE(rejects(470U, 0U));
    ARPG_REQUIRE(rejects(471U, 96U));
    ARPG_REQUIRE(rejects(560U, 7U));
    ARPG_REQUIRE(rejects(517U, 7U));
    ARPG_REQUIRE(rejects(567U, 4U));

    auto dangling = *encoded;
    dangling[188U] = 0xFEU;
    refresh_crc(dangling);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        dangling.data(), dangling.size()).error
        == persistence::CodecError::invalid_state);

    auto wrong_slot = *encoded;
    std::copy_n(wrong_slot.begin() + 188U, 8U, wrong_slot.begin() + 196U);
    refresh_crc(wrong_slot);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        wrong_slot.data(), wrong_slot.size()).error
        == persistence::CodecError::invalid_state);

    auto duplicate_equipment = *encoded;
    std::copy_n(duplicate_equipment.begin() + 188U, 8U,
        duplicate_equipment.begin() + 228U);
    refresh_crc(duplicate_equipment);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        duplicate_equipment.data(), duplicate_equipment.size()).error
        == persistence::CodecError::invalid_state);

    auto zero_sequence = *encoded;
    std::fill(zero_sequence.begin() + 156U, zero_sequence.begin() + 164U,
        static_cast<std::uint8_t>(0U));
    refresh_crc(zero_sequence);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        zero_sequence.data(), zero_sequence.size()).error
        == persistence::CodecError::invalid_state);
    return {};
}

arpg::test::Failure codec_allocation_failures_do_not_escape_noexcept() noexcept {
    struct ResetAllocationFailure final {
        ~ResetAllocationFailure() noexcept {
            gAllocationsBeforeFailure = -1;
        }
    } reset_allocation_failure;
    const auto encoded = persistence::encode_checkpoint(make_owned_fixture());
    ARPG_REQUIRE(encoded.has_value());

    auto excessive_count = *encoded;
    write_u32(excessive_count, 152U, 65536U);
    refresh_crc(excessive_count);
    gAllocationsBeforeFailure = kVectorProxyAllocations == 0 ? 0 : -1;
    const auto rejected_before_allocation = persistence::decode_checkpoint(
        excessive_count.data(), excessive_count.size());
    ARPG_REQUIRE(rejected_before_allocation.error
        == persistence::CodecError::bad_payload_length);
    if constexpr (kVectorProxyAllocations == 0)
        ARPG_REQUIRE(gAllocationsBeforeFailure == 0);
    gAllocationsBeforeFailure = -1;

    gAllocationsBeforeFailure = 1 + kVectorProxyAllocations;
    const auto scratch_failure = persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    ARPG_REQUIRE(scratch_failure.error
        == persistence::CodecError::allocation_failure);
    ARPG_REQUIRE(gAllocationsBeforeFailure == -1);

    gAllocationsBeforeFailure = kVectorProxyAllocations;
    const auto decoded = persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::allocation_failure);
    ARPG_REQUIRE(gAllocationsBeforeFailure == -1);

    const auto encode_fixture = make_fixture();
    gAllocationsBeforeFailure = kVectorProxyAllocations;
    const auto failed_encode = persistence::encode_checkpoint(encode_fixture);
    ARPG_REQUIRE(!failed_encode.has_value());
    ARPG_REQUIRE(gAllocationsBeforeFailure == -1);
    return {};
}

arpg::test::Failure v5_unified_validator_rejects_semantic_state() noexcept {
    const auto encoded = persistence::encode_checkpoint(make_owned_fixture());
    ARPG_REQUIRE(encoded.has_value());
    auto zero_sequence = *encoded;
    std::fill(zero_sequence.begin() + 156U, zero_sequence.begin() + 164U,
        static_cast<std::uint8_t>(0U));
    refresh_crc(zero_sequence);
    ARPG_REQUIRE(persistence::decode_checkpoint(
        zero_sequence.data(), zero_sequence.size()).error
        == persistence::CodecError::invalid_state);
    return {};
}

arpg::test::Failure v6_death_retreat_transition_round_trips() noexcept {
    auto state = make_fixture();
    state.last_transition = checkpoint::TransitionKind::death_retreat;
    state.last_direction = checkpoint::ExitDirection::none;
    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    const auto decoded = persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.state.last_transition
        == checkpoint::TransitionKind::death_retreat);
    ARPG_REQUIRE(!decoded.migrated);

    auto v5 = *encoded;
    v5.erase(v5.begin() + 236U, v5.begin() + 460U);
    const std::array<std::uint8_t, 8U> v5_magic{{
        'I','A','R','P','G','S','0','6'}};
    std::copy(v5_magic.begin(), v5_magic.end(), v5.begin());
    write_u32(v5, 8U, 5U);
    write_u32(v5, 24U, static_cast<std::uint32_t>(v5.size() - 32U));
    refresh_crc(v5);
    ARPG_REQUIRE(persistence::decode_checkpoint(v5.data(), v5.size()).error
        == persistence::CodecError::invalid_enum);
    return {};
}

arpg::test::Failure v6_pending_death_floor_zero_round_trips() noexcept {
    auto state = make_fixture();
    state.current_room.floor_room_index = 0U;
    state.last_transition = checkpoint::TransitionKind::death_retreat;
    state.last_direction = checkpoint::ExitDirection::none;
    state.death_sequence = 1U;
    state.death.lifecycle = checkpoint::DeathLifecycle::pending_continue;
    state.death.data_version = checkpoint::kDeathCheckpointDataVersion;
    state.death.death_depth = state.current_room.depth;
    state.death.death_floor_room_index = 0U;
    state.death.death_ecology = state.current_room.ecology;
    state.death.source_kind = checkpoint::DeathSourceKind::unknown;
    state.death.source_monster_id = 0xFFU;
    state.death.damage_type = checkpoint::DeathDamageType::physical;
    state.death.raw_damage = 10U;
    state.death.health_loss = 10U;
    state.death.final_damage = 10U;
    state.death.recent_damage[0] = 10U;
    state.death.max_hp = 100;
    state.death.damage_reduction_cap = {{7500, 7500, 7500, 7500}};
    state.death.target_room = {
        state.current_room.index + 1U, 0x12345678U, 1U, 0U,
        checkpoint::EntrySide::initial,
        checkpoint::DungeonElement::water, true, false};

    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    const auto decoded = persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.state.current_room.floor_room_index == 0U);
    ARPG_REQUIRE(decoded.state.death.lifecycle
        == checkpoint::DeathLifecycle::pending_continue);
    ARPG_REQUIRE(decoded.state.death.death_floor_room_index == 0U);
    ARPG_REQUIRE(decoded.state.last_transition
        == checkpoint::TransitionKind::death_retreat);

    auto wrong_transition = state;
    wrong_transition.last_transition = checkpoint::TransitionKind::door;
    ARPG_REQUIRE(!persistence::encode_checkpoint(wrong_transition).has_value());
    auto wrong_direction = state;
    wrong_direction.last_direction = checkpoint::ExitDirection::left;
    ARPG_REQUIRE(!persistence::encode_checkpoint(wrong_direction).has_value());
    return {};
}

arpg::test::Failure v6_continued_target_floor_zero_round_trips() noexcept {
    auto state = make_fixture();
    state.current_room.floor_room_index = 0U;
    state.last_transition = checkpoint::TransitionKind::death_retreat;
    state.last_direction = checkpoint::ExitDirection::none;
    state.death_sequence = 1U;
    ARPG_REQUIRE(state.death.lifecycle == checkpoint::DeathLifecycle::none);

    const auto encoded = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(encoded.has_value());
    const auto decoded = persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.state.current_room.floor_room_index == 0U);
    ARPG_REQUIRE(decoded.state.death.lifecycle
        == checkpoint::DeathLifecycle::none);
    ARPG_REQUIRE(decoded.state.last_transition
        == checkpoint::TransitionKind::death_retreat);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"v5 full abyss state and resolution round trip", &v5_full_abyss_state_and_resolution_round_trip},
    {"v5 cleared reward total matches each danger", &v5_cleared_reward_total_matches_each_danger},
    {"v5 resolution total matches each rule danger", &v5_resolution_total_matches_each_rule_danger},
    {"v5 started state survives decode", &v5_started_state_survives_decode},
    {"v5 abyss enums are rejected individually", &v5_abyss_enums_are_rejected_individually},
    {"v5 resolution boolean is rejected", &v5_resolution_boolean_is_rejected},
    {"v5 reward total and mask bits are rejected", &v5_reward_total_and_mask_bits_are_rejected},
    {"v5 claimed and abandoned masks are consistent", &v5_claimed_and_abandoned_masks_are_consistent},
    {"v5 rule and danger match deterministic selection", &v5_rule_and_danger_must_match_deterministic_selection},
    {"v5 lifecycle requires matching room state", &v5_lifecycle_requires_matching_room_state},
    {"v5 abyss origin requires matching door", &v5_abyss_origin_requires_a_matching_door},
    {"v5 resolution counts are validated", &v5_resolution_counts_are_validated},
    {"v5 crc covers abyss fields", &v5_crc_covers_abyss_fields},
    {"legacy door abyss migrates deterministically", &legacy_door_abyss_migrates_deterministically},
    {"legacy door abyss requires new domain roll", &legacy_door_abyss_requires_new_domain_roll},
    {"legacy door migration rejects missing or mismatched direction", &legacy_door_migration_rejects_missing_or_mismatched_direction},
    {"legacy initial and descent abyss clear without drift", &legacy_initial_and_descent_abyss_are_cleared_without_drift},
    {"v1 through v4 decode migrated and v5 does not", &v1_through_v4_decode_as_migrated_but_v5_does_not},
    {"v5 golden layout and items round trip", &v5_golden_layout_and_items_round_trip},
    {"v5 complete ownership and six roll record are golden", &v5_complete_ownership_and_six_roll_record_are_golden},
    {"v5 length count crc and capacity are bounded", &v5_length_count_crc_and_capacity_are_bounded},
    {"v5 corrupt item semantics are rejected", &v5_corrupt_item_semantics_are_rejected},
    {"v5 unified validator rejects semantic state", &v5_unified_validator_rejects_semantic_state},
    {"v6 death retreat transition round trips", &v6_death_retreat_transition_round_trips},
    {"v6 pending death floor zero round trips", &v6_pending_death_floor_zero_round_trips},
    {"v6 continued target floor zero round trips", &v6_continued_target_floor_zero_round_trips},
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
