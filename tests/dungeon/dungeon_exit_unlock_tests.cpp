#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_progress_checkpoint.hpp"
#include "checkpoint/room_checkpoint_schema.hpp"
#include "checkpoint/room_checkpoint_validation.hpp"
#include "items/item_generation.hpp"
#include "items/item_catalog.hpp"
#include "progression/progression_rules.hpp"
#include "abyss/abyss_rules.hpp"

#include <cstdint>
#include <memory>
#include <new>
#include <utility>

namespace {

using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::PendingSaveKind;
using arpg::dungeon::RoomPhase;
using arpg::dungeon::SaveDisposition;

DungeonSession make_session(bool has_hole = true) noexcept {
    DungeonRules rules{};
    auto initial = arpg::dungeon::make_initial_run_state(
        0x25100CULL, rules).state;
    initial.current_room.has_hole = has_hole;
    return DungeonSession{rules, std::move(initial)};
}

void enter_combat(DungeonSession& session) noexcept {
    session.tick({});
    arpg::test::EventSummary ignored{};
    arpg::test::drain_all_events(session, ignored);
    arpg::test::set_player_health(session, 1000000, 1000000);
}

bool defeat_without_authority_tick(
    DungeonSession& session, std::uint32_t count) noexcept {
    for (std::uint32_t ordinal = 0U; ordinal < count; ++ordinal) {
        if (!arpg::test::relay_defeated(session, 0U,
                static_cast<arpg::combat::MonsterOrdinal>(ordinal),
                {20.0F, 20.0F, 0.0F}, true,
                arpg::combat::MonsterId::fire_bomber,
                static_cast<std::uint16_t>(ordinal), 0U, false)) {
            return false;
        }
    }
    return true;
}

bool all_exits(const arpg::dungeon::DungeonSnapshot& snapshot,
    bool expected) noexcept {
    for (const bool open : snapshot.exits_open) {
        if (open != expected) return false;
    }
    return true;
}

bool reach_unlock_pending(DungeonSession& session) noexcept {
    enter_combat(session);
    const std::uint32_t required = arpg::dungeon::required_kills(
        session.snapshot().initial_monster_count);
    if (!defeat_without_authority_tick(session, required)) return false;
    session.tick({});
    return session.snapshot().phase == RoomPhase::committing
        && session.pending_save_view() != nullptr
        && session.pending_save_view()->kind == PendingSaveKind::room_unlock;
}

bool commit_current(DungeonSession& session) noexcept {
    const arpg::dungeon::PendingSave* const pending =
        session.pending_save_view();
    if (pending == nullptr) return false;
    const auto kind = pending->kind;
    const auto generation = pending->expected_generation;
    const auto verified = pending->next_state;
    session.resolve_pending_save({
        SaveDisposition::committed, generation, verified, kind});
    return session.snapshot().phase != RoomPhase::faulted;
}

bool same_progression(const arpg::progression::ProgressionState& left,
    const arpg::progression::ProgressionState& right) noexcept {
    return left.level == right.level
        && left.experience == right.experience
        && left.earned_passive_points == right.earned_passive_points
        && left.unspent_passive_points == right.unspent_passive_points;
}

bool contains_item(const arpg::items::ItemOwnershipState& ownership,
    std::uint64_t id) noexcept {
    for (const auto& item : ownership.items) {
        if (item.id == id) return true;
    }
    return false;
}

arpg::items::ItemInstance normal_item(std::uint64_t id) noexcept {
    const auto item = arpg::items::generate_item({
        id ^ 0xA5A5A5A5ULL,
        arpg::items::ItemSlot::helmet,
        1U,
        id,
        arpg::items::ItemRarity::normal,
    });
    return item.value_or(arpg::items::ItemInstance{});
}

bool commit_unlock(DungeonSession& session) noexcept {
    return reach_unlock_pending(session) && commit_current(session);
}

arpg::dungeon::DungeonRunState available_abyss_state() noexcept {
    DungeonRules rules{};
    auto state = arpg::dungeon::make_initial_run_state(
        0xAB155EEDULL, rules).state;
    std::uint64_t seed = 1U;
    while (!arpg::abyss::is_abyss_roll(seed)) ++seed;
    state.current_room.seed = seed;
    state.current_room.depth = 40U;
    state.current_room.entry = arpg::dungeon::EntrySide::left;
    state.current_room.ecology = arpg::dungeon::DungeonElement::water;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = true;
    state.last_transition = arpg::dungeon::TransitionKind::door;
    state.last_direction = ExitDirection::right;
    const auto selected = arpg::abyss::select_abyss_rule(seed, 40U);
    if (selected.has_value()) {
        state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::available;
        state.abyss.danger = selected->danger;
        state.abyss.rule = selected->rule;
        state.abyss.rules_version = selected->rules_version;
    }
    return state;
}

bool commit_started_abyss_unlock(DungeonSession& session) noexcept {
    if (session.pending_save_view() == nullptr
            || session.pending_save_view()->kind
                != PendingSaveKind::abyss_start
            || !commit_current(session)) {
        return false;
    }
    return commit_unlock(session);
}

arpg::test::Failure quarter_kill_threshold_rounds_up() noexcept {
    ARPG_REQUIRE(arpg::dungeon::required_kills(300U) == 75U);
    ARPG_REQUIRE(arpg::dungeon::required_kills(301U) == 76U);
    ARPG_REQUIRE(arpg::dungeon::required_kills(1125U) == 282U);
    return {};
}

arpg::test::Failure threshold_stays_closed_until_exact_commit() noexcept {
    DungeonSession session = make_session();
    enter_combat(session);
    const auto initial = session.snapshot();
    ARPG_REQUIRE(initial.phase == RoomPhase::combat);
    ARPG_REQUIRE(initial.initial_monster_count > 1U);
    const std::uint32_t required = arpg::dungeon::required_kills(
        initial.initial_monster_count);
    ARPG_REQUIRE(required > 0U);
    ARPG_REQUIRE(defeat_without_authority_tick(session, required - 1U));

    const auto before = session.snapshot();
    ARPG_REQUIRE(before.defeated_monster_count == required - 1U);
    ARPG_REQUIRE(!before.exits_unlocked);
    ARPG_REQUIRE(all_exits(before, false));
    arpg::test::attempt_exit(session, ExitDirection::right);
    ARPG_REQUIRE(!session.request_descent(true));
    ARPG_REQUIRE(!session.pending_save().has_value());

    ARPG_REQUIRE(arpg::test::relay_defeated(session, 0U,
        static_cast<arpg::combat::MonsterOrdinal>(required - 1U),
        {20.0F, 20.0F, 0.0F}, true,
        arpg::combat::MonsterId::fire_bomber,
        static_cast<std::uint16_t>(required - 1U), 0U, false));
    session.tick({});

    const auto frozen = session.snapshot();
    const arpg::dungeon::PendingSave* const pending =
        session.pending_save_view();
    ARPG_REQUIRE(frozen.phase == RoomPhase::committing);
    ARPG_REQUIRE(!frozen.exits_unlocked);
    ARPG_REQUIRE(all_exits(frozen, false));
    ARPG_REQUIRE(pending != nullptr);
    ARPG_REQUIRE(pending->kind == PendingSaveKind::room_unlock);

    std::unique_ptr<arpg::checkpoint::SaveCheckpointSlot>
        pending_checkpoint{new (std::nothrow)
            arpg::checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(pending_checkpoint != nullptr);
    ARPG_REQUIRE(session.capture_save_checkpoint(
        *pending_checkpoint, 17U, &pending->next_state));
    ARPG_REQUIRE(pending_checkpoint->room_progress.required_kills == required);
    ARPG_REQUIRE(pending_checkpoint->room_progress.exits_unlocked);
    ARPG_REQUIRE(!pending_checkpoint->room_progress.full_clear);

    session.resolve_pending_save({SaveDisposition::committed,
        pending->expected_generation, pending->next_state, pending->kind});
    const auto committed = session.snapshot();
    ARPG_REQUIRE(committed.phase == RoomPhase::combat);
    ARPG_REQUIRE(committed.exits_unlocked);
    ARPG_REQUIRE(all_exits(committed, true));
    return {};
}

arpg::test::Failure committed_unlock_keeps_combat_authority_running() noexcept {
    DungeonSession session = make_session();
    enter_combat(session);
    const std::uint32_t required = arpg::dungeon::required_kills(
        session.snapshot().initial_monster_count);
    ARPG_REQUIRE(defeat_without_authority_tick(session, required));
    session.tick({});
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == PendingSaveKind::room_unlock);
    session.resolve_pending_save({SaveDisposition::committed,
        pending->expected_generation, pending->next_state, pending->kind});

    arpg::test::EventSummary events{};
    arpg::test::drain_all_events(session, events);
    ARPG_REQUIRE(events.exits_opened_count == 1U);
    ARPG_REQUIRE(events.room_cleared_count == 0U);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::combat);
    ARPG_REQUIRE(session.queue_action(arpg::combat::Action::light));

    const std::uint64_t before_tick = session.snapshot().combat->tick;
    session.tick({});
    const auto after_tick = session.snapshot();
    ARPG_REQUIRE(after_tick.phase == RoomPhase::combat);
    ARPG_REQUIRE(after_tick.combat->tick == before_tick + 1U);

    DungeonSession skill = make_session(false);
    enter_combat(skill);
    const std::uint32_t skill_required = arpg::dungeon::required_kills(
        skill.snapshot().initial_monster_count);
    ARPG_REQUIRE(defeat_without_authority_tick(skill, skill_required));
    skill.tick({});
    const auto skill_pending = skill.pending_save();
    ARPG_REQUIRE(skill_pending.has_value());
    skill.resolve_pending_save({SaveDisposition::committed,
        skill_pending->expected_generation, skill_pending->next_state,
        skill_pending->kind});
    ARPG_REQUIRE(skill.request_active_skill_slot(0U)
        == arpg::combat::SkillCastResult::accepted);
    return {};
}

arpg::test::Failure unlock_not_committed_retries_without_duplicate_event() noexcept {
    DungeonSession session = make_session();
    ARPG_REQUIRE(reach_unlock_pending(session));
    const auto first = *session.pending_save_view();
    session.resolve_pending_save({SaveDisposition::not_committed, 0U, {},
        PendingSaveKind::room_unlock});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::combat);
    ARPG_REQUIRE(!session.snapshot().exits_unlocked);
    ARPG_REQUIRE(all_exits(session.snapshot(), false));
    arpg::test::EventSummary rolled_back{};
    arpg::test::drain_all_events(session, rolled_back);
    ARPG_REQUIRE(rolled_back.exits_opened_count == 0U);

    session.tick({});
    const auto* retry = session.pending_save_view();
    ARPG_REQUIRE(retry != nullptr);
    ARPG_REQUIRE(retry->kind == PendingSaveKind::room_unlock);
    ARPG_REQUIRE(retry->expected_generation == first.expected_generation);
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        retry->next_state, first.next_state));
    ARPG_REQUIRE(commit_current(session));
    arpg::test::EventSummary committed{};
    arpg::test::drain_all_events(session, committed);
    ARPG_REQUIRE(committed.exits_opened_count == 1U);
    session.tick({});
    arpg::test::EventSummary stable{};
    arpg::test::drain_all_events(session, stable);
    ARPG_REQUIRE(stable.exits_opened_count == 0U);
    return {};
}

arpg::test::Failure unlock_indeterminate_faults_without_publication() noexcept {
    DungeonSession session = make_session();
    ARPG_REQUIRE(reach_unlock_pending(session));
    session.resolve_pending_save({SaveDisposition::indeterminate, 0U, {},
        PendingSaveKind::room_unlock});
    const auto faulted = session.snapshot();
    ARPG_REQUIRE(faulted.phase == RoomPhase::faulted);
    ARPG_REQUIRE(faulted.diagnostics.fault
        == arpg::dungeon::DungeonFault::save_commit_indeterminate);
    ARPG_REQUIRE(!faulted.exits_unlocked);
    ARPG_REQUIRE(all_exits(faulted, false));
    arpg::test::EventSummary events{};
    arpg::test::drain_all_events(session, events);
    ARPG_REQUIRE(events.exits_opened_count == 0U);
    return {};
}

arpg::test::Failure committed_partial_unlock_reloads_as_combat() noexcept {
    DungeonSession session = make_session();
    ARPG_REQUIRE(reach_unlock_pending(session));
    ARPG_REQUIRE(commit_current(session));
    std::unique_ptr<arpg::checkpoint::SaveCheckpointSlot> saved{
        new (std::nothrow)
            arpg::checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(saved != nullptr);
    ARPG_REQUIRE(session.capture_save_checkpoint(*saved, 33U));
    ARPG_REQUIRE(saved->room_progress.exits_unlocked);
    ARPG_REQUIRE(!saved->room_progress.full_clear);

    DungeonSession reloaded{DungeonRules{}, saved->state};
    arpg::test::set_player_health(reloaded, 1000000, 1000000);
    ARPG_REQUIRE(reloaded.restore_room_progress_checkpoint(*saved));
    const auto restored = reloaded.snapshot();
    ARPG_REQUIRE(restored.phase == RoomPhase::combat);
    ARPG_REQUIRE(restored.exits_unlocked);
    ARPG_REQUIRE(all_exits(restored, true));
    ARPG_REQUIRE(restored.remaining_targets > 0U);
    return {};
}

arpg::test::Failure neutral_checkpoint_slot_captures_and_restores_normal_room()
    noexcept {
    static_assert(arpg::dungeon::checkpoint_material_ordinal(0U)
        == arpg::checkpoint::checkpoint_material_ordinal(0U));
    static_assert(arpg::dungeon::checkpoint_material_ordinal(383U)
        == arpg::checkpoint::checkpoint_material_ordinal(383U));
    static_assert(arpg::dungeon::checkpoint_material_ordinal(384U)
        == arpg::checkpoint::checkpoint_material_ordinal(384U));
    static_assert(arpg::dungeon::health_potion_claim_ordinal(0U)
        == arpg::checkpoint::health_potion_claim_ordinal(0U));
    static_assert(arpg::dungeon::health_potion_claim_ordinal(191U)
        == arpg::checkpoint::health_potion_claim_ordinal(191U));
    static_assert(arpg::dungeon::kAbyssMaterialOrdinalBegin
        == arpg::checkpoint::kAbyssMaterialOrdinalBegin);
    static_assert(arpg::dungeon::kCheckpointOrdinarySecondaryOrdinalEnd
        == arpg::checkpoint::kOrdinarySecondaryOrdinalEnd);
    static_assert(arpg::dungeon::kCheckpointAbyssSecondaryOrdinalBegin
        == arpg::checkpoint::kAbyssSecondaryOrdinalBegin);
    static_assert(arpg::dungeon::kGroundHealthPotionCapacity
        == arpg::checkpoint::kHealthPotionGroundCapacity);
    for (std::uint16_t ordinal = 0U;
            ordinal < arpg::dungeon::kGroundMaterialCapacity; ++ordinal) {
        const std::uint16_t packed =
            arpg::dungeon::checkpoint_material_ordinal(ordinal);
        ARPG_REQUIRE(packed
            == arpg::checkpoint::checkpoint_material_ordinal(ordinal));
        ARPG_REQUIRE(arpg::dungeon::material_ordinal_from_checkpoint(packed)
            == arpg::checkpoint::material_ordinal_from_checkpoint(packed));
    }
    for (std::uint16_t spawn = 0U;
            spawn < arpg::dungeon::kGroundHealthPotionCapacity; ++spawn) {
        ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(spawn)
            == arpg::checkpoint::health_potion_claim_ordinal(spawn));
    }

    DungeonSession session = make_session();
    enter_combat(session);
    const auto player = session.snapshot().combat->player.position;
    constexpr std::uint64_t kItemId = 0xC0FFEEU;
    constexpr std::uint16_t kItemOrdinal = 5U;
    constexpr std::uint16_t kMaterialOrdinal = 7U;
    constexpr std::uint16_t kPotionSpawn = 8U;
    const auto item = normal_item(kItemId);
    ARPG_REQUIRE(arpg::items::validate_item(item));
    constexpr std::uint64_t kOwnedItemId = 0xBADC0DEU;
    const auto owned_item = normal_item(kOwnedItemId);
    ARPG_REQUIRE(arpg::items::validate_item(owned_item));
    arpg::test::install_ground_item(session, 0U, owned_item, player);
    ARPG_REQUIRE(session.request_pickup(0U)
        == arpg::dungeon::RequestResult::accepted);
    ARPG_REQUIRE(commit_current(session));
    ARPG_REQUIRE(contains_item(session.item_state(), kOwnedItemId));
    arpg::test::install_ground_item(session, kItemOrdinal, item,
        {player.x + 3.0F, player.y, player.z});
    arpg::test::install_ground_material(session, kMaterialOrdinal,
        arpg::items::MaterialId::reinforcement_stone,
        {player.x + 4.0F, player.y, player.z});
    arpg::test::install_ground_health_potion(session, kPotionSpawn,
        {player.x + 5.0F, player.y, player.z});
    std::unique_ptr<arpg::checkpoint::SaveCheckpointSlot> saved{
        new (std::nothrow) arpg::checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(saved != nullptr);
    saved->state.item_ownership.items.reserve(
        session.item_state().items.size());
    ARPG_REQUIRE(session.capture_save_checkpoint(*saved, 61U));
    ARPG_REQUIRE(saved->persistence_revision == 61U);
    ARPG_REQUIRE(saved->room_progress.lifecycle
        == arpg::checkpoint::RoomProgressLifecycle::active);
    ARPG_REQUIRE(saved->room_progress.equipment_ground_count == 1U);
    ARPG_REQUIRE(saved->room_progress.equipment_ground[0U].ordinal
        == kItemOrdinal);
    ARPG_REQUIRE(saved->room_progress.equipment_ground[0U].item.id
        == kItemId);
    ARPG_REQUIRE(saved->room_progress.secondary_ground_count == 2U);
    ARPG_REQUIRE(saved->room_progress.secondary_ground[0U].tag
        == arpg::checkpoint::SecondaryGroundTag::material);
    ARPG_REQUIRE(saved->room_progress.secondary_ground[0U].ordinal
        == arpg::checkpoint::checkpoint_material_ordinal(kMaterialOrdinal));
    ARPG_REQUIRE(saved->room_progress.secondary_ground[1U].tag
        == arpg::checkpoint::SecondaryGroundTag::health_potion);
    ARPG_REQUIRE(saved->room_progress.secondary_ground[1U].ordinal
        == arpg::checkpoint::health_potion_claim_ordinal(kPotionSpawn));

    arpg::test::DungeonSessionTestAccess::seed_checkpoint_unowned_runtime_state(
        session);
    const auto live_before_restore = session.snapshot();
    ARPG_REQUIRE(session.restore_room_progress_checkpoint(*saved));
    const auto live_after_restore = session.snapshot();
    ARPG_REQUIRE(live_after_restore.session_tick
        == live_before_restore.session_tick);
    ARPG_REQUIRE(live_after_restore.diagnostics.rejected_exit_count
        == live_before_restore.diagnostics.rejected_exit_count);
    ARPG_REQUIRE(live_after_restore.last_exit == live_before_restore.last_exit);
    ARPG_REQUIRE(live_after_restore.pending_room_experience
        == live_before_restore.pending_room_experience);

    DungeonSession restored{DungeonRules{}, saved->state};
    arpg::test::set_player_health(restored, 1000000, 1000000);
    ARPG_REQUIRE(restored.restore_room_progress_checkpoint(*saved));
    ARPG_REQUIRE(arpg::test::room_monster_field_uses_combat_pool(restored));
    const auto restored_tick = restored.snapshot().combat->tick;
    restored.tick({});
    ARPG_REQUIRE(restored.snapshot().combat->tick == restored_tick + 1U);
    ARPG_REQUIRE(arpg::test::defeat_room_monster_by_ordinal(restored, 0U)
        .has_value());
    const auto snapshot = restored.snapshot();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::combat);
    ARPG_REQUIRE(snapshot.ground_item_count == 1U);
    ARPG_REQUIRE(snapshot.ground_items[0U].ordinal == kItemOrdinal);
    ARPG_REQUIRE(snapshot.ground_items[0U].item_id == kItemId);
    ARPG_REQUIRE(snapshot.ground_material_count == 1U);
    ARPG_REQUIRE(snapshot.ground_materials[0U].ordinal == kMaterialOrdinal);
    ARPG_REQUIRE(snapshot.ground_materials[0U].material
        == arpg::items::MaterialId::reinforcement_stone);
    ARPG_REQUIRE(snapshot.ground_health_potion_count == 1U);
    ARPG_REQUIRE(snapshot.ground_health_potions[0U].spawn_ordinal
        == kPotionSpawn);
    ARPG_REQUIRE(snapshot.ground_health_potions[0U].claim_ordinal
        == arpg::checkpoint::health_potion_claim_ordinal(kPotionSpawn));

    DungeonSession rejected{DungeonRules{}, saved->state};
    arpg::test::set_player_health(rejected, 1000000, 1000000);
    const auto before_rejection = rejected.snapshot();
    const auto* const before_world =
        arpg::test::DungeonSessionTestAccess::combat_world_address(rejected);
    ARPG_REQUIRE(before_world != nullptr);
    ARPG_REQUIRE(before_world->room_monster_field() != nullptr);
    const std::uint32_t defeated_before =
        before_world->room_monster_field()->defeated_count();
    auto& invalid_room = saved->room_progress;
    std::uint32_t defeated_ordinal = invalid_room.generated_monsters;
    for (std::uint32_t candidate = invalid_room.generated_monsters;
            candidate > 0U;) {
        --candidate;
        bool checkpointed = false;
        for (std::uint16_t index = 0U;
                index < invalid_room.combat.monster_count; ++index) {
            checkpointed = checkpointed
                || invalid_room.combat.monsters[index].ordinal == candidate;
        }
        if (!checkpointed) {
            defeated_ordinal = candidate;
            break;
        }
    }
    ARPG_REQUIRE(defeated_ordinal < invalid_room.generated_monsters);
    invalid_room.defeat_bits[defeated_ordinal / 64U] |=
        std::uint64_t{1U} << (defeated_ordinal % 64U);
    ++invalid_room.defeated_monsters;
    ++invalid_room.combat.player.max_hp;
    ARPG_REQUIRE(arpg::checkpoint::valid_room_progress_checkpoint_structural(
        invalid_room, saved->state));
    ARPG_REQUIRE(!rejected.restore_room_progress_checkpoint(*saved));
    const auto after_rejection = rejected.snapshot();
    ARPG_REQUIRE(after_rejection.phase == before_rejection.phase);
    ARPG_REQUIRE(after_rejection.remaining_targets
        == before_rejection.remaining_targets);
    ARPG_REQUIRE(after_rejection.combat.has_value());
    ARPG_REQUIRE(before_rejection.combat.has_value());
    ARPG_REQUIRE(after_rejection.combat->tick
        == before_rejection.combat->tick);
    ARPG_REQUIRE(after_rejection.combat->player.max_hp
        == before_rejection.combat->player.max_hp);
    const auto* const after_world =
        arpg::test::DungeonSessionTestAccess::combat_world_address(rejected);
    ARPG_REQUIRE(after_world != nullptr);
    ARPG_REQUIRE(after_world->room_monster_field() != nullptr);
    ARPG_REQUIRE(after_world->room_monster_field()->defeated_count()
        == defeated_before);
    ARPG_REQUIRE(arpg::test::DungeonSessionTestAccess::room_progress(rejected)
            .defeated_monster_count == 0U);
    return {};
}

arpg::test::Failure same_tick_full_defeat_commits_unlock_before_clear() noexcept {
    DungeonSession session = make_session();
    enter_combat(session);
    const std::uint32_t initial = session.snapshot().initial_monster_count;
    ARPG_REQUIRE(defeat_without_authority_tick(session, initial));
    session.tick({});
    ARPG_REQUIRE(session.pending_save_view() != nullptr);
    ARPG_REQUIRE(session.pending_save_view()->kind
        == PendingSaveKind::room_unlock);
    ARPG_REQUIRE(commit_current(session));
    arpg::test::EventSummary unlock_events{};
    arpg::test::drain_all_events(session, unlock_events);
    ARPG_REQUIRE(unlock_events.exits_opened_count == 1U);
    ARPG_REQUIRE(unlock_events.room_cleared_count == 0U);

    session.tick({});
    ARPG_REQUIRE(session.pending_save_view() != nullptr);
    ARPG_REQUIRE(session.pending_save_view()->kind
        == PendingSaveKind::room_clear);
    ARPG_REQUIRE(commit_current(session));
    arpg::test::EventSummary clear_events{};
    arpg::test::drain_all_events(session, clear_events);
    ARPG_REQUIRE(clear_events.exits_opened_count == 0U);
    ARPG_REQUIRE(clear_events.room_cleared_count == 1U);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::cleared);
    ARPG_REQUIRE(session.snapshot().exits_unlocked);

    std::unique_ptr<arpg::checkpoint::SaveCheckpointSlot> cleared{
        new (std::nothrow) arpg::checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(cleared != nullptr);
    ARPG_REQUIRE(session.capture_save_checkpoint(*cleared, 62U));
    ARPG_REQUIRE(cleared->room_progress.lifecycle
        == arpg::checkpoint::RoomProgressLifecycle::active);
    ARPG_REQUIRE(cleared->room_progress.full_clear);
    ARPG_REQUIRE(cleared->room_progress.reward_committed);
    DungeonSession restored{DungeonRules{}, cleared->state};
    arpg::test::set_player_health(restored, 1000000, 1000000);
    ARPG_REQUIRE(restored.restore_room_progress_checkpoint(*cleared));
    ARPG_REQUIRE(restored.snapshot().phase == RoomPhase::awaiting_exit);
    ARPG_REQUIRE(restored.snapshot().exits_unlocked);

    const std::uint64_t reward = session.snapshot().last_room_experience;
    session.tick({});
    session.tick({});
    arpg::test::EventSummary repeated{};
    arpg::test::drain_all_events(session, repeated);
    ARPG_REQUIRE(repeated.exits_opened_count == 0U);
    ARPG_REQUIRE(repeated.room_cleared_count == 0U);
    ARPG_REQUIRE(session.snapshot().last_room_experience == reward);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::awaiting_exit);
    return {};
}

arpg::test::Failure room_clear_rejects_live_target_diagnostic_shortcut() noexcept {
    DungeonSession session = make_session();
    enter_combat(session);
    ARPG_REQUIRE(session.snapshot().remaining_targets > 0U);
    arpg::test::prepare_room_clear_with_live_targets(session);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().remaining_targets > 0U);
    arpg::test::EventSummary events{};
    arpg::test::drain_all_events(session, events);
    ARPG_REQUIRE(events.room_cleared_count == 0U);
    return {};
}

arpg::test::Failure normal_early_exit_commits_monster_xp_and_claimed_loot_only() noexcept {
    DungeonSession session = make_session();
    ARPG_REQUIRE(commit_unlock(session));
    arpg::test::EventSummary ignored{};
    arpg::test::drain_all_events(session, ignored);
    const auto before = session.snapshot();
    ARPG_REQUIRE(before.pending_room_experience > 0U);

    constexpr std::uint64_t kClaimedItemId = 0xE4117U;
    const auto item = normal_item(kClaimedItemId);
    ARPG_REQUIRE(arpg::items::validate_item(item));
    const auto player = session.snapshot().combat->player.position;
    arpg::test::install_ground_item(session, 0U, item, player);
    ARPG_REQUIRE(session.request_pickup(0U)
        == arpg::dungeon::RequestResult::accepted);
    ARPG_REQUIRE(commit_current(session));
    ARPG_REQUIRE(contains_item(session.item_state(), kClaimedItemId));

    constexpr std::uint16_t kUnpickedMaterialOrdinal = 191U;
    arpg::test::install_ground_material(session, kUnpickedMaterialOrdinal,
        arpg::items::MaterialId::reinforcement_stone, player);
    const auto materials_before = session.item_state().materials;
    const auto progression_before = session.snapshot().progression;
    const std::uint64_t earned = session.snapshot().pending_room_experience;
    const auto expected = arpg::progression::apply_experience(
        progression_before, earned,
        arpg::progression::default_progression_rules());

    arpg::test::attempt_exit(session, ExitDirection::right);
    const auto* pending = session.pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    ARPG_REQUIRE(pending->kind == PendingSaveKind::transition);
    ARPG_REQUIRE(same_progression(
        pending->next_state.progression, expected.state));
    ARPG_REQUIRE(contains_item(
        pending->next_state.item_ownership, kClaimedItemId));
    ARPG_REQUIRE(pending->next_state.item_ownership.materials
        == materials_before);
    std::unique_ptr<arpg::checkpoint::SaveCheckpointSlot> leaving{
        new (std::nothrow)
            arpg::checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(leaving != nullptr);
    leaving->state.item_ownership.items.reserve(
        pending->next_state.item_ownership.items.size());
    ARPG_REQUIRE(session.capture_save_checkpoint(
        *leaving, 41U, &pending->next_state));
    ARPG_REQUIRE(leaving->room_progress.lifecycle
        == arpg::checkpoint::RoomProgressLifecycle::none);

    ARPG_REQUIRE(commit_current(session));
    const auto committed = session.snapshot();
    ARPG_REQUIRE(committed.phase == RoomPhase::transitioning);
    ARPG_REQUIRE(!committed.combat.has_value());
    ARPG_REQUIRE(committed.ground_item_count == 0U);
    ARPG_REQUIRE(committed.ground_material_count == 0U);
    ARPG_REQUIRE(committed.pending_room_experience == 0U);
    ARPG_REQUIRE(committed.last_room_experience == earned);
    ARPG_REQUIRE(same_progression(committed.progression, expected.state));
    ARPG_REQUIRE(contains_item(session.item_state(), kClaimedItemId));
    ARPG_REQUIRE(session.item_state().materials == materials_before);
    return {};
}

arpg::test::Failure normal_early_exit_not_committed_keeps_original_room() noexcept {
    DungeonSession session = make_session();
    ARPG_REQUIRE(commit_unlock(session));
    const auto before = session.snapshot();
    arpg::test::attempt_exit(session, ExitDirection::left);
    ARPG_REQUIRE(session.pending_save_view() != nullptr);
    session.resolve_pending_save({SaveDisposition::not_committed, 0U, {},
        PendingSaveKind::transition});
    const auto rolled_back = session.snapshot();
    ARPG_REQUIRE(rolled_back.phase == RoomPhase::combat);
    ARPG_REQUIRE(rolled_back.room_index == before.room_index);
    ARPG_REQUIRE(rolled_back.exits_unlocked);
    ARPG_REQUIRE(rolled_back.combat.has_value());
    ARPG_REQUIRE(rolled_back.pending_room_experience
        == before.pending_room_experience);
    arpg::test::attempt_exit(session, ExitDirection::left);
    ARPG_REQUIRE(session.pending_save_view() != nullptr);
    ARPG_REQUIRE(session.pending_save_view()->kind
        == PendingSaveKind::transition);
    return {};
}

arpg::test::Failure started_abyss_early_exit_commits_failed_resolution() noexcept {
    DungeonSession session{DungeonRules{}, available_abyss_state()};
    ARPG_REQUIRE(commit_started_abyss_unlock(session));
    const auto origin = arpg::test::stable_state(session);
    const auto before = session.snapshot();
    const auto expected_progression = arpg::progression::apply_experience(
        before.progression, before.pending_room_experience,
        arpg::progression::default_progression_rules());

    arpg::test::attempt_exit(session, ExitDirection::right);
    const auto* pending = session.pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    ARPG_REQUIRE(pending->kind == PendingSaveKind::abyss_early_exit);
    ARPG_REQUIRE(pending->resume_phase == RoomPhase::combat);
    ARPG_REQUIRE(pending->next_state.current_room.index
        == origin.current_room.index + 1U);
    const bool next_is_abyss = arpg::dungeon::preview_abyss_doors(
        origin.current_room)[static_cast<std::size_t>(ExitDirection::right)];
    ARPG_REQUIRE(pending->next_state.current_room.is_abyss == next_is_abyss);
    ARPG_REQUIRE(pending->next_state.abyss.lifecycle
        == (next_is_abyss ? arpg::abyss::AbyssLifecycle::available
                          : arpg::abyss::AbyssLifecycle::none));
    ARPG_REQUIRE(pending->next_state.abyss.reward_total == 0U);
    ARPG_REQUIRE(pending->next_state.last_abyss_resolution.valid);
    ARPG_REQUIRE(pending->next_state.last_abyss_resolution.lifecycle
        == arpg::abyss::AbyssLifecycle::failed);
    ARPG_REQUIRE(pending->next_state.last_abyss_resolution.room_seed
        == origin.current_room.seed);
    ARPG_REQUIRE(pending->next_state.last_abyss_resolution.generated == 0U);
    ARPG_REQUIRE(pending->next_state.last_abyss_resolution.claimed == 0U);
    ARPG_REQUIRE(pending->next_state.last_abyss_resolution.abandoned
        == pending->next_state.last_abyss_resolution.total);
    ARPG_REQUIRE(same_progression(pending->next_state.progression,
        expected_progression.state));

    ARPG_REQUIRE(commit_current(session));
    const auto& committed = arpg::test::stable_state(session);
    ARPG_REQUIRE(committed.abyss.lifecycle
        == (next_is_abyss ? arpg::abyss::AbyssLifecycle::available
                          : arpg::abyss::AbyssLifecycle::none));
    ARPG_REQUIRE(committed.last_abyss_resolution.valid);
    ARPG_REQUIRE(committed.last_abyss_resolution.lifecycle
        == arpg::abyss::AbyssLifecycle::failed);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::transitioning);
    ARPG_REQUIRE(session.snapshot().pending_room_experience == 0U);
    return {};
}

arpg::test::Failure started_abyss_early_exit_rollback_and_indeterminate() noexcept {
    DungeonSession rollback{DungeonRules{}, available_abyss_state()};
    ARPG_REQUIRE(commit_started_abyss_unlock(rollback));
    const auto origin = arpg::test::stable_state(rollback).current_room;
    arpg::test::attempt_exit(rollback, ExitDirection::up);
    ARPG_REQUIRE(rollback.pending_save_view() != nullptr);
    rollback.resolve_pending_save({SaveDisposition::not_committed, 0U, {},
        PendingSaveKind::abyss_early_exit});
    ARPG_REQUIRE(rollback.snapshot().phase == RoomPhase::combat);
    ARPG_REQUIRE(rollback.snapshot().room_index == origin.index);
    ARPG_REQUIRE(rollback.snapshot().room_seed == origin.seed);
    ARPG_REQUIRE(rollback.snapshot().exits_unlocked);
    ARPG_REQUIRE(arpg::test::stable_state(rollback).abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::started);

    DungeonSession indeterminate{DungeonRules{}, available_abyss_state()};
    ARPG_REQUIRE(commit_started_abyss_unlock(indeterminate));
    arpg::test::attempt_exit(indeterminate, ExitDirection::down);
    ARPG_REQUIRE(indeterminate.pending_save_view() != nullptr);
    indeterminate.resolve_pending_save({SaveDisposition::indeterminate,
        0U, {}, PendingSaveKind::abyss_early_exit});
    ARPG_REQUIRE(indeterminate.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(indeterminate.snapshot().diagnostics.fault
        == arpg::dungeon::DungeonFault::save_commit_indeterminate);
    return {};
}

arpg::test::Failure cleared_abyss_rebuild_restores_committed_clear_flags_without_events() noexcept {
    DungeonRules rules{};
    DungeonSession live{rules, available_abyss_state()};
    ARPG_REQUIRE(live.pending_save_view() != nullptr);
    ARPG_REQUIRE(live.pending_save_view()->kind
        == PendingSaveKind::abyss_start);
    ARPG_REQUIRE(commit_current(live));
    arpg::test::EventSummary live_events{};
    ARPG_REQUIRE(arpg::test::drive_until_cleared(live, live_events));
    ARPG_REQUIRE(arpg::test::stable_state(live).abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::cleared);

    std::unique_ptr<arpg::checkpoint::SaveCheckpointSlot> saved{
        new (std::nothrow) arpg::checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(saved != nullptr);
    ARPG_REQUIRE(live.capture_save_checkpoint(*saved, 63U));
    ARPG_REQUIRE(saved->state.current_room.is_abyss);
    ARPG_REQUIRE(saved->state.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::cleared);
    ARPG_REQUIRE(saved->room_progress.full_clear);

    DungeonSession rebuilt{rules, saved->state};
    ARPG_REQUIRE(rebuilt.restore_room_progress_checkpoint(*saved));
    const auto snapshot = rebuilt.snapshot();
    const auto& progress =
        arpg::test::DungeonSessionTestAccess::room_progress(rebuilt);
    ARPG_REQUIRE(snapshot.phase == RoomPhase::awaiting_exit);
    ARPG_REQUIRE(snapshot.exits_unlocked);
    ARPG_REQUIRE(all_exits(snapshot, true));
    ARPG_REQUIRE(progress.exits_unlocked);
    ARPG_REQUIRE(progress.full_clear);
    ARPG_REQUIRE(!rebuilt.try_pop_event().has_value());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"quarter kill threshold rounds up", &quarter_kill_threshold_rounds_up},
    {"threshold stays closed until exact commit",
        &threshold_stays_closed_until_exact_commit},
    {"committed unlock keeps combat authority running",
        &committed_unlock_keeps_combat_authority_running},
    {"unlock not committed retries without duplicate event",
        &unlock_not_committed_retries_without_duplicate_event},
    {"unlock indeterminate faults without publication",
        &unlock_indeterminate_faults_without_publication},
    {"committed partial unlock reloads as combat",
        &committed_partial_unlock_reloads_as_combat},
    {"neutral checkpoint slot captures and restores normal room",
        &neutral_checkpoint_slot_captures_and_restores_normal_room},
    {"same tick full defeat commits unlock before clear",
        &same_tick_full_defeat_commits_unlock_before_clear},
    {"room clear rejects live target diagnostic shortcut",
        &room_clear_rejects_live_target_diagnostic_shortcut},
    {"normal early exit commits monster xp and claimed loot only",
        &normal_early_exit_commits_monster_xp_and_claimed_loot_only},
    {"normal early exit rollback keeps original room",
        &normal_early_exit_not_committed_keeps_original_room},
    {"started abyss early exit commits failed resolution",
        &started_abyss_early_exit_commits_failed_resolution},
    {"started abyss early exit rollback and indeterminate",
        &started_abyss_early_exit_rollback_and_indeterminate},
    {"cleared abyss rebuild restores committed clear flags without events",
        &cleared_abyss_rebuild_restores_committed_clear_flags_without_events},
};

}  // namespace

arpg::test::TestSuite dungeon_exit_unlock_suite() noexcept {
    return arpg::test::make_suite("dungeon_exit_unlock", kCases);
}
