#include "dungeon/dungeon_session.hpp"

#include "dungeon/abyss_reward.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_combat_template.hpp"
#include "dungeon/room_navigation.hpp"
#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
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
#include <new>
#include <utility>

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

std::uint16_t affix_drop_chance_bp(std::uint16_t score) noexcept {
    const std::uint32_t chance = 100U
        + static_cast<std::uint32_t>(score) * 150U;
    return static_cast<std::uint16_t>(chance > 5000U ? 5000U : chance);
}

std::uint8_t affix_item_level(
    std::uint64_t depth, std::uint16_t score) noexcept {
    if (depth >= 100U) return 100U;
    const std::uint64_t bonus = std::min<std::uint64_t>(10U,
        (static_cast<std::uint32_t>(score) + 2U) / 3U);
    const std::uint64_t level = depth + bonus;
    return static_cast<std::uint8_t>(level > 100U ? 100U : level);
}

std::uint64_t affix_experience(
    std::uint64_t base_experience, std::uint16_t score) noexcept {
    constexpr std::uint64_t kPercent = 100U;
    const std::uint64_t multiplier = kPercent
        + static_cast<std::uint64_t>(score) * 10U;
    const std::uint64_t whole = base_experience / kPercent;
    const std::uint64_t remainder = base_experience % kPercent;
    const std::uint64_t maximum = (std::numeric_limits<std::uint64_t>::max)();
    if (whole > maximum / multiplier) return maximum;
    const std::uint64_t scaled_whole = whole * multiplier;
    const std::uint64_t scaled_remainder = remainder * multiplier / kPercent;
    return scaled_remainder > maximum - scaled_whole
        ? maximum : scaled_whole + scaled_remainder;
}

DungeonSession::DungeonSession(DungeonSessionConfig config) noexcept
    : DungeonSession(DungeonRules{}, initial_state_for(config)) {}

DungeonSession::DungeonSession(
    DungeonRules rules,
    DungeonRunState stable_state) noexcept
    : rules_(rules), stable_state_(std::move(stable_state)),
      last_exit_(stable_state_.last_direction) {
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
    } else if (phase_ == RoomPhase::cleared) {
        phase_ = RoomPhase::awaiting_exit;
    } else if (combat_.has_value()) {
        if (phase_ == RoomPhase::wave_delay) {
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

            handle_player_defeat();

            if (phase_ == RoomPhase::combat && remaining_targets() == 0U) {
                if (wave_index_ + 1U < encounter_plan_.wave_count) {
                    phase_ = RoomPhase::wave_delay;
                    wave_delay_ticks_ = kWaveDelayTicks;
                } else {
                    prepare_room_clear();
                }
            }
        }

    }

    if (abyss_exit_confirmation_.armed
            && (!combat_.has_value()
                || phase_ != RoomPhase::awaiting_exit)) {
        clear_abyss_exit_confirmation();
    }

    if (stable_state_.abyss.lifecycle == abyss::AbyssLifecycle::cleared
            && (phase_ == RoomPhase::cleared
                || phase_ == RoomPhase::awaiting_exit)) {
        rebuild_committed_abyss_rewards();
        if (phase_ != RoomPhase::faulted && !pending_save_.has_value()) {
            attempt_abyss_reward_materialization();
        }
    }

    if (combat_.has_value() && phase_ != RoomPhase::committing
            && phase_ != RoomPhase::transitioning
            && phase_ != RoomPhase::faulted) {
        const combat::CombatSnapshot state = combat_->snapshot();
        if (phase_ == RoomPhase::awaiting_exit) {
            update_abyss_exit_confirmation_range(state.player.position);
            const auto requested = requested_exit(
                state.player.position, movement);
            if (abyss_exit_confirmation_.armed
                    && abyss_exit_confirmation_.transition
                        == TransitionKind::door) {
                if (!requested.has_value()) {
                    abyss_exit_confirmation_.door_input_released = true;
                } else if (*requested
                        != abyss_exit_confirmation_.direction) {
                    clear_abyss_exit_confirmation();
                }
            }
            request_nearby_pickups(state.player.position);
            if (phase_ == RoomPhase::awaiting_exit
                    && requested.has_value()) {
                attempt_exit(*requested);
            }
        } else {
            request_nearby_pickups(state.player.position);
        }
    }

    ++session_tick_;
}

std::optional<PendingTransition>
DungeonSession::pending_transition() const noexcept {
    if (!pending_save_.has_value()
            || (pending_save_->kind != PendingSaveKind::transition
                && pending_save_->kind != PendingSaveKind::abyss_abandon)) {
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

RequestResult DungeonSession::reset_current_room() noexcept {
    if (phase_ == RoomPhase::transitioning
            || phase_ == RoomPhase::committing
            || phase_ == RoomPhase::faulted) {
        return RequestResult::rejected;
    }
    if (stable_state_.current_room.is_abyss
            && stable_state_.abyss.lifecycle
                == abyss::AbyssLifecycle::started) {
        return prepare_abyss_failure();
    }
    if (stable_state_.current_room.is_abyss
            || (stable_state_.abyss.lifecycle
                    != abyss::AbyssLifecycle::none
                && stable_state_.abyss.lifecycle
                    != abyss::AbyssLifecycle::failed)) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return RequestResult::faulted;
    }
    reset_to_normal_room(true);
    return phase_ == RoomPhase::faulted
        ? RequestResult::faulted : RequestResult::accepted;
}

bool item_id_in_use(
    const items::ItemOwnershipState& ownership,
    const std::array<GroundItem, kGroundDropCapacity>& ground_items,
    std::uint64_t item_id,
    std::uint16_t ignored_ground_index) noexcept {
    if (item_id == 0U) return true;
    for (const items::ItemInstance& item : ownership.items) {
        if (item.id == item_id) return true;
    }
    for (std::uint16_t index = 0U; index < ground_items.size(); ++index) {
        const GroundItem& ground = ground_items[index];
        if (index != ignored_ground_index && ground.active
                && ground.item.id == item_id) {
            return true;
        }
    }
    return false;
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
    if (stable_state_.abyss.lifecycle == abyss::AbyssLifecycle::cleared) {
        construct_cleared_abyss_room();
        return;
    }
    if (stable_state_.abyss.lifecycle == abyss::AbyssLifecycle::available) {
        static_cast<void>(prepare_abyss_start());
        return;
    }
    if (stable_state_.current_room.is_abyss
            || (stable_state_.abyss.lifecycle
                    != abyss::AbyssLifecycle::none
                && stable_state_.abyss.lifecycle
                    != abyss::AbyssLifecycle::failed)) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return;
    }
    construct_normal_room();
}

void DungeonSession::construct_cleared_abyss_room() noexcept {
    if (!stable_state_.current_room.is_abyss
            || stable_state_.abyss.rules_version != abyss::kAbyssRulesVersion) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return;
    }
    const auto selection = abyss::select_abyss_rule(
        stable_state_.current_room.seed, stable_state_.current_room.depth);
    const abyss::AbyssRewardProfile profile = abyss::reward_profile_for(
        stable_state_.abyss.danger, 1U);
    const auto valid_bits = static_cast<std::uint8_t>(
        profile.item_count == 0U ? 0U : (1U << profile.item_count) - 1U);
    const auto& reward = stable_state_.abyss;
    if (!selection.has_value()
            || selection->danger != reward.danger
            || selection->rule != reward.rule
            || reward.reward_total != profile.item_count
            || reward.reward_total == 0U || reward.reward_total > 3U
            || ((reward.generated_mask | reward.claimed_mask
                    | reward.abandoned_mask)
                & static_cast<std::uint8_t>(~valid_bits)) != 0U
            || (reward.claimed_mask
                & static_cast<std::uint8_t>(~reward.generated_mask)) != 0U
            || (reward.abandoned_mask & reward.generated_mask) != 0U) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return;
    }
    const PlayerBuildResult player_build = build_for(stable_state_);
    if (player_build.status != PlayerBuildStatus::valid) {
        enter_fault(DungeonFault::invalid_item_state);
        return;
    }
    auto evasion_stream = core::DeterministicRng::derive_stream(
        stable_state_.current_room.seed, kPlayerEvasionSeedDomain);
    const combat::EncounterWave empty_wave{};
    const auto navigation_config = make_combat_encounter_config(
        stable_state_.current_room.entry,
        rules_.rules_version,
        empty_wave,
        true,
        abyss::combat_config_for(abyss::AbyssRuleId::none),
        player_build.build,
        evasion_stream.next_u64());
    if (!navigation_config.has_value()) {
        enter_fault(DungeonFault::invalid_rules);
        return;
    }
    combat_.emplace(*navigation_config);
    encounter_plan_ = {};
    wave_index_ = 0U;
    wave_delay_ticks_ = 0U;
    phase_ = RoomPhase::cleared;
    rebuild_committed_abyss_rewards();
}

void DungeonSession::rebuild_committed_abyss_rewards() noexcept {
    if (phase_ == RoomPhase::faulted
            || stable_state_.abyss.lifecycle
                != abyss::AbyssLifecycle::cleared) {
        return;
    }
    for (std::uint8_t reward_ordinal = 0U;
         reward_ordinal < stable_state_.abyss.reward_total;
         ++reward_ordinal) {
        const std::uint8_t bit = static_cast<std::uint8_t>(1U << reward_ordinal);
        if ((stable_state_.abyss.generated_mask & bit) == 0U
                || (stable_state_.abyss.claimed_mask & bit) != 0U) {
            continue;
        }

        const GroundItem* existing = nullptr;
        for (const GroundItem& ground : ground_items_) {
            if (ground.active
                    && ground.source == GroundItemSource::abyss_chest
                    && ground.abyss_reward_ordinal == reward_ordinal) {
                existing = &ground;
                break;
            }
        }
        std::uint16_t target_index = existing == nullptr
            ? 0xFFFFU : existing->drop_ordinal;
        if (existing == nullptr) {
            for (std::uint16_t index = 0U;
                 index < ground_items_.size(); ++index) {
                if (!ground_items_[index].active) {
                    target_index = index;
                    break;
                }
            }
            if (target_index == 0xFFFFU) return;
        }
        const std::uint8_t base_item_level = static_cast<std::uint8_t>(
            std::min<std::uint64_t>(stable_state_.current_room.depth, 100U));
        const auto expected = derive_abyss_ground_item(
            stable_state_.current_room.seed, stable_state_.abyss.danger,
            base_item_level, reward_ordinal, target_index);
        if (!expected.has_value()) {
            enter_fault(DungeonFault::abyss_generation_failed);
            return;
        }
        const std::uint16_t ignored_index = existing == nullptr
            ? 0xFFFFU : existing->drop_ordinal;
        if (item_id_in_use(stable_state_.item_ownership, ground_items_,
                expected->item.id, ignored_index)) {
            enter_fault(DungeonFault::abyss_reward_collision);
            return;
        }
        for (const GroundItem& ground : ground_items_) {
            if (!ground.active || &ground == existing) continue;
            if (ground.source == GroundItemSource::abyss_chest
                    && ground.abyss_reward_ordinal == reward_ordinal) {
                enter_fault(DungeonFault::abyss_reward_collision);
                return;
            }
        }
        if (existing != nullptr) {
            if (existing->drop_ordinal >= ground_items_.size()
                    || !same_ground_item(*existing, *expected)) {
                enter_fault(DungeonFault::abyss_reward_collision);
                return;
            }
            continue;
        }

        ground_items_[target_index] = *expected;
    }
}

void DungeonSession::attempt_abyss_reward_materialization() noexcept {
    if (pending_save_.has_value() || pending_abyss_reward_.has_value()
            || stable_state_.abyss.lifecycle
                != abyss::AbyssLifecycle::cleared) {
        return;
    }
    std::uint8_t reward_ordinal = 0xFFU;
    for (std::uint8_t ordinal = 0U;
         ordinal < stable_state_.abyss.reward_total && ordinal < 3U;
         ++ordinal) {
        const std::uint8_t bit = static_cast<std::uint8_t>(1U << ordinal);
        if ((stable_state_.abyss.generated_mask & bit) == 0U
                && (stable_state_.abyss.abandoned_mask & bit) == 0U) {
            reward_ordinal = ordinal;
            break;
        }
    }
    if (reward_ordinal == 0xFFU) return;

    std::uint16_t free_index = 0xFFFFU;
    for (std::uint16_t index = 0U; index < ground_items_.size(); ++index) {
        if (!ground_items_[index].active) {
            free_index = index;
            break;
        }
    }
    if (free_index == 0xFFFFU) {
        saturating_increment(diagnostics_.ground_saturation_count);
        return;
    }
    const std::uint8_t base_item_level = static_cast<std::uint8_t>(
        std::min<std::uint64_t>(stable_state_.current_room.depth, 100U));
    const auto ground = derive_abyss_ground_item(
        stable_state_.current_room.seed, stable_state_.abyss.danger,
        base_item_level, reward_ordinal, free_index);
    if (!ground.has_value()) {
        enter_fault(DungeonFault::abyss_generation_failed);
        return;
    }
    if (item_id_in_use(stable_state_.item_ownership, ground_items_,
            ground->item.id)) {
        enter_fault(DungeonFault::abyss_reward_collision);
        return;
    }
    if (stable_state_.abyss.reward_revision
            == (std::numeric_limits<std::uint32_t>::max)()) {
        enter_fault(DungeonFault::abyss_reward_revision_overflow);
        return;
    }
    if (stable_state_.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return;
    }

    clear_abyss_exit_confirmation();
    try {
        DungeonRunState next = stable_state_;
        ++next.commit_generation;
        next.abyss.generated_mask |= static_cast<std::uint8_t>(
            1U << reward_ordinal);
        ++next.abyss.reward_revision;
        pending_abyss_reward_.emplace(PendingAbyssReward{
            free_index, reward_ordinal, *ground});
        pending_save_.emplace(PendingSave{
            PendingSaveKind::abyss_reward_materialized,
            next.commit_generation,
            std::move(next),
            TransitionKind::none,
            ExitDirection::none,
            phase_,
        });
    } catch (const std::bad_alloc&) {
        pending_abyss_reward_.reset();
        return;
    } catch (...) {
        pending_abyss_reward_.reset();
        enter_fault(DungeonFault::abyss_generation_failed);
        return;
    }
    phase_ = RoomPhase::committing;
}

void DungeonSession::construct_normal_room() noexcept {
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
        abyss::combat_config_for(abyss::AbyssRuleId::none),
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

void DungeonSession::reset_to_normal_room(bool clear_queues) noexcept {
    if (clear_queues) {
        while (events_.try_pop().has_value()) {
        }
        while (combat_events_.try_pop().has_value()) {
        }
    }
    combat_.reset();
    pending_save_.reset();
    pending_item_build_.reset();
    pending_abyss_combat_.reset();
    pending_abyss_reward_.reset();
    clear_abyss_exit_confirmation();
    last_passive_tree_error_ = passives::PassiveTreeError::none;
    construct_current_room();
    if (phase_ != RoomPhase::faulted) {
        static_cast<void>(emit(DungeonEventKind::room_reset));
    }
}

bool DungeonSession::prepare_abyss_start() noexcept {
    if (pending_save_.has_value() || combat_.has_value()
            || !stable_state_.current_room.is_abyss
            || stable_state_.abyss.lifecycle
                != abyss::AbyssLifecycle::available
            || !abyss::is_abyss_roll(stable_state_.current_room.seed)) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return false;
    }
    const auto selection = abyss::select_abyss_rule(
        stable_state_.current_room.seed, stable_state_.current_room.depth);
    if (!selection.has_value()
            || selection->danger != stable_state_.abyss.danger
            || selection->rule != stable_state_.abyss.rule
            || selection->rules_version != stable_state_.abyss.rules_version) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return false;
    }

    EncounterPlanResult built = build_abyss_encounter_plan(
        stable_state_.current_room.seed,
        stable_state_.current_room.depth,
        stable_state_.current_room.ecology,
        rules_.encounter);
    if (built.fault != DungeonFault::none || built.plan.wave_count == 0U) {
        enter_fault(DungeonFault::abyss_generation_failed);
        return false;
    }
    const std::uint8_t base_item_level = static_cast<std::uint8_t>(
        std::min<std::uint64_t>(stable_state_.current_room.depth, 100U));
    const abyss::AbyssRewardProfile reward = abyss::reward_profile_for(
        stable_state_.abyss.danger, base_item_level);
    if (reward.item_count == 0U || reward.item_count > 3U
            || reward.item_level == 0U) {
        enter_fault(DungeonFault::abyss_generation_failed);
        return false;
    }
    const PlayerBuildResult player_build = build_for(stable_state_);
    if (player_build.status != PlayerBuildStatus::valid) {
        enter_fault(DungeonFault::invalid_item_state);
        return false;
    }
    auto evasion_stream = core::DeterministicRng::derive_stream(
        stable_state_.current_room.seed, kPlayerEvasionSeedDomain);
    const auto combat_config = make_combat_encounter_config(
        stable_state_.current_room.entry,
        rules_.rules_version,
        built.plan.waves[0],
        true,
        abyss::combat_config_for(selection->rule),
        player_build.build,
        evasion_stream.next_u64());
    if (!combat_config.has_value()) {
        enter_fault(DungeonFault::abyss_generation_failed);
        return false;
    }
    if (stable_state_.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return false;
    }

    try {
        DungeonRunState next = stable_state_;
        ++next.commit_generation;
        next.abyss.lifecycle = abyss::AbyssLifecycle::started;
        encounter_plan_ = built.plan;
        wave_index_ = 0U;
        wave_delay_ticks_ = 0U;
        pending_abyss_combat_ = *combat_config;
        pending_save_ = PendingSave{
            PendingSaveKind::abyss_start,
            next.commit_generation,
            std::move(next),
            TransitionKind::none,
            ExitDirection::none,
            RoomPhase::locked,
        };
    } catch (...) {
        pending_abyss_combat_.reset();
        enter_fault(DungeonFault::abyss_generation_failed);
        return false;
    }
    phase_ = RoomPhase::committing;
    return true;
}

RequestResult DungeonSession::prepare_abyss_failure() noexcept {
    if (pending_save_.has_value() || !combat_.has_value()
            || !stable_state_.current_room.is_abyss
            || stable_state_.abyss.lifecycle
                != abyss::AbyssLifecycle::started) {
        return RequestResult::rejected;
    }
    if (stable_state_.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return RequestResult::faulted;
    }
    try {
        DungeonRunState next = stable_state_;
        ++next.commit_generation;
        next.current_room.is_abyss = false;
        next.abyss.lifecycle = abyss::AbyssLifecycle::failed;
        pending_save_ = PendingSave{
            PendingSaveKind::abyss_fail,
            next.commit_generation,
            std::move(next),
            TransitionKind::none,
            ExitDirection::none,
            RoomPhase::locked,
        };
    } catch (...) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return RequestResult::faulted;
    }
    phase_ = RoomPhase::committing;
    return RequestResult::accepted;
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
            && event->reward_eligible
            && claim_defeat_reward(*event)) {
            roll_ground_drop(*event);
            const std::size_t monster_index = static_cast<std::size_t>(
                event->monster_id);
            if (monster_index < progression_rules_.monster_experience.size()) {
                saturating_add(pending_room_experience_,
                    affix_experience(
                        progression_rules_.monster_experience[monster_index],
                        event->affix_score));
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

void DungeonSession::handle_player_defeat() noexcept {
    if (phase_ == RoomPhase::faulted || !combat_.has_value()
            || !combat_->player_defeated()) {
        return;
    }
    if (stable_state_.current_room.is_abyss
            && stable_state_.abyss.lifecycle
                == abyss::AbyssLifecycle::started) {
        static_cast<void>(prepare_abyss_failure());
    } else {
        reset_to_normal_room(false);
    }
}

bool DungeonSession::claim_defeat_reward(
    const combat::CombatEvent& event) noexcept {
    const std::uint16_t ordinal = event.spawn_ordinal;
    if (ordinal >= kGroundDropCapacity
            || bit_is_set(stable_state_.item_ownership.claimed_drop_bits, ordinal)
            || bit_is_set(rolled_drop_bits_, ordinal)) {
        return false;
    }
    set_bit(rolled_drop_bits_, ordinal);
    return true;
}

void DungeonSession::roll_ground_drop(
    const combat::CombatEvent& event) noexcept {
    const std::uint16_t ordinal = event.spawn_ordinal;
    if (ordinal >= kGroundDropCapacity) return;

    auto chance = drop_stream(
        stable_state_.current_room.seed, ordinal, kDropChanceDomain);
    if (event.affix_score == 0U) {
        if (chance.next_bounded(100U).value() != 0U) return;
    } else if (chance.next_bounded(10000U).value()
            >= affix_drop_chance_bp(event.affix_score)) {
        return;
    }

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

    if (item_id_in_use(stable_state_.item_ownership, ground_items_, item_id)) {
        enter_fault(DungeonFault::item_id_collision);
        return;
    }

    const std::uint64_t depth = stable_state_.current_room.depth;
    if (depth == 0U) {
        enter_fault(DungeonFault::invalid_item_state);
        return;
    }
    const auto generated = items::generate_item({
        content.next_u64(),
        static_cast<items::ItemSlot>(slot.next_bounded(6U).value()),
        affix_item_level(depth, event.affix_score),
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
        GroundItemSource::monster_drop,
        0xFFU,
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

void DungeonSession::prepare_room_clear() noexcept {
    const bool started_abyss = stable_state_.current_room.is_abyss
        && stable_state_.abyss.lifecycle == abyss::AbyssLifecycle::started;
    if (!started_abyss) {
        settle_room_experience();
        publish_room_clear();
        return;
    }
    if (!combat_.has_value() || pending_save_.has_value()) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return;
    }
    constexpr std::size_t kClearPublicationEventCount = 2U;
    if (!can_emit(kClearPublicationEventCount)) {
        saturating_increment(diagnostics_.event_overflow_count);
        enter_fault(DungeonFault::event_overflow);
        return;
    }
    if (stable_state_.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return;
    }

    const std::uint8_t base_item_level = static_cast<std::uint8_t>(
        std::min<std::uint64_t>(stable_state_.current_room.depth, 100U));
    const abyss::AbyssRewardProfile reward = abyss::reward_profile_for(
        stable_state_.abyss.danger, base_item_level);
    if (reward.item_count == 0U || reward.item_count > 3U) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return;
    }

    std::uint64_t total_experience = pending_room_experience_;
    saturating_add(total_experience,
        progression_rules_.room_clear_experience);
    const progression::ProgressionAward projected =
        progression::apply_experience(
            room_progression_, total_experience, progression_rules_);
    try {
        DungeonRunState next = stable_state_;
        ++next.commit_generation;
        next.progression = projected.state;
        next.abyss.lifecycle = abyss::AbyssLifecycle::cleared;
        next.abyss.reward_total = reward.item_count;
        next.abyss.generated_mask = 0U;
        next.abyss.claimed_mask = 0U;
        next.abyss.abandoned_mask = 0U;
        next.abyss.reward_revision = 0U;
        pending_save_ = PendingSave{
            PendingSaveKind::abyss_clear,
            next.commit_generation,
            std::move(next),
            TransitionKind::none,
            ExitDirection::none,
            RoomPhase::cleared,
        };
    } catch (...) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return;
    }
    phase_ = RoomPhase::committing;
}

void DungeonSession::publish_room_clear() noexcept {
    phase_ = RoomPhase::cleared;
    if (emit(DungeonEventKind::room_cleared)
            && phase_ != RoomPhase::faulted) {
        static_cast<void>(emit(DungeonEventKind::exits_opened));
    }
}

bool DungeonSession::can_emit(std::size_t count) const noexcept {
    return count <= events_.capacity() - events_.size();
}

void DungeonSession::enter_fault(DungeonFault fault) noexcept {
    if (fault == DungeonFault::none) {
        return;
    }
    diagnostics_.fault = fault;
    phase_ = RoomPhase::faulted;
    clear_abyss_exit_confirmation();
    pending_item_build_.reset();
    pending_abyss_combat_.reset();

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
