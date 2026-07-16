#include "dungeon/dungeon_session.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_combat_template.hpp"
#include "dungeon/room_navigation.hpp"
#include "core/deterministic_rng.hpp"
#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"
#include "items/item_modifiers.hpp"
#include "modifiers/player_modifier_values.hpp"
#include "passives/passive_tree_rules.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::dungeon {
namespace {

constexpr std::uint16_t kWaveDelayTicks = 45U;
constexpr std::uint64_t kPlayerEvasionSeedDomain = 0x45564153494F4E31ULL;
constexpr std::uint64_t kDropChanceDomain = 0x44524F505F43484EULL;
constexpr std::uint64_t kDropSlotDomain = 0x44524F505F534C54ULL;
constexpr std::uint64_t kDropContentDomain = 0x44524F505F49544DULL;
constexpr std::uint64_t kDropItemIdDomain = 0x44524F505F49445FULL;

[[nodiscard]] core::DeterministicRng drop_stream(
    std::uint64_t seed,
    std::uint16_t ordinal,
    std::uint64_t domain) noexcept {
    auto ordinal_stream = core::DeterministicRng::derive_stream(seed, ordinal);
    return core::DeterministicRng::derive_stream(
        ordinal_stream.next_u64(), domain);
}

[[nodiscard]] bool bit_is_set(
    const std::array<std::uint64_t, 3>& bits,
    std::uint16_t ordinal) noexcept {
    const std::size_t word = ordinal / 64U;
    const std::uint8_t bit = static_cast<std::uint8_t>(ordinal % 64U);
    return word < bits.size() && (bits[word] & (std::uint64_t{1U} << bit)) != 0U;
}

void set_bit(
    std::array<std::uint64_t, 3>& bits,
    std::uint16_t ordinal) noexcept {
    const std::size_t word = ordinal / 64U;
    const std::uint8_t bit = static_cast<std::uint8_t>(ordinal % 64U);
    if (word < bits.size()) bits[word] |= std::uint64_t{1U} << bit;
}

void saturating_increment(std::uint32_t& value) noexcept {
    if (value != (std::numeric_limits<std::uint32_t>::max)()) {
        ++value;
    }
}

void saturating_add(std::uint64_t& value, std::uint64_t addition) noexcept {
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    value = addition > maximum - value ? maximum : value + addition;
}

[[nodiscard]] DungeonRunState initial_state_for(
    const DungeonSessionConfig& config) noexcept {
    DungeonRunState state = make_initial_run_state(
        config.root_seed, DungeonRules{}).state;
    if (state.current_room.index != config.initial_room_index) {
        state.current_room.index = config.initial_room_index;
        state.current_room.seed = derive_initial_room_seed(
            config.root_seed, config.initial_room_index);
    }
    return state;
}

}  // namespace

DungeonSession::DungeonSession(DungeonSessionConfig config) noexcept
    : DungeonSession(DungeonRules{}, initial_state_for(config)) {}

DungeonSession::DungeonSession(
    DungeonRules rules,
    DungeonRunState stable_state) noexcept
    : rules_(rules), stable_state_(stable_state),
      last_exit_(stable_state.last_direction) {
    if (validate_rules(rules_) != DungeonFault::none) {
        enter_fault(DungeonFault::invalid_rules);
        return;
    }
    construct_current_room();
}

bool DungeonSession::queue_action(combat::Action action) noexcept {
    return phase_ == RoomPhase::combat && combat_.has_value()
        ? combat_->queue_action(action)
        : false;
}

void DungeonSession::tick(combat::MovementInput movement) noexcept {
    if (phase_ == RoomPhase::committing || phase_ == RoomPhase::faulted) {
        ++session_tick_;
        return;
    }

    if (phase_ == RoomPhase::locked) {
        phase_ = RoomPhase::combat;
        if (emit(DungeonEventKind::room_entered)
                && phase_ != RoomPhase::faulted) {
            static_cast<void>(emit(DungeonEventKind::combat_started));
        }
    } else if (phase_ == RoomPhase::transitioning) {
        construct_current_room();
    } else if (combat_.has_value()) {
        if (phase_ == RoomPhase::cleared) {
            phase_ = RoomPhase::awaiting_exit;
        } else if (phase_ == RoomPhase::wave_delay) {
            if (wave_delay_ticks_ != 0U) {
                --wave_delay_ticks_;
            }
            if (wave_delay_ticks_ == 0U) {
                start_next_wave();
            }
        } else if (phase_ == RoomPhase::combat
                || phase_ == RoomPhase::awaiting_exit) {
            combat_->tick(movement);
            relay_combat_events();

            if (phase_ == RoomPhase::combat && remaining_targets() == 0U) {
                if (wave_index_ + 1U < encounter_plan_.wave_count) {
                    phase_ = RoomPhase::wave_delay;
                    wave_delay_ticks_ = kWaveDelayTicks;
                } else {
                    settle_room_experience();
                    phase_ = RoomPhase::cleared;
                    if (emit(DungeonEventKind::room_cleared)
                            && phase_ != RoomPhase::faulted) {
                        static_cast<void>(emit(DungeonEventKind::exits_opened));
                    }
                }
            }
        }

    }

    if (combat_.has_value() && phase_ != RoomPhase::committing
            && phase_ != RoomPhase::transitioning
            && phase_ != RoomPhase::faulted) {
        request_nearby_pickups(combat_->snapshot().player.position);
    }

    if (combat_.has_value() && phase_ == RoomPhase::awaiting_exit) {
        const combat::CombatSnapshot state = combat_->snapshot();
        if (const auto requested = requested_exit(
                state.player.position, movement)) {
            attempt_exit(*requested);
        }
    }

    ++session_tick_;
}

std::optional<PendingTransition>
DungeonSession::pending_transition() const noexcept {
    if (!pending_save_.has_value()
            || pending_save_->kind != PendingSaveKind::transition) {
        return std::nullopt;
    }
    return PendingTransition{
        pending_save_->transition,
        pending_save_->direction,
        pending_save_->expected_generation,
        pending_save_->next_state,
    };
}

std::optional<PendingSave> DungeonSession::pending_save() const noexcept {
    return pending_save_;
}

const PendingSave* DungeonSession::pending_save_view() const noexcept {
    return pending_save_.has_value() ? &*pending_save_ : nullptr;
}

const items::ItemOwnershipState& DungeonSession::item_state() const noexcept {
    return stable_state_.item_ownership;
}

void DungeonSession::reset_current_room() noexcept {
    if (phase_ == RoomPhase::transitioning
            || phase_ == RoomPhase::committing
            || phase_ == RoomPhase::faulted) {
        return;
    }

    while (events_.try_pop().has_value()) {
    }
    while (combat_events_.try_pop().has_value()) {
    }
    combat_.reset();
    pending_save_.reset();
    pending_item_build_.reset();
    last_passive_tree_error_ = passives::PassiveTreeError::none;
    construct_current_room();
    static_cast<void>(emit(DungeonEventKind::room_reset));
}

std::optional<DungeonEvent> DungeonSession::try_pop_event() noexcept {
    return events_.try_pop();
}

std::optional<combat::CombatEvent>
DungeonSession::try_pop_combat_event() noexcept {
    return combat_events_.try_pop();
}

void DungeonSession::construct_current_room() noexcept {
    ground_items_ = {};
    rolled_drop_bits_ = {};
    if (stable_state_.current_room.depth == 0U) {
        enter_fault(DungeonFault::invalid_item_state);
        return;
    }
    room_progression_ = stable_state_.progression;
    pending_room_experience_ = 0U;
    last_room_experience_ = 0U;
    last_levels_gained_ = 0U;
    const EncounterPlanResult plan = build_encounter_plan(
        stable_state_.current_room.seed,
        stable_state_.current_room.depth,
        stable_state_.current_room.ecology,
        rules_.encounter);
    if (plan.fault != DungeonFault::none || plan.plan.wave_count == 0U) {
        enter_fault(plan.fault == DungeonFault::none
            ? DungeonFault::invalid_rules : plan.fault);
        return;
    }
    const auto player_build = build_for(stable_state_);
    if (player_build.status != PlayerBuildStatus::valid) {
        enter_fault(DungeonFault::invalid_item_state);
        return;
    }
    auto evasion_stream = core::DeterministicRng::derive_stream(
        stable_state_.current_room.seed, kPlayerEvasionSeedDomain);
    const auto config = make_combat_encounter_config(
        stable_state_.current_room.entry,
        rules_.rules_version,
        plan.plan.waves[0],
        true,
        player_build.build,
        evasion_stream.next_u64());
    if (!config.has_value()) {
        enter_fault(DungeonFault::invalid_rules);
        return;
    }
    encounter_plan_ = plan.plan;
    wave_index_ = 0U;
    wave_delay_ticks_ = 0U;
    combat_.emplace(*config);
    phase_ = RoomPhase::locked;
}

DungeonSession::PlayerBuildResult DungeonSession::build_for(
    const checkpoint::DungeonRunState& state,
    const items::EquipmentState* equipment_override) const noexcept {
    if (!passives::valid_passive_tree_state(
            state.passive_tree, state.progression)) {
        return {};
    }
    const items::EquipmentProjectionResult equipment_result = equipment_override == nullptr
        ? items::project_equipment_detailed(state.item_ownership)
        : items::project_equipment_detailed(state.item_ownership,
            *equipment_override);
    if (equipment_result.status
            == items::EquipmentProjectionStatus::allocation_failure) {
        return {{}, PlayerBuildStatus::allocation_failure};
    }
    if (equipment_result.status != items::EquipmentProjectionStatus::valid) {
        return {};
    }
    const items::EquipmentProjection& equipment =
        equipment_result.projection;
    std::array<modifiers::Modifier, 256> modifiers{};
    std::size_t modifier_count = 0U;
    if (!passives::append_passive_modifiers(state.passive_tree,
            modifiers.data(), modifiers.size(), modifier_count)) {
        return {};
    }
    if (!equipment.valid
            || equipment.modifier_count > modifiers.size() - modifier_count) {
        return {};
    }
    for (std::size_t index = 0U;
         index < equipment.modifier_count; ++index) {
        modifiers[modifier_count++] = equipment.modifiers[index];
    }

    combat::PlayerCombatBuild build{};
    build.values = modifiers::evaluate_player_modifiers(
        {modifiers.data(), modifier_count});
    build.weapon_physical = equipment.weapon_physical;
    build.local_attack_speed_bp = equipment.local_attack_speed_bp;
    if (!combat::build_player_hit_packet(0, build).has_value()) {
        return {};
    }
    return {build, PlayerBuildStatus::valid};
}

std::optional<combat::PlayerCombatBuild>
DungeonSession::preview_equipment_build(
    const items::EquipmentState& equipment) const noexcept {
    const PlayerBuildResult result = build_for(stable_state_, &equipment);
    return result.status == PlayerBuildStatus::valid
        ? std::optional<combat::PlayerCombatBuild>{result.build}
        : std::nullopt;
}

void DungeonSession::start_next_wave() noexcept {
    if (!combat_.has_value() || wave_index_ + 1U >= encounter_plan_.wave_count) {
        enter_fault(DungeonFault::invalid_rules);
        return;
    }
    const std::uint8_t next_wave = static_cast<std::uint8_t>(wave_index_ + 1U);
    if (!combat_->load_wave(encounter_plan_.waves[next_wave], false)) {
        enter_fault(DungeonFault::invalid_rules);
        return;
    }
    wave_index_ = next_wave;
    phase_ = RoomPhase::combat;
}

void DungeonSession::relay_combat_events() noexcept {
    if (!combat_.has_value()) {
        return;
    }
    while (auto event = combat_->try_pop_event()) {
        if (event->kind == combat::CombatEventKind::defeated
            && event->reward_eligible) {
            roll_ground_drop(*event);
        }
        if (event->kind == combat::CombatEventKind::defeated
            && event->reward_eligible
            && event->target_index < combat_->snapshot().monsters.size()) {
            const combat::MonsterId id = combat_->snapshot()
                .monsters[event->target_index].id;
            const std::size_t monster_index = static_cast<std::size_t>(id);
            if (monster_index < progression_rules_.monster_experience.size()) {
                saturating_add(pending_room_experience_,
                    progression_rules_.monster_experience[monster_index]);
            }
        }
        const bool relayed = combat_events_.try_push(*event);
        if (!relayed) {
            saturating_increment(diagnostics_.combat_relay_overflow_count);
            enter_fault(DungeonFault::combat_relay_overflow);
            assert(relayed && "Dungeon combat event relay overflow");
            return;
        }
    }
}

void DungeonSession::roll_ground_drop(
    const combat::CombatEvent& event) noexcept {
    if (wave_index_ >= encounter_plan_.wave_count) return;
    const auto& wave = encounter_plan_.waves[wave_index_];
    if (event.target_index >= wave.spawn_count) return;
    const std::uint16_t ordinal = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(wave_index_) * 96U
        + event.target_index);
    if (ordinal >= kGroundDropCapacity
            || bit_is_set(stable_state_.item_ownership.claimed_drop_bits, ordinal)
            || bit_is_set(rolled_drop_bits_, ordinal)) {
        return;
    }
    set_bit(rolled_drop_bits_, ordinal);

    auto chance = drop_stream(
        stable_state_.current_room.seed, ordinal, kDropChanceDomain);
    if (chance.next_bounded(100U).value() != 0U) return;

    auto slot = drop_stream(
        stable_state_.current_room.seed, ordinal, kDropSlotDomain);
    auto content = drop_stream(
        stable_state_.current_room.seed, ordinal, kDropContentDomain);
    auto room_stream = core::DeterministicRng::derive_stream(
        stable_state_.root_seed, stable_state_.current_room.index);
    auto item_id_stream = drop_stream(
        room_stream.next_u64(), ordinal, kDropItemIdDomain);
    std::uint64_t item_id = item_id_stream.next_u64();
    if (item_id == 0U) item_id = 1U;

    for (const items::ItemInstance& item : stable_state_.item_ownership.items) {
        if (item.id == item_id) {
            enter_fault(DungeonFault::item_id_collision);
            return;
        }
    }
    for (const GroundItem& ground : ground_items_) {
        if (ground.active && ground.item.id == item_id) {
            enter_fault(DungeonFault::item_id_collision);
            return;
        }
    }

    const std::uint64_t depth = stable_state_.current_room.depth;
    if (depth == 0U) {
        enter_fault(DungeonFault::invalid_item_state);
        return;
    }
    const auto generated = items::generate_item({
        content.next_u64(),
        static_cast<items::ItemSlot>(slot.next_bounded(6U).value()),
        static_cast<std::uint8_t>(depth > 100U ? 100U : depth),
        item_id,
        std::nullopt,
    });
    if (!generated.has_value()) {
        enter_fault(DungeonFault::invalid_item_state);
        return;
    }
    ground_items_[ordinal] = GroundItem{
        true,
        ordinal,
        {event.position.x, event.position.y, 0.0F},
        *generated,
    };
}

void DungeonSession::settle_room_experience() noexcept {
    saturating_add(pending_room_experience_,
        progression_rules_.room_clear_experience);
    last_room_experience_ = pending_room_experience_;
    const progression::ProgressionAward award = progression::apply_experience(
        room_progression_, pending_room_experience_, progression_rules_);
    room_progression_ = award.state;
    last_levels_gained_ = award.levels_gained;
    pending_room_experience_ = 0U;
}

void DungeonSession::enter_fault(DungeonFault fault) noexcept {
    if (fault == DungeonFault::none) {
        return;
    }
    diagnostics_.fault = fault;
    phase_ = RoomPhase::faulted;
    pending_item_build_.reset();

    const DungeonEvent event{
        DungeonEventKind::faulted,
        session_tick_,
        stable_state_.current_room.index,
        stable_state_.current_room.seed,
        0U,
        0U,
        TransitionKind::none,
        last_exit_,
    };
    if (!events_.try_push(event)) {
        saturating_increment(diagnostics_.event_overflow_count);
    }
}

void DungeonSession::emit_committed(
    const checkpoint::RoomDescriptor& previous_room,
    const DungeonRunState& current) noexcept {
    const auto push = [&](DungeonEventKind kind) noexcept {
        const DungeonEvent event{
            kind,
            session_tick_,
            previous_room.index,
            previous_room.seed,
            current.current_room.index,
            current.current_room.seed,
            current.last_transition,
            current.last_direction,
        };
        if (events_.try_push(event)) return true;
        saturating_increment(diagnostics_.event_overflow_count);
        enter_fault(DungeonFault::event_overflow);
        return false;
    };
    if (push(DungeonEventKind::transition_committed)) {
        static_cast<void>(push(DungeonEventKind::room_destroyed));
    }
}

bool DungeonSession::emit(
    DungeonEventKind kind,
    const DungeonRunState* subject,
    const DungeonRunState* destination,
    TransitionKind transition,
    ExitDirection direction) noexcept {
    const DungeonRunState& subject_state = subject != nullptr
        ? *subject : stable_state_;
    const DungeonEvent event{
        kind,
        session_tick_,
        subject_state.current_room.index,
        subject_state.current_room.seed,
        destination != nullptr ? destination->current_room.index : 0U,
        destination != nullptr ? destination->current_room.seed : 0U,
        transition,
        direction,
    };
    const bool emitted = events_.try_push(event);
    if (!emitted) {
        saturating_increment(diagnostics_.event_overflow_count);
        enter_fault(DungeonFault::event_overflow);
        assert(emitted && "Dungeon event queue overflow");
    }
    return emitted;
}

std::uint8_t DungeonSession::remaining_targets() const noexcept {
    if (!combat_.has_value()) {
        return 0U;
    }
    std::uint8_t remaining = 0U;
    const combat::CombatSnapshot state = combat_->snapshot();
    for (const auto& monster : state.monsters) {
        if (monster.active && monster.hp > 0) {
            ++remaining;
        }
    }
    return remaining;
}

}  // namespace arpg::dungeon
