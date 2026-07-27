#include "test_framework.hpp"

#include "combat/combat_world.hpp"
#include "dungeon_test_support.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/room_progress_checkpoint.hpp"
#include "persistence/room_progress_codec.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

namespace {

using namespace arpg;

static_assert(!std::is_copy_constructible_v<
    dungeon::checkpoint::SaveCheckpointSlot>);
static_assert(!std::is_move_constructible_v<
    dungeon::checkpoint::SaveCheckpointSlot>);
static_assert(persistence::kMaximumEncodedCheckpointBytes == 8U * 1024U * 1024U);

void set_first_defeated_after_live(
    dungeon::checkpoint::RoomProgressCheckpoint& room,
    std::uint32_t count) noexcept {
    for (std::uint32_t ordinal = 3U; ordinal < 3U + count; ++ordinal) {
        room.defeat_bits[ordinal / 64U] |=
            std::uint64_t{1U} << (ordinal % 64U);
    }
}

items::ItemInstance normal_item(std::uint64_t id) noexcept {
    items::ItemInstance item{};
    item.id = id;
    item.base_id = 1U;
    item.rarity = items::ItemRarity::normal;
    item.item_level = 1U;
    item.required_level = 1U;
    return item;
}

bool same_vec(combat::Vec3 left, combat::Vec3 right) noexcept {
    std::uint32_t left_bits[3U]{};
    std::uint32_t right_bits[3U]{};
    static_assert(sizeof(left_bits) == sizeof(left));
    std::memcpy(left_bits, &left, sizeof(left));
    std::memcpy(right_bits, &right, sizeof(right));
    return std::equal(std::begin(left_bits), std::end(left_bits),
        std::begin(right_bits));
}

bool same_combat_event(
    const combat::CombatEvent& left,
    const combat::CombatEvent& right) noexcept {
    return left.kind == right.kind && left.tick == right.tick
        && left.attack == right.attack && left.skill == right.skill
        && left.strike_index == right.strike_index
        && left.finisher == right.finisher
        && left.target_ordinal == right.target_ordinal
        && left.hit_count == right.hit_count
        && left.feedback == right.feedback
        && same_vec(left.position, right.position)
        && left.value == right.value
        && left.monster_id == right.monster_id
        && left.spawn_ordinal == right.spawn_ordinal
        && left.affix_score == right.affix_score
        && left.reward_eligible == right.reward_eligible;
}

bool same_dungeon_event(
    const dungeon::DungeonEvent& left,
    const dungeon::DungeonEvent& right) noexcept {
    return left.kind == right.kind
        && left.session_tick == right.session_tick
        && left.room_index == right.room_index
        && left.room_seed == right.room_seed
        && left.destination_room_index == right.destination_room_index
        && left.destination_room_seed == right.destination_room_seed
        && left.transition == right.transition
        && left.direction == right.direction
        && left.abyss_pending_rewards == right.abyss_pending_rewards
        && left.abyss_unpicked_rewards == right.abyss_unpicked_rewards;
}

void drain_events(dungeon::DungeonSession& session) noexcept {
    while (session.try_pop_event().has_value()) {}
    while (session.try_pop_combat_event().has_value()) {}
}

bool matching_event_sequence(
    dungeon::DungeonSession& uninterrupted,
    dungeon::DungeonSession& reloaded) noexcept {
    for (;;) {
        const auto left = uninterrupted.try_pop_event();
        const auto right = reloaded.try_pop_event();
        if (left.has_value() != right.has_value()) return false;
        if (!left.has_value()) break;
        if (!same_dungeon_event(*left, *right)) return false;
    }
    for (;;) {
        const auto left = uninterrupted.try_pop_combat_event();
        const auto right = reloaded.try_pop_combat_event();
        if (left.has_value() != right.has_value()) return false;
        if (!left.has_value()) break;
        if (!same_combat_event(*left, *right)) return false;
    }
    return true;
}

std::array<std::uint64_t, modifiers::kDamageTypeCount>
damage_history_totals(
    const combat::PlayerDamageHistoryCheckpoint& history) noexcept {
    std::array<std::uint64_t, modifiers::kDamageTypeCount> totals{};
    for (const auto& bucket : history.buckets) {
        for (std::size_t type = 0U; type < bucket.size(); ++type) {
            totals[type] += bucket[type];
        }
    }
    return totals;
}

bool same_obstacle_state(
    const combat::RoomCombatCheckpoint& left,
    const combat::RoomCombatCheckpoint& right) noexcept {
    if (left.obstacle_count != right.obstacle_count
            || left.fire_crate_count != right.fire_crate_count) {
        return false;
    }
    for (std::uint16_t index = 0U; index < left.obstacle_count; ++index) {
        const auto& a = left.obstacles[index];
        const auto& b = right.obstacles[index];
        if (a.ordinal != b.ordinal || a.hp != b.hp
                || a.max_hp != b.max_hp
                || a.broken_tick != b.broken_tick
                || a.intact != b.intact) return false;
    }
    for (std::uint8_t index = 0U; index < left.fire_crate_count; ++index) {
        const auto& a = left.fire_crates[index];
        const auto& b = right.fire_crates[index];
        if (!same_vec(a.position, b.position)
                || a.broken_tick != b.broken_tick
                || a.intact != b.intact) return false;
    }
    return true;
}

std::uint32_t v9_authority_hash(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint32_t>(bytes[28U])
        | (static_cast<std::uint32_t>(bytes[29U]) << 8U)
        | (static_cast<std::uint32_t>(bytes[30U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[31U]) << 24U);
}

bool ten_thousand_tick_reload_trace_matches() noexcept {
    constexpr std::uint64_t kRootSeed = 0x51A7E10ADULL;
    constexpr std::uint64_t kFollowingTicks = 10000U;
    dungeon::DungeonRules rules{};
    const auto initial = dungeon::make_initial_run_state(kRootSeed, rules);
    if (initial.fault != dungeon::DungeonFault::none) return false;
    auto uninterrupted = std::make_unique<dungeon::DungeonSession>(
        rules, initial.state);
    if (uninterrupted == nullptr
            || uninterrupted->phase() != dungeon::RoomPhase::locked) {
        return false;
    }
    test::set_player_health(*uninterrupted, 1000000, 1000000);
    // Enter combat without advancing the combat authority. This is the
    // quiescent checkpoint boundary; transient clearing is tested separately
    // by room_combat_checkpoint.room checkpoint transients.
    uninterrupted->tick({});

    auto saved = std::make_unique<dungeon::checkpoint::SaveCheckpointSlot>();
    auto decoded = std::make_unique<dungeon::checkpoint::SaveCheckpointSlot>();
    auto left = std::make_unique<dungeon::checkpoint::SaveCheckpointSlot>();
    auto right = std::make_unique<dungeon::checkpoint::SaveCheckpointSlot>();
    auto save_bytes = std::make_unique<std::uint8_t[]>(
        persistence::kMaximumEncodedCheckpointBytes);
    auto left_bytes = std::make_unique<std::uint8_t[]>(
        persistence::kMaximumEncodedCheckpointBytes);
    auto right_bytes = std::make_unique<std::uint8_t[]>(
        persistence::kMaximumEncodedCheckpointBytes);
    if (!saved || !decoded || !left || !right || !save_bytes
            || !left_bytes || !right_bytes
            || !uninterrupted->capture_save_checkpoint(*saved, 1U)) {
        return false;
    }
    std::size_t saved_size{};
    if (persistence::encode_checkpoint_v9_into(*saved, save_bytes.get(),
            persistence::kMaximumEncodedCheckpointBytes, saved_size)
            != persistence::CodecError::none) return false;
    bool migrated = true;
    if (persistence::decode_checkpoint_v9_into(save_bytes.get(), saved_size,
            *decoded, migrated) != persistence::CodecError::none
            || migrated) return false;
    auto reloaded = std::make_unique<dungeon::DungeonSession>(
        rules, decoded->state);
    if (reloaded) test::set_player_health(*reloaded, 1000000, 1000000);
    if (!reloaded || !reloaded->restore_room_progress_checkpoint(*decoded)) {
        return false;
    }
    drain_events(*uninterrupted);
    drain_events(*reloaded);

    for (std::uint64_t following = 0U;
            following < kFollowingTicks; ++following) {
        const combat::MovementInput input{
            static_cast<std::int8_t>(((following / 31U) & 1U) == 0U ? 1 : -1),
            static_cast<std::int8_t>(((following / 47U) & 1U) == 0U ? -1 : 1),
        };
        uninterrupted->tick(input);
        reloaded->tick(input);
        const std::uint64_t revision = following + 2U;
        if (!uninterrupted->capture_save_checkpoint(*left, revision)
                || !reloaded->capture_save_checkpoint(*right, revision)
                || !dungeon::checkpoint::same_room_progress_checkpoint(
                    left->room_progress, right->room_progress)) {
            return false;
        }
        std::size_t left_size{};
        std::size_t right_size{};
        if (persistence::encode_checkpoint_v9_into(*left, left_bytes.get(),
                persistence::kMaximumEncodedCheckpointBytes, left_size)
                    != persistence::CodecError::none
                || persistence::encode_checkpoint_v9_into(*right,
                    right_bytes.get(), persistence::kMaximumEncodedCheckpointBytes,
                    right_size) != persistence::CodecError::none
                || left_size != right_size
                || v9_authority_hash(left_bytes.get())
                    != v9_authority_hash(right_bytes.get())
                || !std::equal(left_bytes.get(), left_bytes.get() + left_size,
                    right_bytes.get())
                || left->room_progress.combat.evasion_rng_state
                    != right->room_progress.combat.evasion_rng_state
                || damage_history_totals(
                    left->room_progress.combat.player_damage_history)
                    != damage_history_totals(
                        right->room_progress.combat.player_damage_history)
                || !same_obstacle_state(left->room_progress.combat,
                    right->room_progress.combat)
                || !matching_event_sequence(*uninterrupted, *reloaded)) {
            return false;
        }
    }
    return true;
}

bool make_fixture(dungeon::checkpoint::SaveCheckpointSlot& slot) noexcept {
    slot.persistence_revision = 19U;
    slot.state.root_seed = 0x1234U;
    slot.state.commit_generation = 7U;
    slot.state.current_room.index = 41U;
    slot.state.current_room.seed = 43U;
    slot.state.current_room.depth = 2U;
    slot.state.current_room.floor_room_index = 3U;
    auto& room = slot.room_progress;
    room.lifecycle = dungeon::checkpoint::RoomProgressLifecycle::active;
    room.room_index = 41U;
    room.room_seed = 43U;
    room.monster_generator_version = 1U;
    room.monster_blueprint_hash = 0xA11CE1125ULL;
    room.environment_generator_version = 1U;
    room.environment_blueprint_hash = 0xE1170001ULL;
    room.generated_monsters = 1125U;
    room.defeated_monsters = 282U;
    room.required_kills = 282U;
    room.exits_unlocked = true;
    set_first_defeated_after_live(room, 282U);
    std::unique_ptr<combat::CombatWorld> world{
        new (std::nothrow) combat::CombatWorld{}};
    return world != nullptr && world->capture_room_checkpoint(room.combat);
}

bool make_cleared_abyss_fixture(
    dungeon::checkpoint::SaveCheckpointSlot& slot) noexcept {
    if (!make_fixture(slot)) return false;
    auto& state = slot.state;
    auto& room = slot.room_progress;
    state.current_room.is_abyss = true;
    state.abyss.lifecycle = abyss::AbyssLifecycle::cleared;
    state.abyss.rule = abyss::AbyssRuleId::swift_pursuit;
    state.abyss.reward_total = 2U;
    room.generated_monsters = 3U;
    room.defeated_monsters = 3U;
    room.required_kills = 1U;
    room.exits_unlocked = true;
    room.full_clear = true;
    room.reward_committed = true;
    room.defeat_bits = {};
    room.defeat_bits[0U] = 0x07U;
    room.combat.monster_count = 0U;
    room.equipment_ground[0U].ordinal = 0U;
    room.equipment_ground[0U].source = 1U;
    room.equipment_ground[0U].reward_ordinal = 0U;
    room.equipment_ground[0U].item = normal_item(99U);
    return true;
}

test::Failure v9_round_trip_preserves_large_room_fields() noexcept {
    std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> source{
        new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(source != nullptr && decoded != nullptr && bytes != nullptr);
    ARPG_REQUIRE(make_fixture(*source));

    std::size_t written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*source,
        bytes.get(), persistence::kMaximumEncodedCheckpointBytes, written)
        == persistence::CodecError::none);
    ARPG_REQUIRE(written <= persistence::kMaximumEncodedCheckpointBytes);
    ARPG_REQUIRE(std::equal(bytes.get(), bytes.get() + 8U,
        std::array<std::uint8_t, 8U>{
            {'A','R','P','G','S','V','9','\0'}}.begin()));
    bool migrated = true;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), written, *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(!migrated);
    ARPG_REQUIRE(dungeon::checkpoint::same_room_progress_checkpoint(
        source->room_progress, decoded->room_progress));
    ARPG_REQUIRE(decoded->persistence_revision == 19U);
    ARPG_REQUIRE(decoded->state.commit_generation == 7U);
    ARPG_REQUIRE(decoded->room_progress.generated_monsters == 1125U);
    ARPG_REQUIRE(decoded->room_progress.defeated_monsters == 282U);
    ARPG_REQUIRE(decoded->room_progress.required_kills == 282U);
    ARPG_REQUIRE(decoded->room_progress.exits_unlocked);
    ARPG_REQUIRE(decoded->room_progress.monster_blueprint_hash != 0U);
    ARPG_REQUIRE(decoded->room_progress.environment_blueprint_hash != 0U);
    decoded->room_progress.combat.player.velocity.x = -0.0F;
    ARPG_REQUIRE(!dungeon::checkpoint::same_room_progress_checkpoint(
        source->room_progress, decoded->room_progress));
    decoded->room_progress.combat.player.velocity.x = 0.0F;
    decoded->room_progress.combat.attack.elapsed_ticks += 1U;
    ARPG_REQUIRE(!dungeon::checkpoint::same_room_progress_checkpoint(
        source->room_progress, decoded->room_progress));
    decoded->room_progress.combat.attack.elapsed_ticks -= 1U;
    decoded->room_progress.combat.player_damage_history.buckets[299U][0U]
        += 1U;
    ARPG_REQUIRE(!dungeon::checkpoint::same_room_progress_checkpoint(
        source->room_progress, decoded->room_progress));
    decoded->room_progress.combat.player_damage_history.buckets[299U][0U]
        -= 1U;
    ARPG_REQUIRE(dungeon::checkpoint::same_room_progress_checkpoint(
        source->room_progress, decoded->room_progress));
    ARPG_REQUIRE(persistence::verify_checkpoint_v9_readback(
        bytes.get(), written, *source, bytes.get(), written)
        == persistence::CodecError::none);
    source->room_progress.combat.player.position.x += 0.25F;
    ARPG_REQUIRE(dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        source->room_progress, source->state));
    ARPG_REQUIRE(persistence::verify_checkpoint_v9_readback(
        bytes.get(), written, *source, bytes.get(), written)
        == persistence::CodecError::invalid_state);
    source->room_progress.combat.player.position.x -= 0.25F;
    ARPG_REQUIRE(ten_thousand_tick_reload_trace_matches());
    return {};
}

test::Failure v9_rejects_crc_and_length_corruption() noexcept {
    std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> source{
        new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(source != nullptr && decoded != nullptr && bytes != nullptr);
    ARPG_REQUIRE(make_fixture(*source));
    std::size_t written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*source,
        bytes.get(), persistence::kMaximumEncodedCheckpointBytes, written)
        == persistence::CodecError::none);

    bytes[written - 1U] ^= 0x80U;
    bool migrated{};
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), written, *decoded, migrated)
        == persistence::CodecError::bad_crc);
    bytes[written - 1U] ^= 0x80U;
    bytes[24U] = 1U;
    bytes[25U] = 0U;
    bytes[26U] = 0x80U;
    bytes[27U] = 0U;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), written, *decoded, migrated)
        == persistence::CodecError::bad_payload_length);
    return {};
}

test::Failure structural_validation_rejects_identity_and_order_faults() noexcept {
    std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> slot{
        new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(slot != nullptr);
    ARPG_REQUIRE(make_fixture(*slot));
    ARPG_REQUIRE(dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        slot->room_progress, slot->state));

    std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> none_slot{
        new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(none_slot != nullptr);
    ARPG_REQUIRE(dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        none_slot->room_progress, none_slot->state));
    none_slot->room_progress.combat.player.state =
        combat::PlayerState::landing;
    ARPG_REQUIRE(!dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        none_slot->room_progress, none_slot->state));
    none_slot->room_progress.combat.player.state =
        combat::PlayerState::idle;
    none_slot->room_progress.combat.attack.elapsed_ticks = 1U;
    ARPG_REQUIRE(!dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        none_slot->room_progress, none_slot->state));
    none_slot->room_progress.combat.attack.elapsed_ticks = 0U;
    none_slot->room_progress.combat.abyss_environment.warning = true;
    ARPG_REQUIRE(!dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        none_slot->room_progress, none_slot->state));
    none_slot->room_progress.combat.abyss_environment.warning = false;
    none_slot->room_progress.combat.player_damage_history.buckets[299U][
        modifiers::damage_index(modifiers::DamageType::chaos)] = 1U;
    ARPG_REQUIRE(!dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        none_slot->room_progress, none_slot->state));

    slot->room_progress.room_seed ^= 1U;
    ARPG_REQUIRE(!dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        slot->room_progress, slot->state));
    slot->room_progress.room_seed ^= 1U;
    slot->state.death.lifecycle =
        dungeon::checkpoint::DeathLifecycle::pending_continue;
    ARPG_REQUIRE(!dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        slot->room_progress, slot->state));
    slot->state.death = {};
    slot->state.current_room.is_abyss = true;
    slot->state.abyss.lifecycle = abyss::AbyssLifecycle::started;
    slot->state.abyss.rule = abyss::AbyssRuleId::swift_pursuit;
    slot->room_progress.combat.abyss_environment.rule =
        abyss::AbyssRuleId::swift_pursuit;
    ARPG_REQUIRE(dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        slot->room_progress, slot->state));
    slot->room_progress.combat.abyss_environment.rule =
        abyss::AbyssRuleId::heavy_steps;
    ARPG_REQUIRE(!dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        slot->room_progress, slot->state));
    slot->state.current_room.is_abyss = false;
    slot->state.abyss = {};
    slot->room_progress.combat.abyss_environment.rule =
        abyss::AbyssRuleId::none;
    slot->room_progress.equipment_ground_count = 2U;
    slot->room_progress.equipment_ground[0U].ordinal = 7U;
    slot->room_progress.equipment_ground[1U].ordinal = 7U;
    ARPG_REQUIRE(!dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        slot->room_progress, slot->state));
    std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> abyss_slot{
        new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(abyss_slot != nullptr);
    ARPG_REQUIRE(make_cleared_abyss_fixture(*abyss_slot));
    ARPG_REQUIRE(dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        abyss_slot->room_progress, abyss_slot->state));

    abyss_slot->state.abyss.generated_mask = 0x01U;
    abyss_slot->room_progress.equipment_ground_count = 1U;
    ARPG_REQUIRE(dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        abyss_slot->room_progress, abyss_slot->state));
    abyss_slot->state.abyss.claimed_mask = 0x01U;
    ARPG_REQUIRE(!dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        abyss_slot->room_progress, abyss_slot->state));
    abyss_slot->room_progress.equipment_ground_count = 0U;
    ARPG_REQUIRE(dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        abyss_slot->room_progress, abyss_slot->state));
    abyss_slot->state.abyss.abandoned_mask = 0x01U;
    ARPG_REQUIRE(!dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        abyss_slot->room_progress, abyss_slot->state));
    abyss_slot->state.abyss.abandoned_mask = 0U;
    abyss_slot->state.abyss.claimed_mask = 0U;
    abyss_slot->room_progress.equipment_ground_count = 1U;
    abyss_slot->room_progress.equipment_ground[0U].source = 0U;
    ARPG_REQUIRE(!dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        abyss_slot->room_progress, abyss_slot->state));
    return {};
}

constexpr test::TestCase kCases[] = {
    {"v9 large room round trip", &v9_round_trip_preserves_large_room_fields},
    {"v9 corruption", &v9_rejects_crc_and_length_corruption},
    {"v9 structural validation", &structural_validation_rejects_identity_and_order_faults},
};

}  // namespace

arpg::test::TestSuite checkpoint_v9_suite() noexcept {
    return arpg::test::make_suite("checkpoint_v9", kCases);
}
