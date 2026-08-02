#include "test_framework.hpp"

#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "checkpoint/room_checkpoint_validation.hpp"
#include "combat/combat_world.hpp"
#include "dungeon_test_support.hpp"
#include "dungeon/death_checkpoint.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_generation.hpp"
#include "dungeon/room_affix.hpp"
#include "dungeon/dungeon_session.hpp"
#include "persistence/room_progress_codec.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>

namespace {

using namespace arpg;

static_assert(!std::is_copy_constructible_v<
    checkpoint::SaveCheckpointSlot>);
static_assert(!std::is_move_constructible_v<
    checkpoint::SaveCheckpointSlot>);
static_assert(persistence::kMaximumEncodedCheckpointBytes == 8U * 1024U * 1024U);

void set_first_defeated_after_live(
    checkpoint::RoomProgressCheckpoint& room,
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

template <typename Vec>
bool same_vec(const Vec& left, const Vec& right) noexcept {
    std::uint32_t left_bits[3U]{};
    std::uint32_t right_bits[3U]{};
    static_assert(sizeof(left_bits) == sizeof(Vec));
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
    const checkpoint::PlayerDamageHistoryCheckpoint& history) noexcept {
    std::array<std::uint64_t, modifiers::kDamageTypeCount> totals{};
    for (const auto& bucket : history.buckets) {
        for (std::size_t type = 0U; type < bucket.size(); ++type) {
            totals[type] += bucket[type];
        }
    }
    return totals;
}

bool same_obstacle_state(
    const checkpoint::RoomCombatCheckpoint& left,
    const checkpoint::RoomCombatCheckpoint& right) noexcept {
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

std::uint32_t read_u32(const std::uint8_t* bytes,
    const std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U)
        | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

void write_u16(std::uint8_t* bytes, const std::size_t offset,
    const std::uint16_t value) noexcept {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

void write_u32(std::uint8_t* bytes, const std::size_t offset,
    const std::uint32_t value) noexcept {
    for (std::size_t index = 0U; index < sizeof(value); ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(
            value >> (index * 8U));
    }
}

void refresh_v9_envelope(std::uint8_t* bytes,
    const std::size_t size) noexcept {
    const auto payload_size = static_cast<std::uint32_t>(
        size - persistence::kCheckpointHeaderSize);
    write_u32(bytes, 24U, payload_size);
    auto checksum = persistence::crc32_update(
        0U, bytes + 8U, 20U);
    checksum = persistence::crc32_update(checksum,
        bytes + persistence::kCheckpointHeaderSize, payload_size);
    write_u32(bytes, 28U, checksum);
}

bool rewrite_coupon_record_as_task5_wire(std::uint8_t* const bytes,
    const std::size_t size,
    const checkpoint::SecondaryGroundCheckpoint& record,
    const std::uint16_t task5_ordinal) noexcept {
    std::array<std::uint8_t, persistence::kV9SecondaryGroundBytes> needle{};
    needle[0U] = static_cast<std::uint8_t>(record.tag);
    write_u16(needle.data(), 1U, record.ordinal);
    needle[3U] = record.source;
    std::memcpy(needle.data() + 4U, &record.position,
        sizeof(record.position));
    needle.back() = static_cast<std::uint8_t>(record.material);
    const auto found = std::search(bytes + persistence::kCheckpointHeaderSize,
        bytes + size, needle.begin(), needle.end());
    if (found == bytes + size) return false;
    write_u16(found, 1U, task5_ordinal);
    found[3U] = 1U;
    return true;
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

    auto saved = std::make_unique<checkpoint::SaveCheckpointSlot>();
    auto decoded = std::make_unique<checkpoint::SaveCheckpointSlot>();
    auto left = std::make_unique<checkpoint::SaveCheckpointSlot>();
    auto right = std::make_unique<checkpoint::SaveCheckpointSlot>();
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
                || !checkpoint::same_room_progress_checkpoint(
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

bool make_fixture(checkpoint::SaveCheckpointSlot& slot) noexcept {
    static_assert(limits::kRoomMonsterCapacity == 1152U);
    slot.persistence_revision = 19U;
    slot.state.root_seed = 0x1234U;
    slot.state.commit_generation = 7U;
    slot.state.current_room.index = 41U;
    slot.state.current_room.seed = 43U;
    slot.state.current_room.depth = 2U;
    slot.state.current_room.floor_room_index = 3U;
    auto& room = slot.room_progress;
    room.lifecycle = checkpoint::RoomProgressLifecycle::active;
    room.room_index = 41U;
    room.room_seed = 43U;
    room.monster_generator_version = 1U;
    room.monster_blueprint_hash = 0xA11CE1152ULL;
    room.environment_generator_version = 1U;
    room.environment_blueprint_hash = 0xE1170001ULL;
    room.generated_monsters = 1152U;
    room.defeated_monsters = 288U;
    room.required_kills = 288U;
    room.exits_unlocked = true;
    set_first_defeated_after_live(room, 288U);
    std::unique_ptr<combat::CombatWorld> world{
        new (std::nothrow) combat::CombatWorld{}};
    if (world == nullptr || !world->capture_room_checkpoint(room.combat)) {
        return false;
    }
    room.equipment_ground_count = 1U;
    room.equipment_ground[0U].ordinal = 1124U;
    room.equipment_ground[0U].source = 0U;
    room.equipment_ground[0U].reward_ordinal = 0xFFU;
    room.equipment_ground[0U].item = normal_item(1125U);
    room.secondary_ground_count = 3U;
    room.secondary_ground[0U].tag =
        checkpoint::SecondaryGroundTag::health_potion;
    room.secondary_ground[0U].ordinal = 2247U;
    room.secondary_ground[0U].source = 0U;
    room.secondary_ground[0U].material = items::MaterialId::count;
    room.secondary_ground[1U].tag =
        checkpoint::SecondaryGroundTag::material;
    room.secondary_ground[1U].ordinal = 2248U;
    room.secondary_ground[1U].source = 0U;
    room.secondary_ground[1U].material =
        items::MaterialId::reinforcement_stone;
    room.secondary_ground[2U].tag =
        checkpoint::SecondaryGroundTag::material;
    room.secondary_ground[2U].ordinal = 2249U;
    room.secondary_ground[2U].source = 1U;
    room.secondary_ground[2U].material =
        items::MaterialId::reinforcement_stone;
    return true;
}

void clear_secondary_progress(checkpoint::SaveCheckpointSlot& slot) noexcept {
    slot.state.item_ownership.material_claimed_drop_bits = {};
    slot.room_progress.secondary_claim_bits = {};
    slot.room_progress.secondary_ground_count = 0U;
}

bool make_cleared_abyss_fixture(
    checkpoint::SaveCheckpointSlot& slot) noexcept {
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
    room.equipment_ground_count = 0U;
    room.secondary_ground_count = 0U;
    room.equipment_ground[0U].ordinal = 0U;
    room.equipment_ground[0U].source = 1U;
    room.equipment_ground[0U].reward_ordinal = 0U;
    room.equipment_ground[0U].item = normal_item(99U);
    return true;
}

checkpoint::CombatDeathSnapshot minimal_death_snapshot() noexcept {
    checkpoint::CombatDeathSnapshot result{};
    result.source.kind = checkpoint::PlayerDamageSourceKind::unknown;
    result.source.monster = checkpoint::MonsterId::count;
    result.raw_damage = 20U;
    result.health_loss = 20U;
    result.final_damage = 20U;
    result.recent_damage[0U] = 20U;
    result.defense.max_hp = 100;
    result.defense.damage_reduction_cap.fill(7500);
    return result;
}

combat::CombatDeathSnapshot to_runtime_death_snapshot(
    const checkpoint::CombatDeathSnapshot& source) noexcept {
    combat::CombatDeathSnapshot result{};
    result.tick = source.tick;
    result.source.kind = static_cast<combat::PlayerDamageSourceKind>(
        source.source.kind);
    result.source.monster = static_cast<combat::MonsterId>(
        source.source.monster);
    result.source.detail_id = source.source.detail_id;
    result.primary_type = static_cast<modifiers::DamageType>(
        source.primary_type);
    result.raw_damage = source.raw_damage;
    result.barrier_loss = source.barrier_loss;
    result.health_loss = source.health_loss;
    result.final_damage = source.final_damage;
    result.recent_damage = source.recent_damage;
    result.defense.hp = source.defense.hp;
    result.defense.max_hp = source.defense.max_hp;
    result.defense.barrier = source.defense.barrier;
    result.defense.max_barrier = source.defense.max_barrier;
    result.defense.armor = source.defense.armor;
    result.defense.evasion = source.defense.evasion;
    result.defense.armor_reduction_bp = source.defense.armor_reduction_bp;
    result.defense.evasion_rate_bp = source.defense.evasion_rate_bp;
    result.defense.damage_reduction = source.defense.damage_reduction;
    result.defense.damage_reduction_cap =
        source.defense.damage_reduction_cap;
    return result;
}

bool make_pending_death_fixture(
    checkpoint::SaveCheckpointSlot& slot,
    const bool death_was_abyss,
    const bool retain_failed_abyss = false) noexcept {
    checkpoint::clear_save_checkpoint_slot(slot);
    dungeon::DungeonRules rules{};
    auto state = dungeon::make_initial_run_state(0x51A6E11U, rules).state;
    state.current_room.index = 17U;
    state.current_room.seed = 0x150U;
    state.current_room.depth = 9U;
    state.current_room.floor_room_index = 7U;
    state.current_room.entry = dungeon::EntrySide::left;
    state.current_room.ecology = dungeon::DungeonElement::lightning;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = death_was_abyss;
    state.commit_generation = 11U;
    state.death_sequence = 3U;
    state.biases = {};

    const auto target = dungeon::make_death_retreat_target(state, 4U, rules);
    if (target.fault != dungeon::DungeonFault::none) return false;
    state.death = dungeon::make_death_checkpoint(
        to_runtime_death_snapshot(minimal_death_snapshot()),
        state.current_room, target.room);

    if (death_was_abyss || retain_failed_abyss) {
        const auto selection = abyss::select_abyss_rule(
            state.current_room.seed, state.current_room.depth);
        if (!selection.has_value()) return false;
        state.abyss.lifecycle = abyss::AbyssLifecycle::failed;
        state.abyss.danger = selection->danger;
        state.abyss.rule = selection->rule;
        state.abyss.rules_version = selection->rules_version;
        const auto total = abyss::reward_profile_for(
            selection->danger, 1U).item_count;
        state.last_abyss_resolution = {
            true, state.current_room.seed, selection->rule,
            total, 0U, 0U, total,
            death_was_abyss ? abyss::AbyssLifecycle::failed
                            : abyss::AbyssLifecycle::none};
    }

    ++state.commit_generation;
    ++state.death_sequence;
    state.current_room.is_abyss = false;
    state.last_transition = dungeon::TransitionKind::death_retreat;
    state.last_direction = dungeon::ExitDirection::none;
    slot.state = std::move(state);
    slot.persistence_revision = slot.state.commit_generation;
    return checkpoint::valid_room_progress_checkpoint_structural(
        slot.room_progress, slot.state);
}

bool encode_legacy_v9_without_lifecycle(
    const checkpoint::SaveCheckpointSlot& source,
    std::uint8_t* const bytes,
    const std::size_t capacity,
    std::size_t& written) noexcept {
    if (persistence::encode_checkpoint_v9_into(
            source, bytes, capacity, written)
            != persistence::CodecError::none
            || written <= persistence::kCheckpointHeaderSize + 1U
            || bytes[written - 2U] != static_cast<std::uint8_t>(
                source.state.last_abyss_resolution.lifecycle)
            || bytes[written - 1U]
                != persistence::kV9CanonicalSecondaryOrdinalMarker) {
        return false;
    }
    written -= 2U;
    refresh_v9_envelope(bytes, written);
    return true;
}

test::Failure v9_round_trip_preserves_large_room_fields() noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> source{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
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
    ARPG_REQUIRE(checkpoint::same_room_progress_checkpoint(
        source->room_progress, decoded->room_progress));
    ARPG_REQUIRE(decoded->persistence_revision == 19U);
    ARPG_REQUIRE(decoded->state.commit_generation == 7U);
    ARPG_REQUIRE(decoded->room_progress.generated_monsters == 1152U);
    ARPG_REQUIRE(decoded->room_progress.defeated_monsters == 288U);
    ARPG_REQUIRE(decoded->room_progress.required_kills == 288U);
    ARPG_REQUIRE(decoded->room_progress.exits_unlocked);
    ARPG_REQUIRE(decoded->room_progress.monster_blueprint_hash != 0U);
    ARPG_REQUIRE(decoded->room_progress.environment_blueprint_hash != 0U);
    ARPG_REQUIRE(decoded->room_progress.equipment_ground_count == 1U);
    ARPG_REQUIRE(decoded->room_progress.equipment_ground[0U].ordinal == 1124U);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground_count == 3U);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground[0U].ordinal == 2247U);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground[1U].ordinal == 2248U);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground[2U].ordinal == 2249U);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground[2U].source == 1U);
    decoded->room_progress.combat.player.velocity.x = -0.0F;
    ARPG_REQUIRE(!checkpoint::same_room_progress_checkpoint(
        source->room_progress, decoded->room_progress));
    decoded->room_progress.combat.player.velocity.x = 0.0F;
    decoded->room_progress.combat.attack.elapsed_ticks += 1U;
    ARPG_REQUIRE(!checkpoint::same_room_progress_checkpoint(
        source->room_progress, decoded->room_progress));
    decoded->room_progress.combat.attack.elapsed_ticks -= 1U;
    decoded->room_progress.combat.player_damage_history.buckets[299U][0U]
        += 1U;
    ARPG_REQUIRE(!checkpoint::same_room_progress_checkpoint(
        source->room_progress, decoded->room_progress));
    decoded->room_progress.combat.player_damage_history.buckets[299U][0U]
        -= 1U;
    ARPG_REQUIRE(checkpoint::same_room_progress_checkpoint(
        source->room_progress, decoded->room_progress));
    ARPG_REQUIRE(persistence::verify_checkpoint_v9_readback(
        bytes.get(), written, *source, bytes.get(), written)
        == persistence::CodecError::none);
    source->room_progress.combat.player.position.x += 0.25F;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        source->room_progress, source->state));
    ARPG_REQUIRE(persistence::verify_checkpoint_v9_readback(
        bytes.get(), written, *source, bytes.get(), written)
        == persistence::CodecError::invalid_state);
    source->room_progress.combat.player.position.x -= 0.25F;
    ARPG_REQUIRE(ten_thousand_tick_reload_trace_matches());
    return {};
}

test::Failure task5_v9_secondary_ordinals_migrate_to_canonical_once() noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> source{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(source != nullptr && decoded != nullptr && bytes != nullptr);
    ARPG_REQUIRE(make_fixture(*source));
    clear_secondary_progress(*source);

    auto& state = source->state;
    auto& room = source->room_progress;
    room.secondary_claim_bits = {};
    room.secondary_claim_bits[0U] = (std::uint64_t{1U} << 13U)
        | (std::uint64_t{1U} << 20U)
        | (std::uint64_t{1U} << 26U);
    state.item_ownership.material_claimed_drop_bits = {};
    state.item_ownership.material_claimed_drop_bits[0U] =
        (std::uint64_t{1U} << 10U) | (std::uint64_t{1U} << 13U);

    room.secondary_ground_count = 2U;
    room.secondary_ground[0U] = {};
    room.secondary_ground[0U].tag =
        checkpoint::SecondaryGroundTag::material;
    ARPG_REQUIRE(room.combat.monster_count >= 3U);
    ARPG_REQUIRE(room.combat.monsters[1U].ordinal == 1U);
    room.secondary_ground[0U].ordinal = 4U;
    room.secondary_ground[0U].source = 0U;
    room.secondary_ground[0U].position = room.combat.monsters[1U].position;
    room.secondary_ground[0U].material =
        items::MaterialId::reinforcement_stone;
    room.secondary_ground[1U] = {};
    room.secondary_ground[1U].tag =
        checkpoint::SecondaryGroundTag::material;
    room.secondary_ground[1U].ordinal = 14U;
    room.secondary_ground[1U].source = 0U;
    room.secondary_ground[1U].position = {-19.75F, 6.125F, 0.0F};
    room.secondary_ground[1U].material = items::MaterialId::coupon_6;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        room, state));

    std::size_t written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*source,
        bytes.get(), persistence::kMaximumEncodedCheckpointBytes, written)
        == persistence::CodecError::none);
    const auto coupon_record = room.secondary_ground[1U];
    ARPG_REQUIRE(rewrite_coupon_record_as_task5_wire(
        bytes.get(), written, coupon_record, 14U));
    if (bytes[written - 1U]
            == persistence::kV9CanonicalSecondaryOrdinalMarker) {
        --written;
    }
    refresh_v9_envelope(bytes.get(), written);

    bool migrated = false;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), written, *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(migrated);
    const auto& restored = decoded->room_progress;
    ARPG_REQUIRE(restored.secondary_ground_count == 2U);
    ARPG_REQUIRE(restored.secondary_ground[0U].ordinal == 2U);
    ARPG_REQUIRE(restored.secondary_ground[0U].source == 0U);
    ARPG_REQUIRE(restored.secondary_ground[1U].ordinal == 7U);
    ARPG_REQUIRE(restored.secondary_ground[1U].source == 1U);
    ARPG_REQUIRE((restored.secondary_claim_bits[0U]
        & (std::uint64_t{1U} << 10U)) != 0U);
    ARPG_REQUIRE((restored.secondary_claim_bits[0U]
        & (std::uint64_t{1U} << 13U)) != 0U);
    ARPG_REQUIRE((restored.secondary_claim_bits[0U]
        & (std::uint64_t{1U} << 20U)) == 0U);
    ARPG_REQUIRE((restored.secondary_claim_bits[0U]
        & (std::uint64_t{1U} << 26U)) == 0U);
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        restored, decoded->state));
    return {};
}

test::Failure unmarked_task5_common_ordinal_uses_spawn_position() noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> source{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(source != nullptr && decoded != nullptr && bytes != nullptr);
    ARPG_REQUIRE(make_fixture(*source));
    clear_secondary_progress(*source);

    auto& room = source->room_progress;
    ARPG_REQUIRE(room.combat.monster_count >= 3U);
    ARPG_REQUIRE(room.combat.monsters[1U].ordinal == 1U);
    room.secondary_ground_count = 1U;
    room.secondary_ground[0U] = {};
    room.secondary_ground[0U].tag =
        checkpoint::SecondaryGroundTag::material;
    room.secondary_ground[0U].ordinal = 4U;
    room.secondary_ground[0U].source = 0U;
    room.secondary_ground[0U].position = room.combat.monsters[1U].position;
    room.secondary_ground[0U].material =
        items::MaterialId::reinforcement_stone;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        room, source->state));

    std::size_t written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*source,
        bytes.get(), persistence::kMaximumEncodedCheckpointBytes, written)
        == persistence::CodecError::none);
    ARPG_REQUIRE(bytes[written - 1U]
        == persistence::kV9CanonicalSecondaryOrdinalMarker);
    --written;
    refresh_v9_envelope(bytes.get(), written);

    bool migrated = false;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), written, *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(migrated);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground_count == 1U);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground[0U].ordinal == 2U);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground[0U].source == 0U);
    return {};
}

test::Failure unmarked_canonical_v9_common_ordinal_uses_spawn_position()
    noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> source{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    std::unique_ptr<std::uint8_t[]> rewritten{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(source != nullptr && decoded != nullptr && bytes != nullptr
        && rewritten != nullptr);
    ARPG_REQUIRE(make_fixture(*source));
    clear_secondary_progress(*source);

    auto& room = source->room_progress;
    ARPG_REQUIRE(room.combat.monster_count >= 3U);
    ARPG_REQUIRE(room.combat.monsters[2U].ordinal == 2U);
    room.secondary_ground_count = 1U;
    room.secondary_ground[0U] = {};
    room.secondary_ground[0U].tag =
        checkpoint::SecondaryGroundTag::material;
    room.secondary_ground[0U].ordinal = 4U;
    room.secondary_ground[0U].source = 0U;
    room.secondary_ground[0U].position = room.combat.monsters[2U].position;
    room.secondary_ground[0U].material =
        items::MaterialId::reinforcement_stone;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        room, source->state));

    std::size_t written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*source,
        bytes.get(), persistence::kMaximumEncodedCheckpointBytes, written)
        == persistence::CodecError::none);
    ARPG_REQUIRE(bytes[written - 1U]
        == persistence::kV9CanonicalSecondaryOrdinalMarker);
    --written;
    refresh_v9_envelope(bytes.get(), written);

    bool migrated = false;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), written, *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(migrated);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground_count == 1U);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground[0U].ordinal == 4U);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground[0U].source == 0U);

    std::size_t rewritten_size{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*decoded,
        rewritten.get(), persistence::kMaximumEncodedCheckpointBytes,
        rewritten_size) == persistence::CodecError::none);
    ARPG_REQUIRE(rewritten[rewritten_size - 1U]
        == persistence::kV9CanonicalSecondaryOrdinalMarker);
    return {};
}

test::Failure unmarked_v9_rejects_ambiguous_common_ordinal_position()
    noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> source{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(source != nullptr && decoded != nullptr && bytes != nullptr);
    ARPG_REQUIRE(make_fixture(*source));
    clear_secondary_progress(*source);

    auto& room = source->room_progress;
    ARPG_REQUIRE(room.combat.monster_count >= 3U);
    room.combat.monsters[2U].position = room.combat.monsters[1U].position;
    room.secondary_ground_count = 1U;
    room.secondary_ground[0U] = {};
    room.secondary_ground[0U].tag =
        checkpoint::SecondaryGroundTag::material;
    room.secondary_ground[0U].ordinal = 4U;
    room.secondary_ground[0U].source = 0U;
    room.secondary_ground[0U].position = room.combat.monsters[1U].position;
    room.secondary_ground[0U].material =
        items::MaterialId::reinforcement_stone;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        room, source->state));

    std::size_t written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*source,
        bytes.get(), persistence::kMaximumEncodedCheckpointBytes, written)
        == persistence::CodecError::none);
    ARPG_REQUIRE(bytes[written - 1U]
        == persistence::kV9CanonicalSecondaryOrdinalMarker);
    --written;
    refresh_v9_envelope(bytes.get(), written);

    bool migrated = false;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), written, *decoded, migrated)
        == persistence::CodecError::invalid_state);
    return {};
}

test::Failure unmarked_v9_rejects_mixed_secondary_ordinal_semantics()
    noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> source{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(source != nullptr && decoded != nullptr && bytes != nullptr);
    ARPG_REQUIRE(make_fixture(*source));
    clear_secondary_progress(*source);

    auto& room = source->room_progress;
    ARPG_REQUIRE(room.combat.monster_count >= 3U);
    room.secondary_ground_count = 2U;
    room.secondary_ground[0U] = {};
    room.secondary_ground[0U].tag =
        checkpoint::SecondaryGroundTag::material;
    room.secondary_ground[0U].ordinal = 4U;
    room.secondary_ground[0U].source = 0U;
    room.secondary_ground[0U].position = room.combat.monsters[2U].position;
    room.secondary_ground[0U].material =
        items::MaterialId::reinforcement_stone;
    room.secondary_ground[1U] = {};
    room.secondary_ground[1U].tag =
        checkpoint::SecondaryGroundTag::material;
    room.secondary_ground[1U].ordinal = 5U;
    room.secondary_ground[1U].source = 1U;
    room.secondary_ground[1U].position = room.combat.monsters[1U].position;
    room.secondary_ground[1U].material = items::MaterialId::coupon_6;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        room, source->state));

    std::size_t written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*source,
        bytes.get(), persistence::kMaximumEncodedCheckpointBytes, written)
        == persistence::CodecError::none);
    ARPG_REQUIRE(rewrite_coupon_record_as_task5_wire(
        bytes.get(), written, room.secondary_ground[1U], 6U));
    ARPG_REQUIRE(bytes[written - 1U]
        == persistence::kV9CanonicalSecondaryOrdinalMarker);
    --written;
    refresh_v9_envelope(bytes.get(), written);

    bool migrated = false;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), written, *decoded, migrated)
        == persistence::CodecError::invalid_state);
    return {};
}

test::Failure maximum_legal_v9_ground_and_claim_payloads_round_trip() noexcept {
    constexpr std::uint16_t kMaximumEquipmentRecords = 1152U;
    constexpr std::uint16_t kMaximumSecondaryRecords = 2304U;
    std::unique_ptr<checkpoint::SaveCheckpointSlot> source{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(source != nullptr && decoded != nullptr && bytes != nullptr);
    ARPG_REQUIRE(make_fixture(*source));
    auto& room = source->room_progress;
    room.equipment_claim_bits = {};
    room.secondary_claim_bits = {};
    source->state.item_ownership.claimed_drop_bits = {};
    source->state.item_ownership.material_claimed_drop_bits = {};

    room.equipment_ground_count = kMaximumEquipmentRecords;
    for (std::uint16_t ordinal = 0U;
            ordinal < kMaximumEquipmentRecords; ++ordinal) {
        auto& ground = room.equipment_ground[ordinal];
        ground = {};
        ground.ordinal = ordinal;
        ground.source = 0U;
        ground.reward_ordinal = 0xFFU;
        ground.position = {
            static_cast<float>(static_cast<int>(ordinal % 161U) - 80),
            static_cast<float>(static_cast<int>((ordinal / 7U) % 161U) - 80),
            0.0F};
        ground.item = normal_item(0x100000U + ordinal);
    }
    room.secondary_ground_count = kMaximumSecondaryRecords;
    for (std::uint16_t ordinal = 0U;
            ordinal < kMaximumSecondaryRecords; ++ordinal) {
        auto& ground = room.secondary_ground[ordinal];
        ground = {};
        ground.ordinal = ordinal;
        ground.position = {
            static_cast<float>(static_cast<int>(ordinal % 161U) - 80),
            static_cast<float>(static_cast<int>((ordinal / 11U) % 161U) - 80),
            0.0F};
        if ((ordinal & 1U) == 0U) {
            ground.tag = checkpoint::SecondaryGroundTag::material;
            ground.source = 0U;
            ground.material = items::MaterialId::reinforcement_stone;
        } else if ((ordinal & 2U) == 0U) {
            ground.tag = checkpoint::SecondaryGroundTag::material;
            ground.source = 1U;
            ground.material = items::MaterialId::coupon_6;
        } else {
            ground.tag = checkpoint::SecondaryGroundTag::health_potion;
            ground.source = 0U;
            ground.material = items::MaterialId::count;
        }
    }
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        room, source->state));

    std::size_t maximum_record_bytes{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*source,
        bytes.get(), persistence::kMaximumEncodedCheckpointBytes,
        maximum_record_bytes) == persistence::CodecError::none);
    ARPG_REQUIRE(maximum_record_bytes
        < persistence::kMaximumEncodedCheckpointBytes);
    ARPG_REQUIRE(maximum_record_bytes <= persistence::kV9MaximumEncodedBytes);
    bool migrated = true;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(bytes.get(),
        maximum_record_bytes, *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(!migrated);
    ARPG_REQUIRE(checkpoint::same_room_progress_checkpoint(
        room, decoded->room_progress));
    ARPG_REQUIRE(decoded->room_progress.equipment_ground_count
        == kMaximumEquipmentRecords);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground_count
        == kMaximumSecondaryRecords);
    for (std::uint16_t ordinal = 0U;
            ordinal < kMaximumEquipmentRecords; ++ordinal) {
        const auto& expected = room.equipment_ground[ordinal];
        const auto& actual = decoded->room_progress.equipment_ground[ordinal];
        ARPG_REQUIRE(actual.ordinal == expected.ordinal);
        ARPG_REQUIRE(actual.source == expected.source);
        ARPG_REQUIRE(actual.reward_ordinal == expected.reward_ordinal);
        ARPG_REQUIRE(same_vec(actual.position, expected.position));
        ARPG_REQUIRE(actual.item.id == expected.item.id);
        ARPG_REQUIRE(actual.item.base_id == expected.item.base_id);
        ARPG_REQUIRE(actual.item.rarity == expected.item.rarity);
        ARPG_REQUIRE(actual.item.item_level == expected.item.item_level);
        ARPG_REQUIRE(actual.item.required_level == expected.item.required_level);
    }
    for (std::uint16_t ordinal = 0U;
            ordinal < kMaximumSecondaryRecords; ++ordinal) {
        const auto& expected = room.secondary_ground[ordinal];
        const auto& actual = decoded->room_progress.secondary_ground[ordinal];
        ARPG_REQUIRE(actual.tag == expected.tag);
        ARPG_REQUIRE(actual.ordinal == expected.ordinal);
        ARPG_REQUIRE(actual.source == expected.source);
        ARPG_REQUIRE(same_vec(actual.position, expected.position));
        ARPG_REQUIRE(actual.material == expected.material);
    }

    room.equipment_ground_count = 0U;
    room.secondary_ground_count = 0U;
    room.equipment_claim_bits.fill((std::numeric_limits<std::uint64_t>::max)());
    room.secondary_claim_bits.fill((std::numeric_limits<std::uint64_t>::max)());
    room.secondary_claim_bits.back() = 0U;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        room, source->state));
    std::size_t maximum_claim_bytes{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*source,
        bytes.get(), persistence::kMaximumEncodedCheckpointBytes,
        maximum_claim_bytes) == persistence::CodecError::none);
    migrated = true;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(bytes.get(),
        maximum_claim_bytes, *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(!migrated);
    ARPG_REQUIRE(decoded->room_progress.equipment_claim_bits
        == room.equipment_claim_bits);
    ARPG_REQUIRE(decoded->room_progress.secondary_claim_bits
        == room.secondary_claim_bits);
    ARPG_REQUIRE(decoded->room_progress.equipment_ground_count == 0U);
    ARPG_REQUIRE(decoded->room_progress.secondary_ground_count == 0U);
    return {};
}

test::Failure v9_rejects_crc_and_length_corruption() noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> source{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
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

test::Failure v9_partial_unlock_round_trip_restores_combat() noexcept {
    dungeon::DungeonRules rules{};
    const auto initial = dungeon::make_initial_run_state(0x25100CULL, rules);
    ARPG_REQUIRE(initial.fault == dungeon::DungeonFault::none);
    dungeon::DungeonSession session{rules, initial.state};
    session.tick({});
    test::set_player_health(session, 1000000, 1000000);
    drain_events(session);
    const std::uint32_t required = dungeon::required_kills(
        session.snapshot().initial_monster_count);
    for (std::uint32_t ordinal = 0U; ordinal < required; ++ordinal) {
        ARPG_REQUIRE(test::relay_defeated(session, 0U,
            static_cast<combat::MonsterOrdinal>(ordinal),
            {20.0F, 20.0F, 0.0F}, true,
            combat::MonsterId::fire_bomber,
            static_cast<std::uint16_t>(ordinal), 0U, false));
    }
    session.tick({});
    const dungeon::PendingSave* const pending = session.pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    ARPG_REQUIRE(pending->kind == dungeon::PendingSaveKind::room_unlock);
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        pending->expected_generation, pending->next_state, pending->kind});
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::combat);
    ARPG_REQUIRE(session.snapshot().exits_unlocked);

    std::unique_ptr<checkpoint::SaveCheckpointSlot> saved{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(saved != nullptr && decoded != nullptr && bytes != nullptr);
    ARPG_REQUIRE(session.capture_save_checkpoint(*saved, 55U));
    ARPG_REQUIRE(saved->room_progress.exits_unlocked);
    ARPG_REQUIRE(!saved->room_progress.full_clear);
    std::size_t written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*saved, bytes.get(),
        persistence::kMaximumEncodedCheckpointBytes, written)
        == persistence::CodecError::none);
    ARPG_REQUIRE(persistence::verify_checkpoint_v9_readback(
        bytes.get(), written, *saved, bytes.get(), written)
        == persistence::CodecError::none);
    bool migrated = true;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), written, *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(!migrated);
    ARPG_REQUIRE(decoded->room_progress.exits_unlocked);
    ARPG_REQUIRE(!decoded->room_progress.full_clear);

    dungeon::DungeonSession reloaded{rules, decoded->state};
    test::set_player_health(reloaded, 1000000, 1000000);
    ARPG_REQUIRE(reloaded.restore_room_progress_checkpoint(*decoded));
    const auto restored = reloaded.snapshot();
    ARPG_REQUIRE(restored.phase == dungeon::RoomPhase::combat);
    ARPG_REQUIRE(restored.exits_unlocked);
    ARPG_REQUIRE(restored.remaining_targets > 0U);
    for (const bool open : restored.exits_open) ARPG_REQUIRE(open);
    return {};
}

std::unique_ptr<dungeon::DungeonSession>
make_maximum_population_session() noexcept {
    dungeon::DungeonRules rules{};
    auto initial = dungeon::make_initial_run_state(0xD20F1152ULL, rules);
    if (initial.fault != dungeon::DungeonFault::none) return nullptr;
    bool found = false;
    for (std::uint64_t seed = 1U; seed < 100000U; ++seed) {
        const auto selected = abyss::select_abyss_rule(seed, 40U);
        if (!abyss::is_abyss_roll(seed) || !selected.has_value()
                || dungeon::roll_room_density(seed, true).total_count
                    != 1125U) {
            continue;
        }
        initial.state.current_room.seed = seed;
        initial.state.current_room.depth = 40U;
        initial.state.current_room.entry = dungeon::EntrySide::left;
        initial.state.current_room.ecology = dungeon::DungeonElement::chaos;
        initial.state.current_room.has_hole = true;
        initial.state.current_room.is_abyss = true;
        initial.state.last_transition = dungeon::TransitionKind::door;
        initial.state.last_direction = dungeon::ExitDirection::right;
        initial.state.abyss.lifecycle = abyss::AbyssLifecycle::available;
        initial.state.abyss.danger = selected->danger;
        initial.state.abyss.rule = selected->rule;
        initial.state.abyss.rules_version = selected->rules_version;
        found = true;
        break;
    }
    if (!found) return nullptr;
    std::unique_ptr<dungeon::DungeonSession> session{
        new (std::nothrow) dungeon::DungeonSession{rules, initial.state}};
    if (session == nullptr || session->pending_save_view() == nullptr
            || session->pending_save_view()->kind
                != dungeon::PendingSaveKind::abyss_start
            || !test::commit_pending(*session)) {
        return nullptr;
    }
    session->tick({});
    if (session->snapshot().initial_monster_count != 1125U
            || session->phase() == dungeon::RoomPhase::faulted) {
        return nullptr;
    }
    return session;
}

std::unique_ptr<dungeon::DungeonSession> restore_session_from(
    const checkpoint::SaveCheckpointSlot& slot) noexcept {
    std::unique_ptr<dungeon::DungeonSession> restored{
        new (std::nothrow) dungeon::DungeonSession{
            dungeon::DungeonRules{}, slot.state}};
    if (restored == nullptr
            || !restored->restore_room_progress_checkpoint(slot)) {
        return nullptr;
    }
    return restored;
}

test::Failure high_ordinal_session_v9_round_trip_and_claims_are_exact()
    noexcept {
    constexpr std::array<std::uint16_t, 4U> kOrdinals{{
        191U, 192U, 511U, 1124U}};
    auto session = make_maximum_population_session();
    ARPG_REQUIRE(session != nullptr);
    const combat::RoomMonsterPlan* const plan =
        test::DungeonSessionTestAccess::room_monster_plan(*session);
    ARPG_REQUIRE(plan != nullptr && plan->monster_count == 1125U);
    for (const std::uint16_t ordinal : kOrdinals) {
        items::ItemInstance item = normal_item(
            static_cast<std::uint64_t>(ordinal) + 1000U);
        item.item_level = 40U;
        test::DungeonSessionTestAccess::install_ground_item(
            *session, ordinal, item, plan->monsters[ordinal].initial_position);
        ARPG_REQUIRE(test::DungeonSessionTestAccess::ground_items(
            *session)[ordinal].active);
    }
    const combat::Vec3 last_position =
        plan->monsters[1124U].initial_position;
    test::DungeonSessionTestAccess::install_ground_material(*session, 2248U,
        items::MaterialId::reinforcement_stone, last_position,
        dungeon::GroundMaterialSource::monster_common);
    test::DungeonSessionTestAccess::install_ground_material(*session, 2249U,
        items::MaterialId::coupon_6, last_position,
        dungeon::GroundMaterialSource::monster_coupon);
    ARPG_REQUIRE(test::DungeonSessionTestAccess::ground_materials(
        *session)[2248U].active);
    ARPG_REQUIRE(test::DungeonSessionTestAccess::ground_materials(
        *session)[2249U].active);

    const std::unique_ptr<checkpoint::SaveCheckpointSlot> saved{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    const std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    const std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(saved != nullptr && decoded != nullptr && bytes != nullptr);
    saved->state.item_ownership.items.reserve(kOrdinals.size());
    ARPG_REQUIRE(session->capture_save_checkpoint(*saved, 71U));
    std::size_t written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*saved, bytes.get(),
        persistence::kMaximumEncodedCheckpointBytes, written)
        == persistence::CodecError::none);
    bool migrated = true;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(bytes.get(), written,
        *decoded, migrated) == persistence::CodecError::none);
    ARPG_REQUIRE(!migrated);

    auto not_committed = restore_session_from(*decoded);
    ARPG_REQUIRE(not_committed != nullptr);
    test::set_player_position(*not_committed,
        test::DungeonSessionTestAccess::ground_items(
            *not_committed)[1124U].position);
    ARPG_REQUIRE(not_committed->request_pickup(1124U)
        == dungeon::RequestResult::accepted);
    const dungeon::PendingSave* pending =
        not_committed->pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    not_committed->resolve_pending_save({
        dungeon::SaveDisposition::not_committed,
        pending->expected_generation, pending->next_state, pending->kind});
    ARPG_REQUIRE(test::DungeonSessionTestAccess::ground_items(
        *not_committed)[1124U].active);
    ARPG_REQUIRE(not_committed->item_state().items.empty());

    auto indeterminate = restore_session_from(*decoded);
    ARPG_REQUIRE(indeterminate != nullptr);
    test::set_player_position(*indeterminate,
        test::DungeonSessionTestAccess::ground_items(
            *indeterminate)[511U].position);
    ARPG_REQUIRE(indeterminate->request_pickup(511U)
        == dungeon::RequestResult::accepted);
    indeterminate->resolve_pending_save({
        dungeon::SaveDisposition::indeterminate, 0U, {}});
    ARPG_REQUIRE(indeterminate->phase() == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(test::DungeonSessionTestAccess::ground_items(
        *indeterminate)[511U].active);
    ARPG_REQUIRE(indeterminate->item_state().items.empty());

    auto claimed = restore_session_from(*decoded);
    ARPG_REQUIRE(claimed != nullptr);
    for (const std::uint16_t ordinal : kOrdinals) {
        test::set_player_position(*claimed,
            test::DungeonSessionTestAccess::ground_items(
                *claimed)[ordinal].position);
        ARPG_REQUIRE(claimed->request_pickup(ordinal)
            == dungeon::RequestResult::accepted);
        ARPG_REQUIRE(test::commit_pending(*claimed));
        ARPG_REQUIRE(claimed->phase() != dungeon::RoomPhase::faulted);
        ARPG_REQUIRE(claimed->request_pickup(ordinal)
            == dungeon::RequestResult::rejected);
    }
    for (const std::uint16_t ordinal : {std::uint16_t{2248U},
             std::uint16_t{2249U}}) {
        test::set_player_position(*claimed,
            test::DungeonSessionTestAccess::ground_materials(
                *claimed)[ordinal].position);
        ARPG_REQUIRE(claimed->request_material_pickup(ordinal)
            == dungeon::RequestResult::accepted);
        ARPG_REQUIRE(test::commit_pending(*claimed));
        ARPG_REQUIRE(claimed->request_material_pickup(ordinal)
            == dungeon::RequestResult::rejected);
    }
    ARPG_REQUIRE(claimed->item_state().items.size() == kOrdinals.size());
    ARPG_REQUIRE(claimed->item_state().materials[
        items::material_index(items::MaterialId::reinforcement_stone)] == 1U);
    ARPG_REQUIRE(claimed->item_state().materials[
        items::material_index(items::MaterialId::coupon_6)] == 1U);

    ARPG_REQUIRE(claimed->capture_save_checkpoint(*saved, 72U));
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*saved, bytes.get(),
        persistence::kMaximumEncodedCheckpointBytes, written)
        == persistence::CodecError::none);
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(bytes.get(), written,
        *decoded, migrated) == persistence::CodecError::none);
    auto claimed_reload = restore_session_from(*decoded);
    ARPG_REQUIRE(claimed_reload != nullptr);
    for (const std::uint16_t ordinal : kOrdinals) {
        ARPG_REQUIRE(!test::DungeonSessionTestAccess::ground_items(
            *claimed_reload)[ordinal].active);
        ARPG_REQUIRE(claimed_reload->request_pickup(ordinal)
            == dungeon::RequestResult::rejected);
    }
    ARPG_REQUIRE(!test::DungeonSessionTestAccess::ground_materials(
        *claimed_reload)[2248U].active);
    ARPG_REQUIRE(!test::DungeonSessionTestAccess::ground_materials(
        *claimed_reload)[2249U].active);
    return {};
}

test::Failure v8_load_starts_fresh_room_drop_authority() noexcept {
    dungeon::DungeonRules rules{};
    auto initial = dungeon::make_initial_run_state(0xB8F2E5AULL, rules);
    ARPG_REQUIRE(initial.fault == dungeon::DungeonFault::none);
    initial.state.item_ownership.items.push_back(normal_item(1U));
    initial.state.item_ownership.next_item_sequence = 2U;
    initial.state.item_ownership.claimed_drop_bits = {{
        0xFFFFFFFFFFFFFFFFULL, 0x8000000000000001ULL, 0x55AAULL}};
    initial.state.item_ownership.material_claimed_drop_bits[0U] =
        0xFFFFFFFFFFFFFFFFULL;
    initial.state.item_ownership.material_claimed_drop_bits[6U] = 0xA55AU;

    const std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(bytes != nullptr);
    std::size_t written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_into(initial.state,
        bytes.get(), persistence::kMaximumEncodedCheckpointBytes, written)
        == persistence::CodecError::none);
    const persistence::DecodeResult decoded =
        persistence::decode_checkpoint(bytes.get(), written);
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.state.item_ownership.items.size() == 1U);
    ARPG_REQUIRE(decoded.state.item_ownership.claimed_drop_bits
        == initial.state.item_ownership.claimed_drop_bits);

    const std::unique_ptr<checkpoint::SaveCheckpointSlot> migrated{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(migrated != nullptr);
    bool was_migrated = false;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(bytes.get(), written,
        *migrated, was_migrated) == persistence::CodecError::none);
    ARPG_REQUIRE(was_migrated);
    ARPG_REQUIRE(migrated->state.item_ownership.items.size() == 1U);
    ARPG_REQUIRE(migrated->state.item_ownership.claimed_drop_bits
        == decltype(migrated->state.item_ownership.claimed_drop_bits){});
    ARPG_REQUIRE(migrated->state.item_ownership.material_claimed_drop_bits
        == decltype(
            migrated->state.item_ownership.material_claimed_drop_bits){});

    dungeon::DungeonSession migrated_session{rules, migrated->state};
    ARPG_REQUIRE(migrated_session.item_state().items.size() == 1U);
    const std::unique_ptr<checkpoint::SaveCheckpointSlot> fresh{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(fresh != nullptr);
    fresh->state.item_ownership.items.reserve(
        migrated_session.item_state().items.size());
    ARPG_REQUIRE(migrated_session.capture_save_checkpoint(*fresh, 1U));
    ARPG_REQUIRE(fresh->room_progress.equipment_claim_bits
        == decltype(fresh->room_progress.equipment_claim_bits){});
    ARPG_REQUIRE(fresh->room_progress.secondary_claim_bits
        == decltype(fresh->room_progress.secondary_claim_bits){});
    ARPG_REQUIRE(fresh->room_progress.equipment_ground_count == 0U);
    ARPG_REQUIRE(fresh->room_progress.secondary_ground_count == 0U);
    return {};
}

test::Failure v9_resolution_lifecycle_tail_is_backward_compatible() noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> source{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(source != nullptr && decoded != nullptr && bytes != nullptr);
    ARPG_REQUIRE(make_fixture(*source));
    clear_secondary_progress(*source);

    std::size_t written{};
    const auto encode = [&]() noexcept {
        return persistence::encode_checkpoint_v9_into(*source, bytes.get(),
            persistence::kMaximumEncodedCheckpointBytes, written);
    };
    ARPG_REQUIRE(encode() == persistence::CodecError::none);
    ARPG_REQUIRE(bytes[written - 2U]
        == static_cast<std::uint8_t>(abyss::AbyssLifecycle::none));
    ARPG_REQUIRE(bytes[written - 1U]
        == persistence::kV9CanonicalSecondaryOrdinalMarker);

    const std::size_t old_v9_size = written - 1U;
    refresh_v9_envelope(bytes.get(), old_v9_size);
    bool migrated = true;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), old_v9_size, *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(migrated);
    ARPG_REQUIRE(decoded->state.last_abyss_resolution.lifecycle
        == abyss::AbyssLifecycle::none);
    ARPG_REQUIRE(persistence::verify_checkpoint_v9_readback(
        bytes.get(), old_v9_size, *source, bytes.get(), old_v9_size)
        == persistence::CodecError::bad_payload_length);

    ARPG_REQUIRE(encode() == persistence::CodecError::none);
    const std::size_t truncated_size = written - 3U;
    refresh_v9_envelope(bytes.get(), truncated_size);
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), truncated_size, *decoded, migrated)
        == persistence::CodecError::bad_payload_length);

    ARPG_REQUIRE(encode() == persistence::CodecError::none);
    bytes[written] = 0U;
    refresh_v9_envelope(bytes.get(), written + 1U);
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), written + 1U, *decoded, migrated)
        == persistence::CodecError::bad_payload_length);

    ARPG_REQUIRE(encode() == persistence::CodecError::none);
    bytes[written - 1U] = 0xFEU;
    refresh_v9_envelope(bytes.get(), written);
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), written, *decoded, migrated)
        == persistence::CodecError::invalid_enum);
    return {};
}

test::Failure v9_legacy_abyss_death_restores_and_rewrites_failed() noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> source{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(source != nullptr && decoded != nullptr && bytes != nullptr);
    ARPG_REQUIRE(make_pending_death_fixture(*source, true));
    clear_secondary_progress(*source);

    const auto historical_resolution = source->state.last_abyss_resolution;
    std::size_t old_v9_size{};
    ARPG_REQUIRE(encode_legacy_v9_without_lifecycle(
        *source, bytes.get(), persistence::kMaximumEncodedCheckpointBytes,
        old_v9_size));
    bool migrated = true;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), old_v9_size, *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(migrated);
    dungeon::DungeonSession restored{dungeon::DungeonRules{}, decoded->state};
    const auto restored_snapshot = restored.snapshot();
    if (decoded->state.last_abyss_resolution.lifecycle
                == abyss::AbyssLifecycle::none
            && restored_snapshot.phase == dungeon::RoomPhase::faulted
            && restored_snapshot.diagnostics.fault
                == dungeon::DungeonFault::death_sequence_mismatch) {
        return {"legacy V9 abyss death kept lifecycle none and restore faulted",
            __FILE__, __LINE__};
    }
    ARPG_REQUIRE(decoded->state.last_abyss_resolution.lifecycle
        == abyss::AbyssLifecycle::failed);
    ARPG_REQUIRE(restored_snapshot.phase == dungeon::RoomPhase::death_pending);
    ARPG_REQUIRE(restored_snapshot.diagnostics.fault
        == dungeon::DungeonFault::none);

    std::size_t rewritten_size{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(
        *decoded, bytes.get(), persistence::kMaximumEncodedCheckpointBytes,
        rewritten_size) == persistence::CodecError::none);
    ARPG_REQUIRE(rewritten_size == old_v9_size + 2U);
    ARPG_REQUIRE(bytes[rewritten_size - 2U] == static_cast<std::uint8_t>(
        abyss::AbyssLifecycle::failed));
    ARPG_REQUIRE(bytes[rewritten_size - 1U]
        == persistence::kV9CanonicalSecondaryOrdinalMarker);

    checkpoint::clear_save_checkpoint_slot(*source);
    ARPG_REQUIRE(make_fixture(*source));
    clear_secondary_progress(*source);
    source->state.last_abyss_resolution = historical_resolution;
    source->state.last_abyss_resolution.lifecycle =
        abyss::AbyssLifecycle::none;
    ARPG_REQUIRE(encode_legacy_v9_without_lifecycle(
        *source, bytes.get(), persistence::kMaximumEncodedCheckpointBytes,
        old_v9_size));
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), old_v9_size, *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(migrated);
    ARPG_REQUIRE(decoded->state.last_abyss_resolution.lifecycle
        == abyss::AbyssLifecycle::none);

    ARPG_REQUIRE(make_pending_death_fixture(*source, false, true));
    clear_secondary_progress(*source);
    ARPG_REQUIRE(source->state.last_abyss_resolution.lifecycle
        == abyss::AbyssLifecycle::none);
    ARPG_REQUIRE(encode_legacy_v9_without_lifecycle(
        *source, bytes.get(), persistence::kMaximumEncodedCheckpointBytes,
        old_v9_size));
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), old_v9_size, *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(migrated);
    ARPG_REQUIRE(decoded->state.last_abyss_resolution.lifecycle
        == abyss::AbyssLifecycle::none);
    dungeon::DungeonSession ordinary_restored{
        dungeon::DungeonRules{}, decoded->state};
    ARPG_REQUIRE(ordinary_restored.snapshot().phase
        == dungeon::RoomPhase::death_pending);
    return {};
}

test::Failure v9_started_abyss_early_exit_round_trip(
    bool expect_next_abyss) noexcept {
    dungeon::DungeonRules rules{};
    dungeon::DungeonRunState state{};
    dungeon::ExitDirection direction = dungeon::ExitDirection::none;
    bool found = false;
    for (std::uint64_t seed = 1U; seed < 100000U && !found; ++seed) {
        if (!abyss::is_abyss_roll(seed)) continue;
        state = dungeon::make_initial_run_state(0xAB155EEDULL, rules).state;
        state.current_room.seed = seed;
        state.current_room.depth = 40U;
        state.current_room.entry = dungeon::EntrySide::left;
        state.current_room.ecology = dungeon::DungeonElement::water;
        state.current_room.has_hole = true;
        state.current_room.is_abyss = true;
        state.last_transition = dungeon::TransitionKind::door;
        state.last_direction = dungeon::ExitDirection::right;
        const auto selection = abyss::select_abyss_rule(seed, 40U);
        if (!selection.has_value()) continue;
        state.abyss.lifecycle = abyss::AbyssLifecycle::available;
        state.abyss.danger = selection->danger;
        state.abyss.rule = selection->rule;
        state.abyss.rules_version = selection->rules_version;
        const auto preview = dungeon::preview_abyss_doors(state.current_room);
        constexpr std::array<dungeon::ExitDirection, 4> directions{{
            dungeon::ExitDirection::up,
            dungeon::ExitDirection::down,
            dungeon::ExitDirection::left,
            dungeon::ExitDirection::right,
        }};
        for (std::size_t index = 0U; index < preview.size(); ++index) {
            if (preview[index] == expect_next_abyss) {
                direction = directions[index];
                found = true;
                break;
            }
        }
    }
    ARPG_REQUIRE(found);
    const std::uint64_t failed_room_seed = state.current_room.seed;

    dungeon::DungeonSession session{rules, state};
    ARPG_REQUIRE(session.pending_save_view() != nullptr);
    ARPG_REQUIRE(session.pending_save_view()->kind
        == dungeon::PendingSaveKind::abyss_start);
    ARPG_REQUIRE(test::commit_pending(session));
    session.tick({});
    test::set_player_health(session, 1000000, 1000000);
    drain_events(session);
    const std::uint32_t required = dungeon::required_kills(
        session.snapshot().initial_monster_count);
    for (std::uint32_t ordinal = 0U; ordinal < required; ++ordinal) {
        ARPG_REQUIRE(test::relay_defeated(session, 0U,
            static_cast<combat::MonsterOrdinal>(ordinal),
            {20.0F, 20.0F, 0.0F}, true,
            combat::MonsterId::fire_bomber,
            static_cast<std::uint16_t>(ordinal), 0U, false));
    }
    session.tick({});
    ARPG_REQUIRE(session.pending_save_view() != nullptr);
    ARPG_REQUIRE(session.pending_save_view()->kind
        == dungeon::PendingSaveKind::room_unlock);
    ARPG_REQUIRE(test::commit_pending(session));
    test::attempt_exit(session, direction);
    const dungeon::PendingSave* const pending = session.pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    ARPG_REQUIRE(pending->kind
        == dungeon::PendingSaveKind::abyss_early_exit);

    std::unique_ptr<checkpoint::SaveCheckpointSlot> saved{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(saved != nullptr && decoded != nullptr && bytes != nullptr);
    saved->state.item_ownership.items.reserve(
        pending->next_state.item_ownership.items.size());
    ARPG_REQUIRE(session.capture_save_checkpoint(
        *saved, 56U, &pending->next_state));
    ARPG_REQUIRE(saved->room_progress.lifecycle
        == checkpoint::RoomProgressLifecycle::none);
    std::size_t written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*saved, bytes.get(),
        persistence::kMaximumEncodedCheckpointBytes, written)
        == persistence::CodecError::none);
    const std::uint32_t durable_size = read_u32(bytes.get(), 32U);
    const std::uint8_t* const durable = bytes.get() + 36U;
    ARPG_REQUIRE(durable_size > 150U);
    ARPG_REQUIRE(durable[150U] == 0U);
    const auto public_v8 = persistence::decode_checkpoint(
        durable, durable_size);
    ARPG_REQUIRE(public_v8.error == persistence::CodecError::none);
    ARPG_REQUIRE(public_v8.state.last_abyss_resolution.lifecycle
        == abyss::AbyssLifecycle::none);
    ARPG_REQUIRE(bytes[written - 2U]
        == static_cast<std::uint8_t>(abyss::AbyssLifecycle::failed));
    ARPG_REQUIRE(bytes[written - 1U]
        == persistence::kV9CanonicalSecondaryOrdinalMarker);
    ARPG_REQUIRE(persistence::verify_checkpoint_v9_readback(
        bytes.get(), written, *saved, bytes.get(), written)
        == persistence::CodecError::none);
    bool migrated = true;
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(
        bytes.get(), written, *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(!migrated);
    ARPG_REQUIRE(decoded->state.current_room.is_abyss == expect_next_abyss);
    ARPG_REQUIRE(decoded->state.abyss.lifecycle
        == (expect_next_abyss ? abyss::AbyssLifecycle::available
                              : abyss::AbyssLifecycle::none));
    ARPG_REQUIRE(decoded->state.last_abyss_resolution.valid);
    ARPG_REQUIRE(decoded->state.last_abyss_resolution.lifecycle
        == abyss::AbyssLifecycle::failed);
    ARPG_REQUIRE(decoded->state.last_abyss_resolution.room_seed
        == failed_room_seed);
    ARPG_REQUIRE(decoded->state.last_abyss_resolution.generated == 0U);
    ARPG_REQUIRE(decoded->state.last_abyss_resolution.claimed == 0U);
    ARPG_REQUIRE(decoded->state.last_abyss_resolution.abandoned
        == decoded->state.last_abyss_resolution.total);
    std::size_t v8_written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_into(
        saved->state, bytes.get(), persistence::kMaximumEncodedCheckpointBytes,
        v8_written) == persistence::CodecError::invalid_state);
    saved->state.last_abyss_resolution.lifecycle =
        abyss::AbyssLifecycle::cleared;
    std::size_t rejected_written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(
        *saved, bytes.get(), persistence::kMaximumEncodedCheckpointBytes,
        rejected_written) == persistence::CodecError::invalid_state);
    return {};
}

test::Failure v9_started_abyss_early_exit_to_normal_round_trip() noexcept {
    return v9_started_abyss_early_exit_round_trip(false);
}

test::Failure v9_started_abyss_early_exit_to_abyss_round_trip() noexcept {
    return v9_started_abyss_early_exit_round_trip(true);
}

test::Failure v8_reserved_resolution_lifecycle_remains_zero() noexcept {
    dungeon::DungeonRules rules{};
    const auto initial = dungeon::make_initial_run_state(0x25100CULL, rules);
    ARPG_REQUIRE(initial.fault == dungeon::DungeonFault::none);
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            persistence::kMaximumEncodedCheckpointBytes]};
    ARPG_REQUIRE(bytes != nullptr);
    std::size_t written{};
    ARPG_REQUIRE(persistence::encode_checkpoint_into(
        initial.state, bytes.get(), persistence::kMaximumEncodedCheckpointBytes,
        written) == persistence::CodecError::none);
    ARPG_REQUIRE(written > 150U);
    ARPG_REQUIRE(bytes[150U] == 0U);
    const auto decoded = persistence::decode_checkpoint(bytes.get(), written);
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.state.last_abyss_resolution.lifecycle
        == abyss::AbyssLifecycle::none);

    bytes[150U] = static_cast<std::uint8_t>(abyss::AbyssLifecycle::failed);
    auto checksum = persistence::crc32_update(
        0U, bytes.get() + 8U, 20U);
    checksum = persistence::crc32_update(
        checksum, bytes.get() + persistence::kCheckpointHeaderSize,
        written - persistence::kCheckpointHeaderSize);
    for (std::size_t index = 0U; index < sizeof(checksum); ++index) {
        bytes[28U + index] = static_cast<std::uint8_t>(
            checksum >> (index * 8U));
    }
    ARPG_REQUIRE(persistence::decode_checkpoint(bytes.get(), written).error
        == persistence::CodecError::invalid_state);
    return {};
}

test::Failure structural_validation_rejects_identity_and_order_faults() noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> slot{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(slot != nullptr);
    ARPG_REQUIRE(make_fixture(*slot));
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        slot->room_progress, slot->state));

    std::unique_ptr<checkpoint::SaveCheckpointSlot> none_slot{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(none_slot != nullptr);
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        none_slot->room_progress, none_slot->state));
    none_slot->room_progress.combat.player.state =
        checkpoint::PlayerState::landing;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        none_slot->room_progress, none_slot->state));
    none_slot->room_progress.combat.player.state =
        checkpoint::PlayerState::idle;
    none_slot->room_progress.combat.attack.elapsed_ticks = 1U;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        none_slot->room_progress, none_slot->state));
    none_slot->room_progress.combat.attack.elapsed_ticks = 0U;
    none_slot->room_progress.combat.abyss_environment.warning = true;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        none_slot->room_progress, none_slot->state));
    none_slot->room_progress.combat.abyss_environment.warning = false;
    none_slot->room_progress.combat.player_damage_history.buckets[299U][
        static_cast<std::size_t>(checkpoint::DamageType::chaos)] = 1U;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        none_slot->room_progress, none_slot->state));

    slot->room_progress.room_seed ^= 1U;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        slot->room_progress, slot->state));
    slot->room_progress.room_seed ^= 1U;
    slot->state.death.lifecycle =
        checkpoint::DeathLifecycle::pending_continue;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        slot->room_progress, slot->state));
    slot->state.death = {};
    slot->state.current_room.is_abyss = true;
    slot->state.abyss.lifecycle = abyss::AbyssLifecycle::started;
    slot->state.abyss.rule = abyss::AbyssRuleId::swift_pursuit;
    slot->room_progress.combat.abyss_environment.rule =
        abyss::AbyssRuleId::swift_pursuit;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        slot->room_progress, slot->state));
    slot->room_progress.combat.abyss_environment.rule =
        abyss::AbyssRuleId::heavy_steps;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        slot->room_progress, slot->state));
    slot->state.current_room.is_abyss = false;
    slot->state.abyss = {};
    slot->room_progress.combat.abyss_environment.rule =
        abyss::AbyssRuleId::none;
    slot->room_progress.equipment_ground_count = 2U;
    slot->room_progress.equipment_ground[0U].ordinal = 7U;
    slot->room_progress.equipment_ground[1U].ordinal = 7U;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        slot->room_progress, slot->state));
    std::unique_ptr<checkpoint::SaveCheckpointSlot> abyss_slot{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(abyss_slot != nullptr);
    ARPG_REQUIRE(make_cleared_abyss_fixture(*abyss_slot));
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        abyss_slot->room_progress, abyss_slot->state));

    abyss_slot->state.abyss.generated_mask = 0x01U;
    abyss_slot->room_progress.equipment_ground_count = 1U;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        abyss_slot->room_progress, abyss_slot->state));
    abyss_slot->state.abyss.claimed_mask = 0x01U;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        abyss_slot->room_progress, abyss_slot->state));
    abyss_slot->room_progress.equipment_ground_count = 0U;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        abyss_slot->room_progress, abyss_slot->state));
    abyss_slot->state.abyss.abandoned_mask = 0x01U;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        abyss_slot->room_progress, abyss_slot->state));
    abyss_slot->state.abyss.abandoned_mask = 0U;
    abyss_slot->state.abyss.claimed_mask = 0U;
    abyss_slot->room_progress.equipment_ground_count = 1U;
    abyss_slot->room_progress.equipment_ground[0U].source = 0U;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        abyss_slot->room_progress, abyss_slot->state));

    std::unique_ptr<checkpoint::SaveCheckpointSlot> domain_slot{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(domain_slot != nullptr);
    ARPG_REQUIRE(make_fixture(*domain_slot));
    auto& domain_room = domain_slot->room_progress;
    domain_room.generated_monsters = 1125U;
    domain_room.required_kills = checkpoint::required_kills(1125U);
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        domain_room, domain_slot->state));
    domain_room.secondary_claim_bits[2250U / 64U] |=
        std::uint64_t{1U} << (2250U % 64U);
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        domain_room, domain_slot->state));
    domain_room.secondary_claim_bits[2250U / 64U] = 0U;
    domain_room.secondary_claim_bits[2304U / 64U] |=
        std::uint64_t{1U} << (2304U % 64U);
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        domain_room, domain_slot->state));
    domain_room.secondary_claim_bits[2304U / 64U] = 0U;
    domain_room.secondary_claim_bits[2320U / 64U] |=
        std::uint64_t{1U} << (2320U % 64U);
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        domain_room, domain_slot->state));
    domain_room.secondary_claim_bits[2320U / 64U] = 0U;
    domain_room.secondary_ground[2U].source = 0U;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        domain_room, domain_slot->state));
    domain_room.secondary_ground[2U].source = 1U;
    domain_room.secondary_ground[3U] = {};
    domain_room.secondary_ground[3U].tag =
        checkpoint::SecondaryGroundTag::material;
    domain_room.secondary_ground[3U].ordinal = 2250U;
    domain_room.secondary_ground[3U].source = 0U;
    domain_room.secondary_ground[3U].material =
        items::MaterialId::reinforcement_stone;
    domain_room.secondary_ground_count = 4U;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        domain_room, domain_slot->state));
    domain_room.secondary_ground[3U].ordinal = 2319U;
    domain_room.secondary_ground[3U].source = 2U;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        domain_room, domain_slot->state));
    domain_room.secondary_ground[3U].ordinal = 2320U;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        domain_room, domain_slot->state));
    domain_room.secondary_ground_count = 3U;
    domain_room.equipment_claim_bits[1125U / 64U] |=
        std::uint64_t{1U} << (1125U % 64U);
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        domain_room, domain_slot->state));

    std::unique_ptr<checkpoint::SaveCheckpointSlot> reserve_slot{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(reserve_slot != nullptr);
    ARPG_REQUIRE(make_fixture(*reserve_slot));
    auto& reserve_state = reserve_slot->state;
    auto& reserve_room = reserve_slot->room_progress;
    reserve_state.current_room.is_abyss = true;
    reserve_state.abyss.lifecycle = abyss::AbyssLifecycle::cleared;
    reserve_state.abyss.rule = abyss::AbyssRuleId::swift_pursuit;
    reserve_state.abyss.reward_total = 3U;
    reserve_state.abyss.generated_mask = 0x07U;
    reserve_room.generated_monsters = 1125U;
    reserve_room.defeated_monsters = 1125U;
    reserve_room.required_kills = checkpoint::required_kills(1125U);
    reserve_room.exits_unlocked = true;
    reserve_room.full_clear = true;
    reserve_room.reward_committed = true;
    reserve_room.defeat_bits = {};
    for (std::uint16_t ordinal = 0U; ordinal < 1125U; ++ordinal) {
        reserve_room.defeat_bits[ordinal / 64U] |=
            std::uint64_t{1U} << (ordinal % 64U);
        auto& ground = reserve_room.equipment_ground[ordinal];
        ground.ordinal = ordinal;
        ground.source = 0U;
        ground.reward_ordinal = 0xFFU;
        ground.item = normal_item(static_cast<std::uint64_t>(ordinal) + 1U);
    }
    reserve_room.combat.monster_count = 0U;
    for (std::uint16_t reward = 0U; reward < 3U; ++reward) {
        const std::uint16_t ordinal = static_cast<std::uint16_t>(1125U + reward);
        auto& ground = reserve_room.equipment_ground[ordinal];
        ground.ordinal = ordinal;
        ground.source = 1U;
        ground.reward_ordinal = static_cast<std::uint8_t>(reward);
        ground.item = normal_item(static_cast<std::uint64_t>(ordinal) + 1U);
    }
    reserve_room.equipment_ground_count = 1128U;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        reserve_room, reserve_state));
    reserve_room.equipment_ground_count = 1127U;
    reserve_state.abyss.claimed_mask = 0x04U;
    reserve_room.equipment_claim_bits[1151U / 64U] |=
        std::uint64_t{1U} << (1151U % 64U);
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        reserve_room, reserve_state));
    reserve_room.equipment_claim_bits[1150U / 64U] |=
        std::uint64_t{1U} << (1150U % 64U);
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        reserve_room, reserve_state));
    reserve_room.equipment_claim_bits[1150U / 64U] &=
        ~(std::uint64_t{1U} << (1150U % 64U));
    reserve_room.secondary_ground[3U] = {};
    reserve_room.secondary_ground[3U].tag =
        checkpoint::SecondaryGroundTag::material;
    reserve_room.secondary_ground[3U].ordinal = 2304U;
    reserve_room.secondary_ground[3U].source = 2U;
    reserve_room.secondary_ground[3U].material =
        items::MaterialId::reinforcement_stone;
    reserve_room.secondary_ground_count = 4U;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        reserve_room, reserve_state));
    reserve_room.secondary_claim_bits[2305U / 64U] |=
        std::uint64_t{1U} << (2305U % 64U);
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        reserve_room, reserve_state));
    return {};
}

constexpr test::TestCase kCases[] = {
    {"v9 maximum room round trip", &v9_round_trip_preserves_large_room_fields},
    {"task5 v9 secondary ordinal migration",
        &task5_v9_secondary_ordinals_migrate_to_canonical_once},
    {"unmarked task5 common ordinal uses spawn position",
        &unmarked_task5_common_ordinal_uses_spawn_position},
    {"unmarked canonical v9 common ordinal uses spawn position",
        &unmarked_canonical_v9_common_ordinal_uses_spawn_position},
    {"unmarked v9 rejects ambiguous common ordinal position",
        &unmarked_v9_rejects_ambiguous_common_ordinal_position},
    {"unmarked v9 rejects mixed secondary ordinal semantics",
        &unmarked_v9_rejects_mixed_secondary_ordinal_semantics},
    {"maximum legal v9 ground and claim payload round trip",
        &maximum_legal_v9_ground_and_claim_payloads_round_trip},
    {"high ordinal session v9 round trip and exact claims",
        &high_ordinal_session_v9_round_trip_and_claims_are_exact},
    {"v8 load starts fresh room drop authority",
        &v8_load_starts_fresh_room_drop_authority},
    {"v9 corruption", &v9_rejects_crc_and_length_corruption},
    {"v9 structural validation", &structural_validation_rejects_identity_and_order_faults},
};

constexpr test::TestCase kUnlockCases[] = {
    {"v9 partial unlock round trip restores combat",
        &v9_partial_unlock_round_trip_restores_combat},
    {"v9 started abyss early exit to normal round trip",
        &v9_started_abyss_early_exit_to_normal_round_trip},
    {"v9 started abyss early exit to abyss round trip",
        &v9_started_abyss_early_exit_to_abyss_round_trip},
    {"v8 reserved resolution lifecycle remains zero",
        &v8_reserved_resolution_lifecycle_remains_zero},
    {"v9 resolution lifecycle tail is backward compatible",
        &v9_resolution_lifecycle_tail_is_backward_compatible},
    {"v9 legacy abyss death restores and rewrites failed",
        &v9_legacy_abyss_death_restores_and_rewrites_failed},
};

}  // namespace

arpg::test::TestSuite checkpoint_v9_suite() noexcept {
    return arpg::test::make_suite("checkpoint_v9", kCases);
}

arpg::test::TestSuite checkpoint_v9_unlock_suite() noexcept {
    return arpg::test::make_suite("checkpoint_v9_unlock", kUnlockCases);
}
