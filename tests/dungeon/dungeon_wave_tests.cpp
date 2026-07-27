#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "dungeon_test_support.hpp"

#include "abyss/abyss_rules.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_monster_plan_builder.hpp"

#include <cstddef>
#include <cstdint>

namespace {

using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::RoomPhase;

DungeonRunState state_for_seed(std::uint64_t seed, const DungeonRules& rules) noexcept {
    return arpg::dungeon::make_initial_run_state(seed, rules).state;
}

DungeonRunState empty_inventory_abyss_available_state() noexcept {
    DungeonRunState state = state_for_seed(
        0xAB155EEDULL, DungeonRules{});
    std::uint64_t seed = 1U;
    while (!arpg::abyss::is_abyss_roll(seed)) ++seed;
    state.current_room.seed = seed;
    state.current_room.depth = 40U;
    state.current_room.entry = arpg::dungeon::EntrySide::left;
    state.current_room.ecology = arpg::dungeon::DungeonElement::chaos;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = true;
    state.last_transition = arpg::dungeon::TransitionKind::door;
    state.last_direction = arpg::dungeon::ExitDirection::right;
    const auto selection = arpg::abyss::select_abyss_rule(seed, 40U);
    if (selection.has_value()) {
        state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::available;
        state.abyss.danger = selection->danger;
        state.abyss.rule = selection->rule;
        state.abyss.rules_version = selection->rules_version;
    }
    state.item_ownership.items.clear();
    state.item_ownership.items.shrink_to_fit();
    return state;
}

bool staged_progress_empty(
    const arpg::dungeon::RoomProgressState& progress) noexcept {
    if (progress.initial_monster_count != 0U
            || progress.defeated_monster_count != 0U) {
        return false;
    }
    for (const std::uint64_t word : progress.defeated_monster_bits) {
        if (word != 0U) return false;
    }
    return true;
}

bool same_item_instance(
    const arpg::items::ItemInstance& left,
    const arpg::items::ItemInstance& right) noexcept {
    if (left.id != right.id || left.base_id != right.base_id
            || left.rarity != right.rarity
            || left.item_level != right.item_level
            || left.required_level != right.required_level
            || left.affix_count != right.affix_count
            || left.reserved != right.reserved
            || left.reinforcement != right.reinforcement
            || left.extension_reserved != right.extension_reserved) {
        return false;
    }
    for (std::size_t index = 0U; index < left.affixes.size(); ++index) {
        if (left.affixes[index].affix_id != right.affixes[index].affix_id
                || left.affixes[index].tier != right.affixes[index].tier
                || left.affixes[index].variant
                    != right.affixes[index].variant
                || left.affixes[index].value_roll_bp
                    != right.affixes[index].value_roll_bp) {
            return false;
        }
    }
    return true;
}

bool same_position(
    arpg::combat::Vec3 left, arpg::combat::Vec3 right) noexcept {
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

struct GroundSentinels final {
    arpg::dungeon::GroundItem item{};
    arpg::dungeon::GroundMaterial material{};
    arpg::dungeon::GroundHealthPotion potion{};
};

GroundSentinels install_ground_sentinels(
    DungeonSession& session) noexcept {
    arpg::items::ItemInstance item{};
    item.id = 0x51A7E11U;
    item.base_id = 7U;
    item.rarity = arpg::items::ItemRarity::rare;
    item.item_level = 42U;
    item.required_level = 21U;
    item.affixes[0] = {11U, 2U, 3U, 444U};
    item.affixes[1] = {17U, 4U, 5U, 777U};
    item.affix_count = 2U;
    item.reserved = {1U, 2U, 3U};
    item.reinforcement = 9U;
    item.extension_reserved = {8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U};
    arpg::test::install_ground_item(
        session, 0U, item, {11.0F, 22.0F, 33.0F});
    arpg::test::install_ground_material(
        session, 0U, arpg::items::MaterialId::coupon_9,
        {44.0F, 55.0F, 66.0F},
        arpg::dungeon::GroundMaterialSource::monster_coupon);
    arpg::test::install_ground_health_potion(
        session, 0U, {77.0F, 88.0F, 99.0F});
    return {
        arpg::test::DungeonSessionTestAccess::ground_items(session)[0],
        arpg::test::DungeonSessionTestAccess::ground_materials(session)[0],
        arpg::test::DungeonSessionTestAccess::ground_health_potions(session)[0],
    };
}

bool same_ground_sentinels(
    const DungeonSession& session,
    const GroundSentinels& before) noexcept {
    const auto& item =
        arpg::test::DungeonSessionTestAccess::ground_items(session)[0];
    const auto& material =
        arpg::test::DungeonSessionTestAccess::ground_materials(session)[0];
    const auto& potion =
        arpg::test::DungeonSessionTestAccess::ground_health_potions(session)[0];
    return item.active == before.item.active
        && item.drop_ordinal == before.item.drop_ordinal
        && item.source == before.item.source
        && item.abyss_reward_ordinal == before.item.abyss_reward_ordinal
        && same_position(item.position, before.item.position)
        && same_item_instance(item.item, before.item.item)
        && material.active == before.material.active
        && material.ordinal == before.material.ordinal
        && material.source == before.material.source
        && same_position(material.position, before.material.position)
        && material.material == before.material.material
        && potion.active == before.potion.active
        && potion.spawn_ordinal == before.potion.spawn_ordinal
        && potion.claim_ordinal == before.potion.claim_ordinal
        && same_position(potion.position, before.potion.position);
}

bool same_environment_record(
    const arpg::combat::RoomEnvironmentRecord& left,
    const arpg::combat::RoomEnvironmentRecord& right) noexcept {
    return left.ordinal == right.ordinal
        && left.home_cell == right.home_cell
        && left.prop == right.prop
        && left.anchor.x == right.anchor.x
        && left.anchor.y == right.anchor.y
        && left.anchor.z == right.anchor.z
        && left.scale_bp == right.scale_bp
        && left.quarter_turns == right.quarter_turns
        && left.mirror_x == right.mirror_x
        && left.obstacle.kind == right.obstacle.kind
        && left.obstacle.max_hp == right.obstacle.max_hp
        && left.obstacle.bounds.minimum.x
            == right.obstacle.bounds.minimum.x
        && left.obstacle.bounds.minimum.y
            == right.obstacle.bounds.minimum.y
        && left.obstacle.bounds.minimum.z
            == right.obstacle.bounds.minimum.z
        && left.obstacle.bounds.maximum.x
            == right.obstacle.bounds.maximum.x
        && left.obstacle.bounds.maximum.y
            == right.obstacle.bounds.maximum.y
        && left.obstacle.bounds.maximum.z
            == right.obstacle.bounds.maximum.z;
}

bool same_environment_blueprint(
    const arpg::combat::RoomEnvironmentBlueprint& left,
    const arpg::combat::RoomEnvironmentBlueprint& right) noexcept {
    if (left.record_count != right.record_count
            || left.obstacle_count != right.obstacle_count
            || left.generator_version != right.generator_version
            || left.blueprint_hash != right.blueprint_hash
            || left.cell_offsets != right.cell_offsets
            || left.cell_counts != right.cell_counts) {
        return false;
    }
    for (std::uint16_t index = 0U;
         index < left.record_count; ++index) {
        if (!same_environment_record(
                left.records[index], right.records[index])) {
            return false;
        }
    }
    return true;
}

arpg::test::Failure construction_builds_one_complete_population() noexcept {
    const DungeonRules rules{};
    DungeonSession session{rules, state_for_seed(0x2A11CEU, rules)};
    const auto snapshot = session.snapshot();
    const auto* world = arpg::test::combat_world_address(session);
    ARPG_REQUIRE(world != nullptr);
    ARPG_REQUIRE(world->room_monster_field() != nullptr);
    ARPG_REQUIRE(snapshot.initial_monster_count >= 300U);
    ARPG_REQUIRE(snapshot.initial_monster_count <= 750U);
    ARPG_REQUIRE(snapshot.defeated_monster_count == 0U);
    ARPG_REQUIRE(snapshot.remaining_targets == snapshot.initial_monster_count);
    ARPG_REQUIRE(snapshot.combat.has_value());
    ARPG_REQUIRE(snapshot.combat->total_living_monsters
        == snapshot.initial_monster_count);
    ARPG_REQUIRE(snapshot.combat->monster_count
        < snapshot.initial_monster_count);
    ARPG_REQUIRE(world->room_monster_field()->total_count()
        == snapshot.initial_monster_count);
    ARPG_REQUIRE(world->room_monster_field()->living_count()
        == snapshot.initial_monster_count);
    ARPG_REQUIRE(snapshot.monster_generator_version != 0U);
    ARPG_REQUIRE(snapshot.monster_blueprint_hash != 0U);
    ARPG_REQUIRE(snapshot.environment_generator_version != 0U);
    ARPG_REQUIRE(snapshot.environment_blueprint_hash != 0U);
    ARPG_REQUIRE(snapshot.wave_index == 0U);
    ARPG_REQUIRE(snapshot.wave_count == 1U);
    ARPG_REQUIRE(snapshot.wave_delay_ticks == 0U);
    const auto generated = session.try_pop_event();
    ARPG_REQUIRE(generated.has_value());
    ARPG_REQUIRE(generated->kind
        == arpg::dungeon::DungeonEventKind::population_generated);
    ARPG_REQUIRE(!session.try_pop_event().has_value());
    session.tick({});
    const auto entered = session.try_pop_event();
    const auto started = session.try_pop_event();
    ARPG_REQUIRE(entered.has_value());
    ARPG_REQUIRE(started.has_value());
    ARPG_REQUIRE(entered->kind
        == arpg::dungeon::DungeonEventKind::room_entered);
    ARPG_REQUIRE(started->kind
        == arpg::dungeon::DungeonEventKind::combat_started);
    ARPG_REQUIRE(!session.try_pop_event().has_value());
    return {};
}

arpg::test::Failure abyss_staging_and_commit_allocate_three_owners() noexcept {
    using namespace arpg;
    DungeonRules rules{};
    DungeonRunState state = empty_inventory_abyss_available_state();
    ARPG_REQUIRE(state.item_ownership.items.empty());
    DungeonSession session{rules, state_for_seed(0xAB155EEDULL, rules)};
    test::DungeonSessionTestAccess::configure_current_room_construction(
        session, std::move(state));
    test::DungeonSessionTestAccess::construct_current_room(session);

    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::committing);
    ARPG_REQUIRE(!session.snapshot().combat.has_value());
    ARPG_REQUIRE(test::room_environment_blueprint(session) == nullptr);
    ARPG_REQUIRE(test::staged_room_monster_field(session) != nullptr);
    ARPG_REQUIRE(test::staged_room_environment_blueprint(session)
        != nullptr);
    ARPG_REQUIRE(test::staged_room_progress(session)
        .initial_monster_count > 0U);

    test::DungeonSessionTestAccess::discard_staged_room_population(session);
    const std::uint64_t before = test::allocation_count();
    const bool staged =
        test::DungeonSessionTestAccess::stage_pending_abyss_population(session);
    const std::uint64_t after_staging = test::allocation_count();
    ARPG_REQUIRE(staged);
    ARPG_REQUIRE(after_staging - before == 2U);

    const dungeon::PendingSave* const pending = session.pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    ARPG_REQUIRE(pending->kind == dungeon::PendingSaveKind::abyss_start);
    const bool activated =
        test::DungeonSessionTestAccess::activate_staged_room_population(
            session);
    const std::uint64_t after_activation = test::allocation_count();

    ARPG_REQUIRE(activated);
    ARPG_REQUIRE(after_activation - before == 3U);
    ARPG_REQUIRE(session.snapshot().combat.has_value());
    ARPG_REQUIRE(test::room_environment_blueprint(session) != nullptr);
    ARPG_REQUIRE(test::staged_room_monster_field(session) == nullptr);
    ARPG_REQUIRE(test::staged_room_environment_blueprint(session)
        == nullptr);
    ARPG_REQUIRE(staged_progress_empty(
        test::staged_room_progress(session)));

    DungeonSession committed{
        rules, empty_inventory_abyss_available_state()};
    const dungeon::PendingSave* const committed_pending =
        committed.pending_save_view();
    ARPG_REQUIRE(committed_pending != nullptr);
    ARPG_REQUIRE(committed_pending->kind
        == dungeon::PendingSaveKind::abyss_start);
    committed.resolve_pending_save({dungeon::SaveDisposition::committed,
        committed_pending->expected_generation,
        committed_pending->next_state});
    ARPG_REQUIRE(committed.snapshot().phase == RoomPhase::locked);
    ARPG_REQUIRE(committed.snapshot().combat.has_value());
    ARPG_REQUIRE(test::room_environment_blueprint(committed) != nullptr);
    return {};
}

arpg::test::Failure owner_allocation_failures_rollback_population() noexcept {
    using namespace arpg;
    DungeonRules rules{};
    for (std::size_t failure_index = 0U;
         failure_index < 3U; ++failure_index) {
        DungeonRunState state = state_for_seed(
            0xA110C000U + failure_index, rules);
        ARPG_REQUIRE(state.item_ownership.items.empty());
        DungeonSession session{rules, state};
        test::DungeonSessionTestAccess::configure_current_room_construction(
            session, std::move(state));
        {
            test::ScopedAllocationFailure fail{failure_index};
            test::DungeonSessionTestAccess::construct_current_room(session);
        }
        const auto snapshot = session.snapshot();
        ARPG_REQUIRE(snapshot.phase == RoomPhase::faulted);
        ARPG_REQUIRE(snapshot.diagnostics.fault
            == (failure_index < 2U
                ? dungeon::DungeonFault::population_capacity
                : dungeon::DungeonFault::environment_navigation));
        ARPG_REQUIRE(!snapshot.combat.has_value());
        ARPG_REQUIRE(snapshot.initial_monster_count == 0U);
        ARPG_REQUIRE(snapshot.defeated_monster_count == 0U);
        ARPG_REQUIRE(snapshot.remaining_targets == 0U);
        ARPG_REQUIRE(test::combat_world_address(session) == nullptr);
        ARPG_REQUIRE(test::room_environment_blueprint(session) == nullptr);
        ARPG_REQUIRE(test::staged_room_monster_field(session) == nullptr);
        ARPG_REQUIRE(test::staged_room_environment_blueprint(session)
            == nullptr);
        ARPG_REQUIRE(staged_progress_empty(
            test::staged_room_progress(session)));
        ARPG_REQUIRE(staged_progress_empty(
            test::DungeonSessionTestAccess::room_progress(session)));
        std::size_t event_count = 0U;
        while (const auto event = session.try_pop_event()) {
            ARPG_REQUIRE(event->kind
                == dungeon::DungeonEventKind::faulted);
            ++event_count;
        }
        ARPG_REQUIRE(event_count == 1U);
    }
    return {};
}

arpg::test::Failure reset_and_reload_rebuild_equal_blueprints() noexcept {
    const DungeonRules rules{};
    const DungeonRunState state = state_for_seed(0xC0FFEEU, rules);
    DungeonSession canonical{rules, state};
    DungeonSession reset{rules, state};
    const auto before = canonical.snapshot();
    ARPG_REQUIRE(reset.reset_current_room()
        == arpg::dungeon::RequestResult::accepted);
    const auto after_reset = reset.snapshot();
    DungeonSession reloaded{rules, state};
    const auto after_reload = reloaded.snapshot();
    const auto* const canonical_plan = arpg::test::room_monster_plan(canonical);
    const auto* const reset_plan = arpg::test::room_monster_plan(reset);
    const auto* const reload_plan = arpg::test::room_monster_plan(reloaded);
    const auto* const canonical_environment =
        arpg::test::room_environment_blueprint(canonical);
    const auto* const reset_environment =
        arpg::test::room_environment_blueprint(reset);
    const auto* const reload_environment =
        arpg::test::room_environment_blueprint(reloaded);
    ARPG_REQUIRE(canonical_plan != nullptr);
    ARPG_REQUIRE(reset_plan != nullptr);
    ARPG_REQUIRE(reload_plan != nullptr);
    ARPG_REQUIRE(canonical_environment != nullptr);
    ARPG_REQUIRE(reset_environment != nullptr);
    ARPG_REQUIRE(reload_environment != nullptr);
    ARPG_REQUIRE(arpg::dungeon::room_monster_plan_equal_fields(
        *canonical_plan, *reset_plan));
    ARPG_REQUIRE(arpg::dungeon::room_monster_plan_equal_fields(
        *canonical_plan, *reload_plan));
    ARPG_REQUIRE(same_environment_blueprint(
        *canonical_environment, *reset_environment));
    ARPG_REQUIRE(same_environment_blueprint(
        *canonical_environment, *reload_environment));
    ARPG_REQUIRE(before.initial_monster_count
        == after_reset.initial_monster_count);
    ARPG_REQUIRE(before.initial_monster_count
        == after_reload.initial_monster_count);
    ARPG_REQUIRE(before.monster_generator_version
        == after_reset.monster_generator_version);
    ARPG_REQUIRE(before.monster_generator_version
        == after_reload.monster_generator_version);
    ARPG_REQUIRE(before.monster_blueprint_hash
        == after_reset.monster_blueprint_hash);
    ARPG_REQUIRE(before.monster_blueprint_hash
        == after_reload.monster_blueprint_hash);
    ARPG_REQUIRE(before.environment_generator_version
        == after_reset.environment_generator_version);
    ARPG_REQUIRE(before.environment_generator_version
        == after_reload.environment_generator_version);
    ARPG_REQUIRE(before.environment_blueprint_hash
        == after_reset.environment_blueprint_hash);
    ARPG_REQUIRE(before.environment_blueprint_hash
        == after_reload.environment_blueprint_hash);
    ARPG_REQUIRE(before.combat->monster_count
        == after_reset.combat->monster_count);
    ARPG_REQUIRE(before.combat->monster_count
        == after_reload.combat->monster_count);
    for (std::size_t index = 0U;
         index < before.combat->monster_count; ++index) {
        const auto& first = before.combat->monsters[index];
        const auto& reset_monster = after_reset.combat->monsters[index];
        const auto& reload = after_reload.combat->monsters[index];
        ARPG_REQUIRE(first.monster_ordinal == reset_monster.monster_ordinal);
        ARPG_REQUIRE(first.monster_ordinal == reload.monster_ordinal);
        ARPG_REQUIRE(first.id == reset_monster.id && first.id == reload.id);
        ARPG_REQUIRE(first.position.x == reset_monster.position.x && first.position.x == reload.position.x);
        ARPG_REQUIRE(first.position.y == reset_monster.position.y && first.position.y == reload.position.y);
    }
    return {};
}

arpg::test::Failure legacy_abyss_flag_without_lifecycle_faults() noexcept {
    DungeonRules rules{};
    DungeonRunState abyss = state_for_seed(0xAB155U, rules);
    abyss.current_room.is_abyss = true;
    DungeonSession abyss_session{rules, abyss};
    const auto abyss_snapshot = abyss_session.snapshot();
    ARPG_REQUIRE(abyss_snapshot.phase == RoomPhase::faulted);
    ARPG_REQUIRE(abyss_snapshot.diagnostics.fault
        == arpg::dungeon::DungeonFault::invalid_abyss_state);
    ARPG_REQUIRE(!abyss_snapshot.combat.has_value());
    return {};
}

arpg::test::Failure clearing_local_residency_does_not_start_a_wave() noexcept {
    const DungeonRules rules{};
    DungeonSession session{rules, state_for_seed(0x2A11CEU, rules)};
    const auto* const world = arpg::test::combat_world_address(session);
    const auto* const field = world != nullptr
        ? world->room_monster_field() : nullptr;
    const auto* const environment =
        arpg::test::room_environment_blueprint(session);
    ARPG_REQUIRE(world != nullptr);
    ARPG_REQUIRE(field != nullptr);
    ARPG_REQUIRE(environment != nullptr);
    const std::uint64_t monster_hash = field->plan().blueprint_hash;
    const std::uint64_t environment_hash = environment->blueprint_hash;
    session.tick({});
    const std::uint16_t resident_count =
        session.snapshot().combat->monster_count;
    ARPG_REQUIRE(resident_count > 0U);
    for (std::uint16_t index = 0U; index < resident_count; ++index) {
        ARPG_REQUIRE(arpg::test::defeat_next_live_monster(session));
    }
    session.tick({});
    const auto state = session.snapshot();
    ARPG_REQUIRE(state.phase == RoomPhase::combat);
    ARPG_REQUIRE(state.remaining_targets
        == state.initial_monster_count - state.defeated_monster_count);
    ARPG_REQUIRE(state.defeated_monster_count > 0U);
    ARPG_REQUIRE(state.remaining_targets > 0U);
    ARPG_REQUIRE(state.combat->total_living_monsters
        == state.remaining_targets);
    ARPG_REQUIRE(state.wave_index == 0U);
    ARPG_REQUIRE(state.wave_count == 1U);
    ARPG_REQUIRE(state.wave_delay_ticks == 0U);
    auto* const mutable_world = arpg::test::mutable_combat_world(session);
    ARPG_REQUIRE(mutable_world == world);
    auto* const mutable_field = mutable_world->room_monster_field();
    ARPG_REQUIRE(mutable_field == field);
    const auto home_region = arpg::combat::make_room_streaming_region(
        mutable_world->player_position());
    const arpg::combat::RoomStreamingRegion far_region{
        0U,
        static_cast<std::uint8_t>(
            arpg::combat::room_spatial::maximum_streaming_columns),
        0U,
        static_cast<std::uint8_t>(
            arpg::combat::room_spatial::maximum_streaming_rows),
    };
    ARPG_REQUIRE(mutable_field->synchronize_active_region(far_region));
    ARPG_REQUIRE(mutable_field->resident_ordinals().count > 0U);
    ARPG_REQUIRE(mutable_field->synchronize_active_region(home_region));
    ARPG_REQUIRE(arpg::test::combat_world_address(session) == world);
    ARPG_REQUIRE(world->room_monster_field() == field);
    ARPG_REQUIRE(arpg::test::room_environment_blueprint(session)
        == environment);
    ARPG_REQUIRE(field->plan().blueprint_hash == monster_hash);
    ARPG_REQUIRE(environment->blueprint_hash == environment_hash);
    std::size_t population_events = 0U;
    while (const auto event = session.try_pop_event()) {
        population_events += event->kind
            == arpg::dungeon::DungeonEventKind::population_generated;
    }
    ARPG_REQUIRE(population_events == 1U);
    return {};
}

arpg::test::Failure duplicate_defeat_ordinal_faults_lifecycle() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session;
    const auto initial = session.snapshot().initial_monster_count;
    ARPG_REQUIRE(test::append_defeat_record(session, 0U));
    ARPG_REQUIRE(test::append_defeat_record(session, 0U));
    ARPG_REQUIRE(test::append_defeat_record(session, 1U));

    test::relay_combat_lifecycle(session);
    const auto faulted = session.snapshot();
    ARPG_REQUIRE(faulted.phase == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(faulted.diagnostics.fault
        == dungeon::DungeonFault::invalid_monster_plan);
    ARPG_REQUIRE(faulted.defeated_monster_count == 1U);
    ARPG_REQUIRE(faulted.remaining_targets == initial - 1U);
    ARPG_REQUIRE(test::defeat_ledger_size(session) == 1U);
    return {};
}

arpg::test::Failure invalid_defeat_ordinal_faults_lifecycle() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session;
    const GroundSentinels sentinels = install_ground_sentinels(session);
    const auto before = session.snapshot();
    const auto invalid = static_cast<combat::MonsterOrdinal>(
        before.initial_monster_count);
    const auto initial = before.initial_monster_count;
    ARPG_REQUIRE(test::append_defeat_record(session, invalid));

    test::relay_combat_lifecycle(session);
    const auto faulted = session.snapshot();
    ARPG_REQUIRE(faulted.phase == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(faulted.diagnostics.fault
        == dungeon::DungeonFault::invalid_monster_plan);
    ARPG_REQUIRE(faulted.defeated_monster_count == 0U);
    ARPG_REQUIRE(faulted.remaining_targets == initial);
    ARPG_REQUIRE(faulted.pending_room_experience
        == before.pending_room_experience);
    ARPG_REQUIRE(faulted.ground_item_count == before.ground_item_count);
    ARPG_REQUIRE(faulted.ground_material_count
        == before.ground_material_count);
    ARPG_REQUIRE(faulted.ground_health_potion_count
        == before.ground_health_potion_count);
    ARPG_REQUIRE(same_ground_sentinels(session, sentinels));
    return {};
}

arpg::test::Failure defeat_ledger_overflow_faults_lifecycle() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session;
    const GroundSentinels sentinels = install_ground_sentinels(session);
    const auto before = session.snapshot();
    const auto initial = before.initial_monster_count;
    ARPG_REQUIRE(initial
        > combat::kDefeatLedgerCapacity);
    ARPG_REQUIRE(test::inject_defeat_ledger_overflow(session));

    test::relay_combat_lifecycle(session);
    const auto faulted = session.snapshot();
    ARPG_REQUIRE(faulted.phase == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(faulted.diagnostics.fault
        == dungeon::DungeonFault::defeat_ledger_overflow);
    ARPG_REQUIRE(faulted.diagnostics.combat_relay_overflow_count == 0U);
    ARPG_REQUIRE(dungeon::dungeon_fault_name(faulted.diagnostics.fault)
        == "defeat_ledger_overflow");
    ARPG_REQUIRE(faulted.defeated_monster_count == 0U);
    ARPG_REQUIRE(faulted.remaining_targets == initial);
    ARPG_REQUIRE(test::defeat_ledger_size(session)
        == combat::kDefeatLedgerCapacity);
    ARPG_REQUIRE(faulted.pending_room_experience
        == before.pending_room_experience);
    ARPG_REQUIRE(faulted.ground_item_count == before.ground_item_count);
    ARPG_REQUIRE(faulted.ground_material_count
        == before.ground_material_count);
    ARPG_REQUIRE(faulted.ground_health_potion_count
        == before.ground_health_potion_count);
    ARPG_REQUIRE(same_ground_sentinels(session, sentinels));
    return {};
}

arpg::test::Failure high_ordinal_defeat_ledger_awards_experience() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session;
    const auto before = session.snapshot();
    ARPG_REQUIRE(before.initial_monster_count > 192U);
    constexpr combat::MonsterOrdinal ordinal = 192U;
    ARPG_REQUIRE(test::relay_defeated(
        session, 2U, 0U, {1.0F, 2.0F, 0.0F}, true,
        combat::MonsterId::fire_bomber, ordinal, 0U, false));
    const auto after = session.snapshot();
    ARPG_REQUIRE(after.phase != dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(after.defeated_monster_count == 1U);
    ARPG_REQUIRE(after.remaining_targets
        == before.initial_monster_count - 1U);
    ARPG_REQUIRE(after.pending_room_experience
        > before.pending_room_experience);
    return {};
}

arpg::test::Failure visual_overflow_does_not_drop_ledger_experience() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session;
    const auto before = session.snapshot();
    test::DungeonSessionTestAccess::fill_current_combat_events(
        session, combat::kCombatEventCapacity);
    ARPG_REQUIRE(test::defeat_next_live_monster(session));
    test::relay_combat_lifecycle(session);

    const auto after = session.snapshot();
    ARPG_REQUIRE(after.phase != dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(after.defeated_monster_count == 1U);
    ARPG_REQUIRE(after.remaining_targets
        == before.initial_monster_count - 1U);
    ARPG_REQUIRE(after.pending_room_experience
        > before.pending_room_experience);
    ARPG_REQUIRE(after.combat->diagnostics.event_overflow_count > 0U);
    ARPG_REQUIRE(after.diagnostics.combat_relay_overflow_count == 0U);
    std::size_t visual_count = 0U;
    while (const auto event = session.try_pop_combat_event()) {
        ARPG_REQUIRE(event->kind != combat::CombatEventKind::defeated);
        ++visual_count;
    }
    ARPG_REQUIRE(visual_count == combat::kCombatEventCapacity);
    return {};
}

arpg::test::Failure rollback_keeps_health_but_committed_room_resets_it() noexcept {
    DungeonRules rules;
    auto state = state_for_seed(0x515151U, rules);
    state.current_room.has_hole = true;
    const auto reach_awaiting = [](DungeonSession& session) noexcept {
        session.tick({});
        arpg::test::force_defeat_current_wave(session);
        session.tick({});
        if (session.snapshot().phase == RoomPhase::committing
                && session.snapshot().pending_save_kind
                    == arpg::dungeon::PendingSaveKind::room_clear) {
            if (!arpg::test::commit_pending(session)) return false;
        }
        if (session.snapshot().phase == RoomPhase::cleared) session.tick({});
        return session.snapshot().phase == RoomPhase::awaiting_exit;
    };

    DungeonSession rollback{rules, state};
    ARPG_REQUIRE(reach_awaiting(rollback));
    arpg::test::damage_current_player(rollback, 100);
    const int damaged = rollback.snapshot().combat->player.hp;
    ARPG_REQUIRE(damaged < rollback.snapshot().combat->player.max_hp);
    ARPG_REQUIRE(rollback.request_descent(true));
    const auto failed = rollback.pending_transition();
    ARPG_REQUIRE(failed.has_value());
    rollback.resolve_pending_transition({arpg::dungeon::SaveDisposition::not_committed, 0U, {}});
    ARPG_REQUIRE(rollback.snapshot().room_seed == state.current_room.seed);
    ARPG_REQUIRE(rollback.snapshot().combat->player.hp == damaged);

    DungeonSession committed{rules, state};
    ARPG_REQUIRE(reach_awaiting(committed));
    arpg::test::damage_current_player(committed, 100);
    ARPG_REQUIRE(committed.request_descent(true));
    const auto pending = committed.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    committed.resolve_pending_transition({arpg::dungeon::SaveDisposition::committed,
        pending->next_state.commit_generation, pending->next_state});
    committed.tick({});
    const auto fresh = committed.snapshot();
    ARPG_REQUIRE(fresh.room_seed != state.current_room.seed);
    ARPG_REQUIRE(fresh.combat->player.hp == fresh.combat->player.max_hp);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"construction builds one complete population", &construction_builds_one_complete_population},
    {"abyss staging and commit allocate three owners",
     &abyss_staging_and_commit_allocate_three_owners},
    {"owner allocation failures rollback population",
     &owner_allocation_failures_rollback_population},
    {"reset and reload rebuild equal blueprints", &reset_and_reload_rebuild_equal_blueprints},
    {"legacy abyss flag without lifecycle faults", &legacy_abyss_flag_without_lifecycle_faults},
    {"clearing local residency does not start a wave", &clearing_local_residency_does_not_start_a_wave},
    {"duplicate defeat ordinal faults lifecycle",
     &duplicate_defeat_ordinal_faults_lifecycle},
    {"invalid defeat ordinal faults lifecycle",
     &invalid_defeat_ordinal_faults_lifecycle},
    {"defeat ledger overflow faults lifecycle",
     &defeat_ledger_overflow_faults_lifecycle},
    {"high ordinal ledger awards experience",
     &high_ordinal_defeat_ledger_awards_experience},
    {"visual overflow preserves ledger experience",
     &visual_overflow_does_not_drop_ledger_experience},
    {"rollback keeps health but committed room resets it", &rollback_keeps_health_but_committed_room_resets_it},
};

}  // namespace

arpg::test::TestSuite dungeon_wave_suite() noexcept {
    return arpg::test::make_suite("dungeon_population_lifecycle", kCases);
}
