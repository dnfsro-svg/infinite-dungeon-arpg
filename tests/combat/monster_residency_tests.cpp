#include "test_framework.hpp"
#include "allocation_probe.hpp"

#include "combat/combat_defeat_ledger.hpp"
#include "combat/combat_world.hpp"
#include "combat/room_monster_field.hpp"
#include "combat/room_spatial_grid.hpp"
#include "modifiers/effect_set.hpp"
#include "room_field_test_fixture.hpp"
#include "combat_test_support.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace {

using namespace arpg::combat;
namespace fixture = arpg::test::room_field_fixture;

[[nodiscard]] std::unique_ptr<RoomMonsterField> single_monster_field(
    MonsterId id,
    Vec3 spawn,
    MonsterAffixSet affixes = {}) noexcept {
    auto field = std::make_unique<RoomMonsterField>();
    RoomMonsterPlan& plan = field->plan_storage_for_construction();
    fixture::populate_plan(plan, 1U);
    plan.monsters[0].id = id;
    plan.monsters[0].affixes = affixes;
    plan.monsters[0].initial_position = spawn;
    arpg::dungeon::RoomMonsterPlanBuildResult result{};
    result.density.total_count = 1U;
    if (field->seal_plan(result) != RoomMonsterFieldFault::none) return {};
    return field;
}

[[nodiscard]] bool same_persisted_runtime_fields(
    const MonsterRuntime& value,
    const MonsterRuntime& expected) noexcept {
    return value.id == expected.id
        && value.affixes == expected.affixes
        && value.monster_ordinal == expected.monster_ordinal
        && value.spawn_ordinal == expected.spawn_ordinal
        && value.affix_profile.max_hp == expected.affix_profile.max_hp
        && value.affix_profile.armor_rating
            == expected.affix_profile.armor_rating
        && value.affix_profile.max_shield
            == expected.affix_profile.max_shield
        && value.affix_profile.shield_recharge_delay_ticks
            == expected.affix_profile.shield_recharge_delay_ticks
        && value.affix_profile.damage_bp == expected.affix_profile.damage_bp
        && value.affix_profile.attack_timing_bp
            == expected.affix_profile.attack_timing_bp
        && value.affix_profile.move_bp == expected.affix_profile.move_bp
        && value.affix_profile.cooldown_bp
            == expected.affix_profile.cooldown_bp
        && value.affix_profile.horizontal_impulse_bp
            == expected.affix_profile.horizontal_impulse_bp
        && value.kind == expected.kind
        && value.spawn.x == expected.spawn.x
        && value.spawn.y == expected.spawn.y
        && value.spawn.z == expected.spawn.z
        && value.position.x == expected.position.x
        && value.position.y == expected.position.y
        && value.position.z == expected.position.z
        && value.velocity.x == expected.velocity.x
        && value.velocity.y == expected.velocity.y
        && value.velocity.z == expected.velocity.z
        && value.facing == expected.facing
        && value.reaction == expected.reaction
        && value.armor == expected.armor
        && value.reaction_ticks == expected.reaction_ticks
        && value.ai_phase == expected.ai_phase
        && value.ai_ticks == expected.ai_ticks
        && value.attack_serial == expected.attack_serial
        && value.contact_attack_resolved == expected.contact_attack_resolved
        && value.hp == expected.hp
        && value.max_hp == expected.max_hp
        && value.break_value == expected.break_value
        && value.max_break == expected.max_break
        && value.shield == expected.shield
        && value.max_shield == expected.max_shield
        && value.shield_ticks == expected.shield_ticks
        && value.max_shield_ticks == expected.max_shield_ticks
        && value.shield_recharge_ticks == expected.shield_recharge_ticks
        && value.break_window_ticks == expected.break_window_ticks
        && value.hit_stop_ticks == expected.hit_stop_ticks
        && value.engagement_latch == expected.engagement_latch
        && value.burning_ground_ticks == expected.burning_ground_ticks
        && value.blink_assault_ticks == expected.blink_assault_ticks
        && value.blink_empowered == expected.blink_empowered
        && value.attack_target_position.x == expected.attack_target_position.x
        && value.attack_target_position.y == expected.attack_target_position.y
        && value.attack_target_position.z == expected.attack_target_position.z
        && value.attack_vector.x == expected.attack_vector.x
        && value.attack_vector.y == expected.attack_vector.y
        && value.attack_vector.z == expected.attack_vector.z
        && value.affix_warning == expected.affix_warning
        && value.affix_warning_ticks == expected.affix_warning_ticks;
}

void mutate_every_persistent_field(
    MonsterRuntime& runtime,
    std::uint16_t ordinal) noexcept {
    runtime.position = {17.0F + ordinal, -9.0F, 0.0F};
    runtime.velocity = {-0.75F, 0.25F, 1.5F};
    runtime.facing = Facing::left;
    runtime.reaction = ReactionState::airborne;
    runtime.armor = ArmorState::broken;
    runtime.reaction_ticks = 37U;
    runtime.ai_phase = MonsterAiPhase::recovery;
    runtime.ai_ticks = 41U;
    runtime.attack_serial = 0x10203040U + ordinal;
    runtime.contact_attack_resolved = true;
    runtime.hp = 73;
    runtime.max_hp = 211;
    runtime.break_value = 19;
    runtime.max_break = 67;
    runtime.shield = 31;
    runtime.max_shield = 83;
    runtime.shield_ticks = 43U;
    runtime.max_shield_ticks = 89U;
    runtime.shield_recharge_ticks = 17U;
    runtime.break_window_ticks = 29U;
    runtime.hit_stop_ticks = 5U;
    runtime.engagement_latch = 1U;
    runtime.burning_ground_ticks = 53U;
    runtime.blink_assault_ticks = 59U;
    runtime.blink_empowered = true;
    runtime.attack_target_position = {-13.0F, 23.0F, 0.0F};
    runtime.attack_vector = {0.5F, -0.75F, 0.0F};
    runtime.affix_warning = MonsterAffixWarning::chain_lightning;
    runtime.affix_warning_ticks = 61U;
}

[[nodiscard]] arpg::modifiers::EffectDefinition effect_for(
    MonsterOrdinal ordinal) noexcept {
    arpg::modifiers::EffectDefinition definition{};
    definition.id = 0x50000000U + ordinal;
    definition.duration_ticks = 300 + ordinal;
    definition.refresh_rule = arpg::modifiers::RefreshRule::refresh_duration;
    definition.max_stacks = 1U;
    return definition;
}

[[nodiscard]] MonsterAffixSet one_affix(
    const MonsterAffixId id,
    const MonsterAffixTier tier = MonsterAffixTier::m1) noexcept {
    MonsterAffixSet set{};
    set.values[0] = {id, tier};
    set.count = 1U;
    return set;
}

arpg::test::Failure field_keeps_1125_living_states_with_bounded_residency()
    noexcept {
    MonsterPool pool{};
    auto field = std::make_unique<RoomMonsterField>(pool);
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    ARPG_REQUIRE(field->total_count() == 1125U);
    ARPG_REQUIRE(field->living_count() == 1125U);
    ARPG_REQUIRE(field->defeated_count() == 0U);
    for (std::size_t row = 0U; row < room_spatial::rows; ++row) {
        for (std::size_t column = 0U; column < room_spatial::columns;
             ++column) {
            const auto residents = field->required_residents(
                fixture::streaming_region_for_cell(column, row));
            ARPG_REQUIRE(residents.fault == RoomMonsterFieldFault::none);
            ARPG_REQUIRE(residents.count <= 105U);
            ARPG_REQUIRE(std::is_sorted(residents.ordinals.begin(),
                residents.ordinals.begin() + residents.count));
        }
    }
    const RoomStreamingRegion center = fixture::streaming_region_for_cell(
        10U, 10U);
    ARPG_REQUIRE(field->synchronize_active_region(center));
    ARPG_REQUIRE(pool.active_count() == field->required_residents(center).count);
    ARPG_REQUIRE(pool.active_count() <= 105U);
    return {};
}

arpg::test::Failure runtime_and_effect_fields_round_trip_by_global_ordinal()
    noexcept {
    MonsterPool pool{};
    auto field = std::make_unique<RoomMonsterField>(pool);
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    const RoomStreamingRegion center = fixture::streaming_region_for_cell(
        10U, 10U);
    ARPG_REQUIRE(field->synchronize_active_region(center));
    const auto center_residents = field->required_residents(center);
    ARPG_REQUIRE(center_residents.count == 105U);
    constexpr std::array<std::size_t, 3U> kSelected{{0U, 52U, 104U}};
    std::array<MonsterRuntime, kSelected.size()> expected{};
    std::array<MonsterOrdinal, kSelected.size()> ordinals{};
    for (std::size_t index = 0U; index < kSelected.size(); ++index) {
        const MonsterOrdinal ordinal = center_residents.ordinals[kSelected[index]];
        ordinals[index] = ordinal;
        MonsterRuntime* runtime = field->active_runtime(ordinal);
        ARPG_REQUIRE(runtime != nullptr);
        mutate_every_persistent_field(*runtime, ordinal);
        expected[index] = *runtime;
        auto* effects = field->active_effects(ordinal);
        ARPG_REQUIRE(effects != nullptr);
        ARPG_REQUIRE(effects->apply(effect_for(ordinal))
            == arpg::modifiers::ApplyResult::applied);
        effects->tick();
    }

    ARPG_REQUIRE(field->synchronize_active_region(
        fixture::streaming_region_for_cell(0U, 0U)));
    for (std::size_t index = 0U; index < ordinals.size(); ++index) {
        const MonsterPersistentState* persisted =
            field->persistent_state(ordinals[index]);
        ARPG_REQUIRE(persisted != nullptr);
        ARPG_REQUIRE(persisted->touched);
        ARPG_REQUIRE(persisted->effects.remaining_ticks(
            effect_for(ordinals[index]).id)
            == effect_for(ordinals[index]).duration_ticks - 1);
    }

    ARPG_REQUIRE(field->synchronize_active_region(center));
    for (std::size_t index = 0U; index < ordinals.size(); ++index) {
        const MonsterRuntime* restored = field->active_runtime(ordinals[index]);
        ARPG_REQUIRE(restored != nullptr);
        ARPG_REQUIRE(same_persisted_runtime_fields(*restored, expected[index]));
        const auto* effects = field->active_effects(ordinals[index]);
        ARPG_REQUIRE(effects != nullptr);
        ARPG_REQUIRE(effects->remaining_ticks(effect_for(ordinals[index]).id)
            == effect_for(ordinals[index]).duration_ticks - 1);
    }
    return {};
}

arpg::test::Failure all_105_effect_sets_survive_complete_slot_churn() noexcept {
    MonsterPool pool{};
    auto field = std::make_unique<RoomMonsterField>(pool);
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    const RoomStreamingRegion center = fixture::streaming_region_for_cell(
        10U, 10U);
    ARPG_REQUIRE(field->synchronize_active_region(center));
    const auto residents = field->required_residents(center);
    ARPG_REQUIRE(residents.count == 105U);
    for (std::size_t index = 0U; index < residents.count; ++index) {
        const MonsterOrdinal ordinal = residents.ordinals[index];
        auto* effects = field->active_effects(ordinal);
        ARPG_REQUIRE(effects != nullptr);
        ARPG_REQUIRE(effects->apply(effect_for(ordinal))
            == arpg::modifiers::ApplyResult::applied);
    }
    ARPG_REQUIRE(field->synchronize_active_region(
        fixture::streaming_region_for_cell(0U, 0U)));
    ARPG_REQUIRE(field->synchronize_active_region(center));
    for (std::size_t index = 0U; index < residents.count; ++index) {
        const MonsterOrdinal ordinal = residents.ordinals[index];
        const auto* effects = field->active_effects(ordinal);
        ARPG_REQUIRE(effects != nullptr);
        ARPG_REQUIRE(effects->active_count() == 1U);
        ARPG_REQUIRE(effects->remaining_ticks(effect_for(ordinal).id)
            == effect_for(ordinal).duration_ticks);
    }
    return {};
}

arpg::test::Failure owner_and_event_ordinals_survive_active_slot_reuse()
    noexcept {
    MonsterPool pool{};
    auto field = std::make_unique<RoomMonsterField>(pool);
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    const RoomStreamingRegion center = fixture::streaming_region_for_cell(
        10U, 10U);
    ARPG_REQUIRE(field->synchronize_active_region(center));
    const MonsterOrdinal owner = field->required_residents(center).ordinals[0];
    const auto original_handle = field->resident_handle(owner);
    ARPG_REQUIRE(original_handle.has_value());

    ProjectilePool projectiles{};
    HazardPool hazards{};
    const auto projectile = projectiles.spawn(
        owner, {}, {}, 1000U, DamagePacket{1}, 0.1F);
    const auto hazard = hazards.spawn(HazardSource::monster, owner,
        HazardKind::native, {}, 1.0F, 10U, 20U, 5U, DamagePacket{1});
    ARPG_REQUIRE(projectile.has_value());
    ARPG_REQUIRE(hazard.has_value());
    CombatEvent defeat{};
    defeat.kind = CombatEventKind::defeated;
    defeat.target_ordinal = owner;

    ARPG_REQUIRE(field->synchronize_active_region(
        fixture::streaming_region_for_cell(0U, 0U)));
    ARPG_REQUIRE(!field->resident_handle(owner).has_value());
    ARPG_REQUIRE(pool.slots()[original_handle->index].active);
    ARPG_REQUIRE(pool.slots()[original_handle->index].spawn_ordinal != owner);
    ARPG_REQUIRE(projectiles.get(*projectile)->owner_ordinal == owner);
    ARPG_REQUIRE(hazards.get(*hazard)->owner_ordinal == owner);
    ARPG_REQUIRE(defeat.target_ordinal == owner);
    return {};
}

arpg::test::Failure defeat_ledger_preserves_105_unique_same_tick_records()
    noexcept {
    CombatDefeatLedger ledger{};
    std::array<CombatEvent, kCombatEventCapacity - 1U> near_full_visuals{};
    for (CombatEvent& event : near_full_visuals) {
        event.kind = CombatEventKind::hit;
    }
    for (MonsterOrdinal ordinal = 0U; ordinal < 105U; ++ordinal) {
        ARPG_REQUIRE(ledger.append({
            ordinal,
            static_cast<MonsterId>(ordinal
                % static_cast<MonsterOrdinal>(MonsterId::count)),
            ordinal,
            true,
        }));
    }
    ARPG_REQUIRE(ledger.size() == 105U);
    std::array<bool, 105U> seen{};
    for (MonsterOrdinal expected = 0U; expected < 105U; ++expected) {
        const auto record = ledger.try_pop();
        ARPG_REQUIRE(record.has_value());
        ARPG_REQUIRE(record->monster_ordinal == expected);
        ARPG_REQUIRE(record->monster_ordinal < seen.size());
        ARPG_REQUIRE(!seen[record->monster_ordinal]);
        seen[record->monster_ordinal] = true;
    }
    ARPG_REQUIRE(ledger.size() == 0U);
    ARPG_REQUIRE(!ledger.try_pop().has_value());
    ARPG_REQUIRE(near_full_visuals.back().kind == CombatEventKind::hit);
    return {};
}

arpg::test::Failure resident_order_is_independent_of_traversal_history()
    noexcept {
    MonsterPool direct_pool{};
    MonsterPool traversed_pool{};
    auto direct = std::make_unique<RoomMonsterField>(direct_pool);
    auto traversed = std::make_unique<RoomMonsterField>(traversed_pool);
    ARPG_REQUIRE(fixture::seal_test_plan(*direct, 1125U)
        == RoomMonsterFieldFault::none);
    ARPG_REQUIRE(fixture::seal_test_plan(*traversed, 1125U)
        == RoomMonsterFieldFault::none);
    const RoomStreamingRegion target = fixture::streaming_region_for_cell(
        10U, 10U);
    ARPG_REQUIRE(direct->synchronize_active_region(target));
    ARPG_REQUIRE(traversed->synchronize_active_region(
        fixture::streaming_region_for_cell(0U, 0U)));
    ARPG_REQUIRE(traversed->synchronize_active_region(
        fixture::streaming_region_for_cell(19U, 19U)));
    ARPG_REQUIRE(traversed->synchronize_active_region(
        fixture::streaming_region_for_cell(0U, 19U)));
    ARPG_REQUIRE(traversed->synchronize_active_region(target));

    const auto direct_residents = direct->resident_ordinals();
    const auto traversed_residents = traversed->resident_ordinals();
    ARPG_REQUIRE(direct_residents.count == traversed_residents.count);
    ARPG_REQUIRE(std::memcmp(direct_residents.ordinals.data(),
        traversed_residents.ordinals.data(),
        direct_residents.count * sizeof(MonsterOrdinal)) == 0);
    return {};
}

arpg::test::Failure malformed_129_resident_region_reports_capacity_fault()
    noexcept {
    MonsterPool pool{};
    auto field = std::make_unique<RoomMonsterField>(pool);
    constexpr std::uint16_t kCell = 210U;
    fixture::populate_overfull_single_cell_plan(
        field->plan_storage_for_construction(), 129U, kCell);
    arpg::dungeon::RoomMonsterPlanBuildResult result{};
    result.density.total_count = 129U;
    ARPG_REQUIRE(field->seal_plan(result) == RoomMonsterFieldFault::none);
    ARPG_REQUIRE(!field->synchronize_active_region(
        fixture::streaming_region_for_cell(10U, 10U)));
    ARPG_REQUIRE(field->fault()
        == RoomMonsterFieldFault::monster_residency_capacity);
    ARPG_REQUIRE(pool.active_count() == 0U);
    return {};
}

arpg::test::Failure all_400_region_transitions_allocate_nothing() noexcept {
    MonsterPool pool{};
    auto field = std::make_unique<RoomMonsterField>(pool);
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    std::array<bool, arpg::limits::kRoomMonsterCapacity> seen{};
    for (std::size_t row = 0U; row < room_spatial::rows; ++row) {
        for (std::size_t column = 0U; column < room_spatial::columns;
             ++column) {
            ARPG_REQUIRE(field->synchronize_active_region(
                fixture::streaming_region_for_cell(column, row)));
            ARPG_REQUIRE(pool.active_count() <= 105U);
            const RoomResidentOrdinals residents = field->resident_ordinals();
            ARPG_REQUIRE(residents.count == pool.active_count());
            seen.fill(false);
            for (std::size_t index = 0U; index < residents.count; ++index) {
                const MonsterOrdinal ordinal = residents.ordinals[index];
                ARPG_REQUIRE(ordinal < seen.size());
                ARPG_REQUIRE(!seen[ordinal]);
                seen[ordinal] = true;
                ARPG_REQUIRE(field->active_runtime(ordinal) != nullptr);
            }
        }
    }
    field->write_back_active();
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    for (MonsterOrdinal ordinal = 0U; ordinal < field->total_count();
         ++ordinal) {
        const MonsterPersistentState* state = field->persistent_state(ordinal);
        ARPG_REQUIRE(state != nullptr);
        ARPG_REQUIRE(!state->touched);
    }
    return {};
}

arpg::test::Failure combat_world_keeps_105_defeats_outside_visual_queue()
    noexcept {
    auto field = std::make_unique<RoomMonsterField>();
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    CombatEncounterConfig config{};
    config.player_spawn = fixture::cell_center(10U, 10U);
    CombatWorld world{config, std::move(field), {}};
    ARPG_REQUIRE(world.fault() == CombatFault::none);
    ARPG_REQUIRE(world.active_monster_count() == 105U);
    arpg::test::CombatWorldTestAccess::fill_event_queue(
        world, kCombatEventCapacity - 1U);
    for (std::size_t slot = 0U; slot < 105U; ++slot) {
        arpg::test::CombatWorldTestAccess::defeat_monster(world, slot, true);
    }
    ARPG_REQUIRE(world.fault() == CombatFault::none);
    ARPG_REQUIRE(world.room_monster_field() != nullptr);
    ARPG_REQUIRE(world.room_monster_field()->defeated_count() == 105U);
    ARPG_REQUIRE(world.living_monster_count() == 1020U);
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monster_count == 105U);
    ARPG_REQUIRE(snapshot.total_living_monsters == 1020U);
    ARPG_REQUIRE(snapshot.defeated_monsters == 105U);
    std::array<bool, 1125U> seen{};
    for (std::size_t index = 0U; index < 105U; ++index) {
        const auto record = world.try_pop_defeat_record();
        ARPG_REQUIRE(record.has_value());
        ARPG_REQUIRE(record->monster_ordinal < seen.size());
        ARPG_REQUIRE(!seen[record->monster_ordinal]);
        seen[record->monster_ordinal] = true;
    }
    ARPG_REQUIRE(!world.try_pop_defeat_record().has_value());
    return {};
}

arpg::test::Failure dormant_projectile_owner_stays_authoritative() noexcept {
    auto field = std::make_unique<RoomMonsterField>();
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    CombatEncounterConfig config{};
    config.player_spawn = fixture::cell_center(10U, 10U);
    CombatWorld world{config, std::move(field), {}};
    RoomMonsterField* active_field = world.room_monster_field();
    ARPG_REQUIRE(active_field != nullptr);
    const auto center_residents = active_field->resident_ordinals();
    ARPG_REQUIRE(center_residents.count == 105U);
    const MonsterOrdinal owner = center_residents.ordinals[0];
    const auto owner_handle = active_field->resident_handle(owner);
    ARPG_REQUIRE(owner_handle.has_value());
    const Vec3 destination = fixture::cell_center(0U, 0U);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::spawn_projectile(
        world, *owner_handle, destination, {}, 1000U, DamagePacket{10},
        0.25F, false, {}));
    ARPG_REQUIRE(world.snapshot().projectiles[0].owner_ordinal == owner);
    arpg::test::CombatWorldTestAccess::set_player_position(world, destination);
    const int hp_before = world.snapshot().player.hp;
    world.tick({});
    ARPG_REQUIRE(!active_field->resident_handle(owner).has_value());
    ARPG_REQUIRE(active_field->persistent_state(owner) != nullptr);
    ARPG_REQUIRE(!active_field->persistent_state(owner)->defeated);
    ARPG_REQUIRE(world.snapshot().player.hp < hp_before);
    ARPG_REQUIRE(world.active_projectile_count() == 0U);
    return {};
}

arpg::test::Failure room_field_rejects_legacy_pool_mutators() noexcept {
    auto field = std::make_unique<RoomMonsterField>();
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    CombatEncounterConfig config{};
    config.player_spawn = fixture::cell_center(10U, 10U);
    CombatWorld world{config, std::move(field), {}};
    RoomMonsterField* active_field = world.room_monster_field();
    ARPG_REQUIRE(active_field != nullptr);
    const RoomResidentOrdinals residents = active_field->resident_ordinals();
    ARPG_REQUIRE(residents.count == 105U);
    const auto handle = active_field->resident_handle(residents.ordinals[0]);
    ARPG_REQUIRE(handle.has_value());

    EncounterWave wave{};
    wave.spawn_count = 1U;
    wave.spawns[0] = {MonsterId::fire_bomber, {}, {}, 0U};
    ARPG_REQUIRE(!world.load_wave(wave));
    ARPG_REQUIRE(!world.destroy_monster(*handle));
    ARPG_REQUIRE(world.active_monster_count() == residents.count);
    ARPG_REQUIRE(world.living_monster_count() == 1125U);
    ARPG_REQUIRE(active_field->resident_handle(residents.ordinals[0])
        .has_value());
    return {};
}

arpg::test::Failure dormant_owner_can_spawn_chain_hazard_by_ordinal()
    noexcept {
    auto field = std::make_unique<RoomMonsterField>();
    RoomMonsterPlan& plan = field->plan_storage_for_construction();
    fixture::populate_plan(plan, 1125U);
    constexpr std::uint16_t kHomeCell = 210U;
    const MonsterOrdinal owner = plan.cell_offsets[kHomeCell];
    plan.monsters[owner].affixes = one_affix(
        MonsterAffixId::chain_lightning);
    arpg::dungeon::RoomMonsterPlanBuildResult result{};
    result.density.total_count = 1125U;
    ARPG_REQUIRE(field->seal_plan(result) == RoomMonsterFieldFault::none);

    CombatEncounterConfig config{};
    config.player_spawn = fixture::cell_center(10U, 10U);
    CombatWorld world{config, std::move(field), {}};
    RoomMonsterField* active_field = world.room_monster_field();
    ARPG_REQUIRE(active_field != nullptr);
    ARPG_REQUIRE(active_field->active_runtime(owner) != nullptr);
    ARPG_REQUIRE(active_field->synchronize_active_region(
        fixture::streaming_region_for_cell(0U, 0U)));
    ARPG_REQUIRE(active_field->active_runtime(owner) == nullptr);
    const MonsterPersistentState* state = active_field->persistent_state(owner);
    ARPG_REQUIRE(state != nullptr && !state->defeated && state->hp > 0);
    const Vec3 center = fixture::cell_center(0U, 0U);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::trigger_chain_lightning(
        world, owner, state->affixes, center));

    const CombatSnapshot snapshot = world.snapshot();
    bool hazard_found = false;
    for (const HazardSnapshot& hazard : snapshot.hazards) {
        hazard_found |= hazard.active
            && hazard.kind == HazardKind::chain_lightning
            && hazard.owner_ordinal == owner;
    }
    ARPG_REQUIRE(hazard_found);
    bool warning_found = false;
    while (const auto event = world.try_pop_event()) {
        warning_found |= event->kind == CombatEventKind::affix_chain_warning
            && event->target_ordinal == owner;
    }
    ARPG_REQUIRE(warning_found);
    return {};
}

arpg::test::Failure room_field_applies_abyss_monster_defenses() noexcept {
    auto field = std::make_unique<RoomMonsterField>();
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    CombatEncounterConfig config{};
    config.player_spawn = fixture::cell_center(10U, 10U);
    config.abyss.monster_armor_bp = 17500U;
    config.abyss.monster_extra_shield_bp = 5000U;
    CombatWorld world{config, std::move(field), {}};
    RoomMonsterField* active_field = world.room_monster_field();
    ARPG_REQUIRE(active_field != nullptr);
    const MonsterOrdinal ordinal =
        active_field->resident_ordinals().ordinals[0];
    const MonsterPersistentState* state =
        active_field->persistent_state(ordinal);
    ARPG_REQUIRE(state != nullptr);

    MonsterPool reference_pool{};
    MonsterSpawnSpec spec{};
    spec.id = state->id;
    spec.position = state->spawn;
    spec.affixes = state->affixes;
    spec.spawn_ordinal = ordinal;
    const auto reference_handle = reference_pool.spawn(spec, config.abyss);
    ARPG_REQUIRE(reference_handle.has_value());
    const MonsterRuntime* reference = reference_pool.get(*reference_handle);
    ARPG_REQUIRE(reference != nullptr);
    ARPG_REQUIRE(state->affix_profile.armor_rating
        == reference->affix_profile.armor_rating);
    ARPG_REQUIRE(state->max_shield == reference->max_shield);
    ARPG_REQUIRE(state->shield == reference->shield);
    ARPG_REQUIRE(state->max_hp == reference->max_hp);
    return {};
}

arpg::test::Failure home_leash_clamps_to_one_cell_halo() noexcept {
    MonsterPool pool{};
    auto field = std::make_unique<RoomMonsterField>(pool);
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    constexpr MonsterOrdinal kOrdinal = 0U;
    Vec3 maximum{100000.0F, 100000.0F, 7.0F};
    ARPG_REQUIRE(field->clamp_to_home_leash(kOrdinal, maximum));
    ARPG_REQUIRE(maximum.x == room_bounds::min_x
        + 2.0F * room_spatial::cell_width);
    ARPG_REQUIRE(maximum.y == room_bounds::min_y
        + 2.0F * room_spatial::cell_depth);
    ARPG_REQUIRE(maximum.z == 7.0F);
    Vec3 minimum{-100000.0F, -100000.0F, -3.0F};
    ARPG_REQUIRE(field->clamp_to_home_leash(kOrdinal, minimum));
    ARPG_REQUIRE(minimum.x == room_bounds::min_x);
    ARPG_REQUIRE(minimum.y == room_bounds::min_y);
    ARPG_REQUIRE(minimum.z == -3.0F);
    return {};
}

arpg::test::Failure blueprint_obstacles_replace_legacy_fire_geometry()
    noexcept {
    std::array<RoomEnvironmentRecord, 1U> records{};
    std::array<std::uint16_t, kRoomEnvironmentCellCount + 1U> offsets{};
    std::array<std::uint8_t, kRoomEnvironmentCellCount> counts{};
    offsets.fill(1U);
    offsets[0] = 0U;
    counts[0] = 1U;
    const Vec3 obstacle_center = fixture::cell_center(0U, 0U);
    records[0].ordinal = 0U;
    records[0].home_cell = 0U;
    records[0].prop = RoomPropKind::crate;
    records[0].obstacle = {{
        {obstacle_center.x - 0.5F, obstacle_center.y - 0.5F, 0.0F},
        {obstacle_center.x + 0.5F, obstacle_center.y + 0.5F, 2.0F}},
        RoomObstacleKind::breakable, 10U};

    auto field = std::make_unique<RoomMonsterField>();
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    CombatEncounterConfig config{};
    config.player_spawn = {};
    config.fire_room_obstacles = true;
    CombatWorld world{config, std::move(field), {
        records.data(), offsets.data(), counts.data(), 1U}};
    const CombatSnapshot before = world.snapshot();
    ARPG_REQUIRE(before.player.position.x == 0.0F);
    ARPG_REQUIRE(before.player.position.y == 0.0F);
    ARPG_REQUIRE(before.fire_crate_count == 0U);
    ARPG_REQUIRE(world.room_obstacles() != nullptr);
    ARPG_REQUIRE(world.room_obstacles()->state(0U) != nullptr);
    ARPG_REQUIRE(world.room_obstacles()->state(0U)->intact);
    arpg::test::CombatWorldTestAccess::resolve_obstacle_hits(
        world, records[0].obstacle.bounds);
    ARPG_REQUIRE(!world.room_obstacles()->state(0U)->intact);
    return {};
}

arpg::test::Failure empty_blueprint_still_disables_legacy_fire_geometry()
    noexcept {
    auto field = std::make_unique<RoomMonsterField>();
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    CombatEncounterConfig config{};
    config.player_spawn = {};
    config.fire_room_obstacles = true;
    CombatWorld world{config, std::move(field), {}};
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(world.fault() == CombatFault::none);
    ARPG_REQUIRE(world.room_obstacles() != nullptr);
    ARPG_REQUIRE(world.room_obstacles()->obstacle_count() == 0U);
    ARPG_REQUIRE(snapshot.player.position.x == 0.0F);
    ARPG_REQUIRE(snapshot.player.position.y == 0.0F);
    ARPG_REQUIRE(snapshot.fire_crate_count == 0U);
    return {};
}

arpg::test::Failure room_resident_uses_sticky_ten_meter_engagement()
    noexcept {
    const Vec3 spawn = fixture::cell_center(0U, 0U);
    MonsterAffixSet affixes{};
    affixes.values[0] = {
        MonsterAffixId::blink_assault, MonsterAffixTier::m1};
    affixes.count = 1U;
    auto field = single_monster_field(
        MonsterId::chaos_chaser, spawn, affixes);
    ARPG_REQUIRE(field != nullptr);

    CombatEncounterConfig config{};
    config.player_spawn = {spawn.x + 10.01F, spawn.y, 0.0F};
    config.initial_invulnerability_ticks = 1000U;
    CombatWorld world{config, std::move(field), {}};
    ARPG_REQUIRE(world.active_monster_count() == 1U);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_engagement_latch(
        world, 0U) == 0U);
    const CombatSnapshot before = world.snapshot();

    arpg::test::tick_n(world, 480);
    const CombatSnapshot dormant = world.snapshot();
    ARPG_REQUIRE(world.active_monster_count() == 1U);
    ARPG_REQUIRE(dormant.monster_count == 1U);
    ARPG_REQUIRE(dormant.monsters[0].ai_phase == MonsterAiPhase::idle);
    ARPG_REQUIRE(dormant.monsters[0].position.x
        == before.monsters[0].position.x);
    ARPG_REQUIRE(dormant.monsters[0].position.y
        == before.monsters[0].position.y);
    ARPG_REQUIRE(dormant.monsters[0].velocity.x == 0.0F);
    ARPG_REQUIRE(dormant.monsters[0].velocity.y == 0.0F);
    ARPG_REQUIRE(dormant.monsters[0].affix_warning
        == MonsterAffixWarning::none);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_engagement_latch(
        world, 0U) == 0U);

    arpg::test::CombatWorldTestAccess::set_player_position(
        world, {spawn.x + 9.99F, spawn.y, 0.0F});
    world.tick({});
    ARPG_REQUIRE(world.snapshot().monsters[0].ai_phase
        != MonsterAiPhase::idle);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_engagement_latch(
        world, 0U) == 1U);

    arpg::test::CombatWorldTestAccess::set_player_position(
        world, {spawn.x + 20.0F, spawn.y, 0.0F});
    world.tick({});
    ARPG_REQUIRE(world.active_monster_count() == 1U);
    ARPG_REQUIRE(world.snapshot().monsters[0].ai_phase
        != MonsterAiPhase::idle);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_engagement_latch(
        world, 0U) == 1U);
    return {};
}

arpg::test::Failure front_armor_hit_wakes_distant_room_resident() noexcept {
    const Vec3 spawn = fixture::cell_center(0U, 0U);
    auto field = single_monster_field(MonsterId::water_bulwark, spawn);
    ARPG_REQUIRE(field != nullptr);

    CombatEncounterConfig config{};
    config.player_spawn = {spawn.x + 12.0F, spawn.y, 0.0F};
    CombatWorld world{config, std::move(field), {}};
    const CombatSnapshot before = world.snapshot();
    ARPG_REQUIRE(before.monsters[0].armor == ArmorState::armored);
    ARPG_REQUIRE(before.monsters[0].reaction == ReactionState::idle);
    ARPG_REQUIRE(before.monsters[0].ai_phase == MonsterAiPhase::idle);

    PlayerAttackHitSpec hit{};
    hit.source = AttackId::j1;
    hit.base_physical = 1;
    hit.break_damage = 1;
    hit.impact = ImpactKind::light_hitstun;
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::resolve_player_attack_hit(
        world, 0U, hit));
    const CombatSnapshot after = world.snapshot();
    ARPG_REQUIRE(after.monsters[0].reaction == ReactionState::idle);
    ARPG_REQUIRE(after.monsters[0].armor == ArmorState::armored);
    ARPG_REQUIRE(after.monsters[0].ai_phase == MonsterAiPhase::move);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_engagement_latch(
        world, 0U) == 1U);
    return {};
}

arpg::test::Failure old_unlatched_move_state_sleeps_when_distant() noexcept {
    const Vec3 spawn = fixture::cell_center(0U, 0U);
    auto field = single_monster_field(MonsterId::chaos_chaser, spawn);
    ARPG_REQUIRE(field != nullptr);

    CombatEncounterConfig config{};
    config.player_spawn = {spawn.x + 20.0F, spawn.y, 0.0F};
    CombatWorld world{config, std::move(field), {}};
    auto old_save = std::make_unique<arpg::checkpoint::RoomCombatCheckpoint>();
    ARPG_REQUIRE(old_save != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*old_save));
    ARPG_REQUIRE(old_save->monster_count == 1U);
    old_save->monsters[0].reaction =
        arpg::checkpoint::ReactionState::idle;
    old_save->monsters[0].ai_phase =
        arpg::checkpoint::MonsterAiPhase::move;
    old_save->monsters[0].ai_ticks = 0U;
    old_save->monsters[0].velocity = {-0.5F, 0.25F, 0.0F};
    old_save->monsters[0].engagement_latch = 0U;
    ARPG_REQUIRE(world.restore_room_checkpoint(*old_save));
    const CombatSnapshot before = world.snapshot();
    ARPG_REQUIRE(before.monsters[0].ai_phase == MonsterAiPhase::move);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_engagement_latch(
        world, 0U) == 0U);

    world.tick({});
    const CombatSnapshot after = world.snapshot();
    ARPG_REQUIRE(after.monsters[0].ai_phase == MonsterAiPhase::idle);
    ARPG_REQUIRE(after.monsters[0].position.x
        == before.monsters[0].position.x);
    ARPG_REQUIRE(after.monsters[0].position.y
        == before.monsters[0].position.y);
    ARPG_REQUIRE(after.monsters[0].velocity.x == 0.0F);
    ARPG_REQUIRE(after.monsters[0].velocity.y == 0.0F);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_engagement_latch(
        world, 0U) == 0U);
    return {};
}

arpg::test::Failure old_unlatched_attack_phase_becomes_engaged() noexcept {
    const Vec3 spawn = fixture::cell_center(0U, 0U);
    auto field = single_monster_field(MonsterId::chaos_chaser, spawn);
    ARPG_REQUIRE(field != nullptr);

    CombatEncounterConfig config{};
    config.player_spawn = {spawn.x + 20.0F, spawn.y, 0.0F};
    CombatWorld world{config, std::move(field), {}};
    auto old_save = std::make_unique<arpg::checkpoint::RoomCombatCheckpoint>();
    ARPG_REQUIRE(old_save != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*old_save));
    ARPG_REQUIRE(old_save->monster_count == 1U);
    old_save->monsters[0].reaction =
        arpg::checkpoint::ReactionState::idle;
    old_save->monsters[0].ai_phase =
        arpg::checkpoint::MonsterAiPhase::telegraph;
    old_save->monsters[0].ai_ticks = 5U;
    old_save->monsters[0].engagement_latch = 0U;
    ARPG_REQUIRE(world.restore_room_checkpoint(*old_save));
    world.tick({});
    const CombatSnapshot after = world.snapshot();
    ARPG_REQUIRE(after.monsters[0].ai_phase == MonsterAiPhase::telegraph);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_engagement_latch(
        world, 0U) == 1U);
    return {};
}

arpg::test::Failure old_blink_warning_keeps_unlatched_resident_engaged()
    noexcept {
    const Vec3 spawn = fixture::cell_center(0U, 0U);
    MonsterAffixSet affixes{};
    affixes.values[0] = {
        MonsterAffixId::blink_assault, MonsterAffixTier::m1};
    affixes.count = 1U;
    auto field = single_monster_field(
        MonsterId::chaos_chaser, spawn, affixes);
    ARPG_REQUIRE(field != nullptr);

    CombatEncounterConfig config{};
    config.player_spawn = {spawn.x + 20.0F, spawn.y, 0.0F};
    CombatWorld world{config, std::move(field), {}};
    auto old_save = std::make_unique<arpg::checkpoint::RoomCombatCheckpoint>();
    ARPG_REQUIRE(old_save != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*old_save));
    ARPG_REQUIRE(old_save->monster_count == 1U);
    old_save->monsters[0].reaction =
        arpg::checkpoint::ReactionState::idle;
    old_save->monsters[0].ai_phase =
        arpg::checkpoint::MonsterAiPhase::move;
    old_save->monsters[0].ai_ticks = 0U;
    old_save->monsters[0].engagement_latch = 0U;
    old_save->monsters[0].affix_warning =
        arpg::checkpoint::MonsterAffixWarning::blink;
    old_save->monsters[0].affix_warning_ticks = 5U;
    ARPG_REQUIRE(world.restore_room_checkpoint(*old_save));

    world.tick({});
    const CombatSnapshot after = world.snapshot();
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_engagement_latch(
        world, 0U) == 1U);
    ARPG_REQUIRE(after.monsters[0].affix_warning
        == MonsterAffixWarning::blink);
    ARPG_REQUIRE(after.monsters[0].affix_warning_ticks == 4U);
    return {};
}

arpg::test::Failure old_blink_empower_keeps_unlatched_resident_engaged()
    noexcept {
    const Vec3 spawn = fixture::cell_center(0U, 0U);
    MonsterAffixSet affixes{};
    affixes.values[0] = {
        MonsterAffixId::blink_assault, MonsterAffixTier::m1};
    affixes.count = 1U;
    auto field = single_monster_field(
        MonsterId::chaos_chaser, spawn, affixes);
    ARPG_REQUIRE(field != nullptr);

    CombatEncounterConfig config{};
    config.player_spawn = {spawn.x + 20.0F, spawn.y, 0.0F};
    CombatWorld world{config, std::move(field), {}};
    auto old_save = std::make_unique<arpg::checkpoint::RoomCombatCheckpoint>();
    ARPG_REQUIRE(old_save != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*old_save));
    ARPG_REQUIRE(old_save->monster_count == 1U);
    old_save->monsters[0].reaction =
        arpg::checkpoint::ReactionState::idle;
    old_save->monsters[0].ai_phase =
        arpg::checkpoint::MonsterAiPhase::move;
    old_save->monsters[0].ai_ticks = 0U;
    old_save->monsters[0].engagement_latch = 0U;
    old_save->monsters[0].affix_warning =
        arpg::checkpoint::MonsterAffixWarning::none;
    old_save->monsters[0].affix_warning_ticks = 0U;
    old_save->monsters[0].blink_empowered = true;
    ARPG_REQUIRE(world.restore_room_checkpoint(*old_save));

    world.tick({});
    const CombatSnapshot after = world.snapshot();
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_engagement_latch(
        world, 0U) == 1U);
    ARPG_REQUIRE(after.monsters[0].blink_empowered);
    ARPG_REQUIRE(after.monsters[0].ai_phase != MonsterAiPhase::idle);
    return {};
}

arpg::test::Failure defeat_ledger_overflow_sets_hard_fault() noexcept {
    auto field = std::make_unique<RoomMonsterField>();
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == RoomMonsterFieldFault::none);
    CombatEncounterConfig config{};
    config.player_spawn = fixture::cell_center(10U, 10U);
    CombatWorld world{config, std::move(field), {}};
    for (std::size_t slot = 0U; slot < 105U; ++slot) {
        arpg::test::CombatWorldTestAccess::defeat_monster(world, slot, true);
    }
    ARPG_REQUIRE(world.fault() == CombatFault::none);
    ARPG_REQUIRE(world.room_monster_field()->synchronize_active_region(
        fixture::streaming_region_for_cell(0U, 0U)));
    ARPG_REQUIRE(world.active_monster_count() >= 24U);
    for (std::size_t slot = 0U; slot < 23U; ++slot) {
        arpg::test::CombatWorldTestAccess::defeat_monster(world, slot, true);
    }
    ARPG_REQUIRE(world.fault() == CombatFault::none);
    arpg::test::CombatWorldTestAccess::defeat_monster(world, 23U, true);
    ARPG_REQUIRE(world.fault() == CombatFault::defeat_ledger_overflow);
    std::size_t record_count = 0U;
    while (world.try_pop_defeat_record().has_value()) ++record_count;
    ARPG_REQUIRE(record_count == kDefeatLedgerCapacity);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"1125 persistent states with bounded residency",
        &field_keeps_1125_living_states_with_bounded_residency},
    {"runtime and effects round trip by ordinal",
        &runtime_and_effect_fields_round_trip_by_global_ordinal},
    {"105 effect sets survive complete slot churn",
        &all_105_effect_sets_survive_complete_slot_churn},
    {"owners and events survive slot reuse",
        &owner_and_event_ordinals_survive_active_slot_reuse},
    {"105 defeats use independent non-lossy ledger",
        &defeat_ledger_preserves_105_unique_same_tick_records},
    {"resident order ignores traversal history",
        &resident_order_is_independent_of_traversal_history},
    {"129 residents report dedicated capacity fault",
        &malformed_129_resident_region_reports_capacity_fault},
    {"400 region transitions allocate nothing",
        &all_400_region_transitions_allocate_nothing},
    {"105 defeats bypass saturated visual queue",
        &combat_world_keeps_105_defeats_outside_visual_queue},
    {"dormant projectile owner remains authoritative",
        &dormant_projectile_owner_stays_authoritative},
    {"field rejects legacy pool mutators",
        &room_field_rejects_legacy_pool_mutators},
    {"dormant owner can spawn chain hazard",
        &dormant_owner_can_spawn_chain_hazard_by_ordinal},
    {"room field applies abyss defenses",
        &room_field_applies_abyss_monster_defenses},
    {"home leash clamps to one-cell halo",
        &home_leash_clamps_to_one_cell_halo},
    {"blueprint obstacles replace legacy fire geometry",
        &blueprint_obstacles_replace_legacy_fire_geometry},
    {"empty blueprint disables legacy fire geometry",
        &empty_blueprint_still_disables_legacy_fire_geometry},
    {"room resident uses sticky ten meter engagement",
        &room_resident_uses_sticky_ten_meter_engagement},
    {"front armor hit wakes distant room resident",
        &front_armor_hit_wakes_distant_room_resident},
    {"old unlatched move state sleeps when distant",
        &old_unlatched_move_state_sleeps_when_distant},
    {"old unlatched attack phase becomes engaged",
        &old_unlatched_attack_phase_becomes_engaged},
    {"old blink warning keeps unlatched resident engaged",
        &old_blink_warning_keeps_unlatched_resident_engaged},
    {"old blink empower keeps unlatched resident engaged",
        &old_blink_empower_keeps_unlatched_resident_engaged},
    {"defeat ledger overflow sets hard fault",
        &defeat_ledger_overflow_sets_hard_fault},
};

}  // namespace

arpg::test::TestSuite monster_residency_suite() noexcept {
    return arpg::test::make_suite("monster_residency", kCases);
}
