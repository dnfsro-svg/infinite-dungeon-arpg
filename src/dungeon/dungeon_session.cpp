#include "dungeon/dungeon_session.hpp"

#include "dungeon/abyss_reward.hpp"
#include "dungeon/death_checkpoint.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_combat_template.hpp"
#include "dungeon/room_environment.hpp"
#include "dungeon/room_monster_plan_builder.hpp"
#include "dungeon/room_navigation.hpp"
#include "dungeon/room_progress_checkpoint.hpp"
#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "combat/combat_scaling.hpp"
#include "core/deterministic_rng.hpp"
#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"
#include "items/item_modifiers.hpp"
#include "modifiers/player_modifier_values.hpp"
#include "passives/passive_tree_rules.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <utility>

namespace arpg::dungeon {
namespace {

constexpr std::uint64_t kPlayerEvasionSeedDomain = 0x45564153494F4E31ULL;
constexpr std::uint64_t kDropChanceDomain = 0x44524F505F43484EULL;
constexpr std::uint64_t kDropSlotDomain = 0x44524F505F534C54ULL;
constexpr std::uint64_t kDropContentDomain = 0x44524F505F49544DULL;
constexpr std::uint64_t kDropItemIdDomain = 0x44524F505F49445FULL;

[[nodiscard]] ::arpg::checkpoint::CheckpointVec3 checkpoint_position(
    const combat::Vec3 position) noexcept {
    return {position.x, position.y, position.z};
}

[[nodiscard]] combat::Vec3 combat_position(
    const ::arpg::checkpoint::CheckpointVec3 position) noexcept {
    return {position.x, position.y, position.z};
}

[[nodiscard]] core::DeterministicRng drop_stream(
    std::uint64_t seed,
    std::uint16_t ordinal,
    std::uint64_t domain) noexcept {
    auto ordinal_stream = core::DeterministicRng::derive_stream(seed, ordinal);
    return core::DeterministicRng::derive_stream(
        ordinal_stream.next_u64(), domain);
}

template <std::size_t Size>
[[nodiscard]] bool bit_is_set(
    const std::array<std::uint64_t, Size>& bits,
    std::uint16_t ordinal) noexcept {
    const std::size_t word = ordinal / 64U;
    const std::uint8_t bit = static_cast<std::uint8_t>(ordinal % 64U);
    return word < bits.size() && (bits[word] & (std::uint64_t{1U} << bit)) != 0U;
}

template <std::size_t Size>
void set_bit(
    std::array<std::uint64_t, Size>& bits,
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

[[nodiscard]] bool valid_last_abyss_resolution(
    const checkpoint::LastAbyssResolution& value) noexcept {
    if (value.lifecycle != abyss::AbyssLifecycle::none
            && value.lifecycle != abyss::AbyssLifecycle::failed) {
        return false;
    }
    if (!value.valid) {
        return value.room_seed == 0U
            && value.rule == abyss::AbyssRuleId::none
            && value.total == 0U
            && value.generated == 0U
            && value.claimed == 0U
            && value.abandoned == 0U
            && value.lifecycle == abyss::AbyssLifecycle::none;
    }
    const auto danger = abyss::danger_for_rule(value.rule);
    if (value.room_seed == 0U || !danger.has_value()) return false;
    const std::uint8_t expected_total = abyss::reward_profile_for(
        *danger, 1U).item_count;
    const bool valid_counts = value.total == expected_total
        && value.total != 0U && value.total <= 3U
        && value.generated <= value.total
        && value.claimed <= value.generated
        && value.abandoned <= value.total
        && static_cast<std::uint16_t>(value.generated)
            + static_cast<std::uint16_t>(value.abandoned) == value.total;
    return valid_counts
        && (value.lifecycle != abyss::AbyssLifecycle::failed
            || (value.generated == 0U && value.claimed == 0U
                && value.abandoned == value.total));
}

[[nodiscard]] bool canonical_empty_abyss(
    const checkpoint::AbyssCheckpoint& value) noexcept {
    return value.lifecycle == abyss::AbyssLifecycle::none
        && value.danger == abyss::AbyssDanger::low
        && value.rule == abyss::AbyssRuleId::none
        && value.rules_version == 0U
        && value.reward_total == 0U
        && value.generated_mask == 0U
        && value.claimed_mask == 0U
        && value.abandoned_mask == 0U
        && value.reward_revision == 0U;
}

[[nodiscard]] bool valid_failed_abyss(
    const checkpoint::DungeonRunState& state) noexcept {
    if (state.abyss.lifecycle != abyss::AbyssLifecycle::failed
            || state.current_room.is_abyss
            || state.abyss.reward_total != 0U
            || state.abyss.generated_mask != 0U
            || state.abyss.claimed_mask != 0U
            || state.abyss.abandoned_mask != 0U
            || state.abyss.reward_revision != 0U) {
        return false;
    }
    const auto selection = abyss::select_abyss_rule(
        state.current_room.seed, state.current_room.depth);
    return selection.has_value()
        && state.abyss.danger == selection->danger
        && state.abyss.rule == selection->rule
        && state.abyss.rules_version == selection->rules_version;
}

[[nodiscard]] bool same_combat_death_snapshot(
    const combat::CombatDeathSnapshot& lhs,
    const combat::CombatDeathSnapshot& rhs) noexcept {
    return lhs.tick == rhs.tick
        && lhs.source.kind == rhs.source.kind
        && lhs.source.monster == rhs.source.monster
        && lhs.source.detail_id == rhs.source.detail_id
        && lhs.primary_type == rhs.primary_type
        && lhs.raw_damage == rhs.raw_damage
        && lhs.barrier_loss == rhs.barrier_loss
        && lhs.health_loss == rhs.health_loss
        && lhs.final_damage == rhs.final_damage
        && lhs.recent_damage == rhs.recent_damage
        && lhs.defense.hp == rhs.defense.hp
        && lhs.defense.max_hp == rhs.defense.max_hp
        && lhs.defense.barrier == rhs.defense.barrier
        && lhs.defense.max_barrier == rhs.defense.max_barrier
        && lhs.defense.armor == rhs.defense.armor
        && lhs.defense.evasion == rhs.defense.evasion
        && lhs.defense.armor_reduction_bp == rhs.defense.armor_reduction_bp
        && lhs.defense.evasion_rate_bp == rhs.defense.evasion_rate_bp
        && lhs.defense.damage_reduction == rhs.defense.damage_reduction
        && lhs.defense.damage_reduction_cap
            == rhs.defense.damage_reduction_cap;
}

[[nodiscard]] bool same_death_checkpoint(
    const checkpoint::DeathCheckpoint& lhs,
    const checkpoint::DeathCheckpoint& rhs) noexcept {
    const auto& a = lhs.target_room;
    const auto& b = rhs.target_room;
    return lhs.lifecycle == rhs.lifecycle
        && lhs.data_version == rhs.data_version
        && lhs.death_depth == rhs.death_depth
        && lhs.death_floor_room_index == rhs.death_floor_room_index
        && lhs.death_ecology == rhs.death_ecology
        && lhs.death_was_abyss == rhs.death_was_abyss
        && lhs.source_kind == rhs.source_kind
        && lhs.source_monster_id == rhs.source_monster_id
        && lhs.source_detail_id == rhs.source_detail_id
        && lhs.damage_type == rhs.damage_type
        && lhs.raw_damage == rhs.raw_damage
        && lhs.barrier_loss == rhs.barrier_loss
        && lhs.health_loss == rhs.health_loss
        && lhs.final_damage == rhs.final_damage
        && lhs.recent_damage == rhs.recent_damage
        && lhs.hp == rhs.hp && lhs.max_hp == rhs.max_hp
        && lhs.barrier == rhs.barrier
        && lhs.max_barrier == rhs.max_barrier
        && lhs.armor == rhs.armor && lhs.evasion == rhs.evasion
        && lhs.armor_reduction_bp == rhs.armor_reduction_bp
        && lhs.evasion_rate_bp == rhs.evasion_rate_bp
        && lhs.damage_reduction == rhs.damage_reduction
        && lhs.damage_reduction_cap == rhs.damage_reduction_cap
        && a.index == b.index && a.seed == b.seed
        && a.depth == b.depth
        && a.floor_room_index == b.floor_room_index
        && a.entry == b.entry && a.ecology == b.ecology
        && a.has_hole == b.has_hole && a.is_abyss == b.is_abyss;
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
    try {
        death_validation_scratch_.item_ownership.items.reserve(
            stable_state_.item_ownership.items.empty()
                ? 1U : stable_state_.item_ownership.items.size());
    } catch (...) {
        enter_fault(DungeonFault::invalid_item_state);
        return;
    }
    if (!pending_save_.reserve_items(
            stable_state_.item_ownership.items.size())) {
        enter_fault(DungeonFault::invalid_item_state);
        return;
    }
    if (validate_rules(rules_) != DungeonFault::none) {
        enter_fault(DungeonFault::invalid_rules);
        return;
    }
    construct_current_room();
}

bool DungeonSession::queue_action(combat::Action action) noexcept {
    return phase_ == RoomPhase::combat
        && !health_potion_abyss_clear_retry_gate_active()
        && combat_.has_value()
        ? combat_->queue_action(action)
        : false;
}

bool DungeonSession::health_potion_abyss_clear_retry_gate_active()
    const noexcept {
    return retry_health_potion_abyss_clear_before_combat_;
}

combat::SkillCastResult DungeonSession::request_active_skill_slot(
    std::uint8_t slot) noexcept {
    const std::size_t index = static_cast<std::size_t>(slot);
    if (phase_ != RoomPhase::combat
            || health_potion_abyss_clear_retry_gate_active()
            || !combat_.has_value()
            || index >= skills::kActiveSkillSlotCount) {
        return combat::SkillCastResult::none;
    }
    const skills::ActiveSkillId skill =
        stable_state_.skill_loadout.slots[index].active;
    return skill == skills::ActiveSkillId::none
        ? combat::SkillCastResult::none
        : combat_->request_active_skill(skill);
}

RoomPhase DungeonSession::phase() const noexcept {
    return phase_;
}

void DungeonSession::tick(
    combat::MovementInput movement,
    AutoPickupPolicy pickup_policy) noexcept {
    if (retry_health_potion_abyss_clear_before_combat_) {
        retry_health_potion_abyss_clear_before_combat_ = false;
        if (phase_ != RoomPhase::combat || pending_save_.has_value()
                || !combat_.has_value() || remaining_targets() != 0U) {
            enter_fault(DungeonFault::save_receipt_mismatch);
        } else {
            prepare_room_clear();
        }
        ++session_tick_;
        return;
    }
    if (phase_ == RoomPhase::committing
            || phase_ == RoomPhase::death_pending
            || phase_ == RoomPhase::faulted) {
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
    } else if (combat_.has_value()
            && (phase_ == RoomPhase::combat
                || phase_ == RoomPhase::awaiting_exit)) {
        combat_->tick(movement);
        relay_combat_events();

        handle_player_defeat();

        if (phase_ == RoomPhase::combat) {
            if (!room_progress_.exits_unlocked
                    && room_progress_.required_kills != 0U
                    && room_progress_.defeated_monster_count
                        >= room_progress_.required_kills) {
                prepare_room_unlock();
            } else if (remaining_targets() == 0U) {
                prepare_room_clear();
            }
        }

    }

    if (abyss_exit_confirmation_.armed
            && (!combat_.has_value()
                || (phase_ != RoomPhase::awaiting_exit
                    && !(phase_ == RoomPhase::combat
                        && room_progress_.exits_unlocked)))) {
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
        const combat::Vec3 player_position = combat_->player_position();
        if (phase_ == RoomPhase::awaiting_exit
                || (phase_ == RoomPhase::combat
                    && room_progress_.exits_unlocked)) {
            update_abyss_exit_confirmation_range(player_position);
            const auto requested = requested_exit(
                player_position, movement);
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
            request_nearby_pickups(player_position, pickup_policy);
            if ((phase_ == RoomPhase::awaiting_exit
                    || (phase_ == RoomPhase::combat
                        && room_progress_.exits_unlocked))
                    && requested.has_value()) {
                attempt_exit(*requested);
            }
        } else {
            request_nearby_pickups(player_position, pickup_policy);
        }
    }

    ++session_tick_;
}

std::optional<PendingTransition>
DungeonSession::pending_transition() const noexcept {
    if (!pending_save_.has_value()
            || (pending_save_->kind != PendingSaveKind::transition
                && pending_save_->kind != PendingSaveKind::abyss_abandon
                && pending_save_->kind
                    != PendingSaveKind::abyss_early_exit)) {
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
    return pending_save_.copy();
}

const PendingSave* DungeonSession::pending_save_view() const noexcept {
    return pending_save_.has_value() ? &*pending_save_ : nullptr;
}

const items::ItemOwnershipState& DungeonSession::item_state() const noexcept {
    return stable_state_.item_ownership;
}

RequestResult DungeonSession::reset_current_room() noexcept {
    if (health_potion_abyss_clear_retry_gate_active()
            || phase_ == RoomPhase::transitioning
            || phase_ == RoomPhase::committing
            || phase_ == RoomPhase::death_pending
            || phase_ == RoomPhase::faulted) {
        return RequestResult::rejected;
    }
    retry_health_potion_abyss_clear_before_combat_ = false;
    if (stable_state_.current_room.is_abyss
            && stable_state_.abyss.lifecycle
                == abyss::AbyssLifecycle::started) {
        return prepare_abyss_failure();
    }
    if (stable_state_.current_room.is_abyss
            && stable_state_.abyss.lifecycle
                == abyss::AbyssLifecycle::cleared) {
        return RequestResult::rejected;
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
    const std::array<GroundItem,
        kAuthoritativeEquipmentDropCapacity>& ground_items,
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

bool DungeonSession::capture_save_checkpoint(
    ::arpg::checkpoint::SaveCheckpointSlot& destination,
    const std::uint64_t persistence_revision,
    const DungeonRunState* const durable_override) const noexcept {
    if (persistence_revision == 0U) return false;
    const DungeonRunState& durable = durable_override == nullptr
        ? stable_state_ : *durable_override;
    if (!copy_run_state_reusing_items(destination.state, durable)) return false;
    destination.persistence_revision = persistence_revision;
    checkpoint::clear_room_progress_checkpoint(destination.room_progress);

    const auto& durable_room = durable.current_room;
    const auto& stable_room = stable_state_.current_room;
    const bool same_room_geometry = durable_room.index == stable_room.index
        && durable_room.seed == stable_room.seed
        && durable_room.depth == stable_room.depth
        && durable_room.floor_room_index == stable_room.floor_room_index
        && durable_room.entry == stable_room.entry
        && durable_room.ecology == stable_room.ecology
        && durable_room.has_hole == stable_room.has_hole;
    const bool same_room = same_room_geometry
        && durable_room.is_abyss == stable_room.is_abyss;
    const bool death_room = same_room_geometry
        && durable.death.lifecycle
            == checkpoint::DeathLifecycle::pending_continue
        && combat_.has_value() && combat_->death_snapshot().has_value()
        && durable.death.death_was_abyss == stable_room.is_abyss;
    if ((!same_room && !death_room)
            || !combat_.has_value() || room_environment_ == nullptr) {
        return checkpoint::valid_room_progress_checkpoint_structural(
            destination.room_progress, destination.state);
    }
    const combat::RoomMonsterField* const field =
        combat_->room_monster_field();
    if (field == nullptr) return false;

    checkpoint::RoomProgressCheckpoint& room = destination.room_progress;
    room.lifecycle = combat_->death_snapshot().has_value()
        ? checkpoint::RoomProgressLifecycle::death_pending
        : checkpoint::RoomProgressLifecycle::active;
    room.room_index = durable.current_room.index;
    room.room_seed = durable.current_room.seed;
    room.monster_generator_version = field->plan().generator_version;
    room.monster_blueprint_hash = field->plan().blueprint_hash;
    room.environment_generator_version = room_environment_->generator_version;
    room.environment_blueprint_hash = room_environment_->blueprint_hash;
    room.generated_monsters = room_progress_.initial_monster_count;
    room.defeated_monsters = room_progress_.defeated_monster_count;
    room.required_kills = room_progress_.required_kills;
    room.defeat_bits = room_progress_.defeated_monster_bits;
    const bool post_mutation = durable_override != nullptr
        && pending_save_.has_value()
        && durable_override == &pending_save_->next_state;
    const bool unlock_pending = post_mutation
        && pending_save_->kind == PendingSaveKind::room_unlock;
    const bool full_clear_pending = phase_ == RoomPhase::committing
        && pending_save_.has_value()
        && (pending_save_->kind == PendingSaveKind::room_clear
            || pending_save_->kind == PendingSaveKind::abyss_clear);
    room.exits_unlocked = room_progress_.exits_unlocked
        || unlock_pending || full_clear_pending;
    room.full_clear = room_progress_.full_clear || full_clear_pending;
    room.reward_committed = room.full_clear;
    room.equipment_claim_bits = room_drop_state_.equipment_claim_bits();
    room.secondary_claim_bits = room_drop_state_.secondary_claim_bits();
    if (post_mutation && pending_save_.has_value()) {
        if (pending_save_->kind == PendingSaveKind::loot_pickup
                || pending_save_->kind
                    == PendingSaveKind::abyss_reward_claim) {
            set_bit(room.equipment_claim_bits,
                pending_save_->pickup_ordinal);
        }
        if (pending_save_->kind == PendingSaveKind::material_pickup) {
            set_bit(room.secondary_claim_bits,
                pending_save_->pickup_ordinal);
        }
        if (pending_save_->health_potion_claim.has_value()) {
            const PendingHealthPotionClaim& claim =
                *pending_save_->health_potion_claim;
            for (std::uint8_t index = 0U; index < claim.count; ++index) {
                set_bit(room.secondary_claim_bits,
                    secondary_drop_ordinal(claim.spawn_ordinals[index]));
            }
        }
        if (pending_save_->kind == PendingSaveKind::room_clear
                || pending_save_->kind == PendingSaveKind::abyss_clear) {
            for (const GroundMaterial& material : ground_materials_) {
                if (material.active) {
                    set_bit(room.secondary_claim_bits, material.ordinal);
                }
            }
        }
    }
    const combat::PlayerCombatBuild* const post_build = post_mutation
            && pending_item_build_.has_value()
        ? &*pending_item_build_ : nullptr;
    const std::uint8_t potion_count = post_mutation
            && pending_save_->health_potion_claim.has_value()
        ? pending_save_->health_potion_claim->count : 0U;
    const bool clear_abyss = post_mutation
        && pending_save_->kind == PendingSaveKind::abyss_clear;
    if (!combat_->capture_room_checkpoint_post_mutation(room.combat,
            post_build, potion_count, kHealthPotionRestoreBp, clear_abyss)) {
        return false;
    }

    std::uint16_t previous_equipment = 0U;
    bool have_equipment = false;
    for (std::uint16_t ground_index = 0U;
            ground_index < ground_items_.size(); ++ground_index) {
        const GroundItem* ground_ptr = &ground_items_[ground_index];
        if (!ground_ptr->active && post_mutation
                && pending_abyss_reward_.has_value()
                && pending_abyss_reward_->ground_index == ground_index) {
            ground_ptr = &pending_abyss_reward_->ground;
        }
        const GroundItem& ground = *ground_ptr;
        const bool abyss_claimed = ground.source
                == GroundItemSource::abyss_chest
            && ground.abyss_reward_ordinal < 3U
            && (durable.abyss.claimed_mask
                & static_cast<std::uint8_t>(
                    1U << ground.abyss_reward_ordinal)) != 0U;
        if (!ground.active
                || bit_is_set(room.equipment_claim_bits,
                    ground.drop_ordinal)
                || abyss_claimed) continue;
        if (room.equipment_ground_count >= room.equipment_ground.size()
                || (have_equipment
                    && ground.drop_ordinal <= previous_equipment)) {
            return false;
        }
        auto& packed = room.equipment_ground[room.equipment_ground_count++];
        packed.ordinal = ground.drop_ordinal;
        packed.source = static_cast<std::uint8_t>(ground.source);
        packed.reward_ordinal = ground.abyss_reward_ordinal;
        packed.position = checkpoint_position(ground.position);
        packed.item = ground.item;
        previous_equipment = ground.drop_ordinal;
        have_equipment = true;
    }

    for (std::uint16_t canonical = 0U;
            canonical < checkpoint::kRoomSecondaryGroundCapacity; ++canonical) {
        if (bit_is_set(room.secondary_claim_bits, canonical)) continue;
        const bool abyss_material = canonical
            >= kCheckpointAbyssSecondaryOrdinalBegin;
        const GroundMaterial& material = ground_materials_[canonical];
        if (material.active) {
            const bool valid_source =
                (material.source == GroundMaterialSource::monster_common
                    && !abyss_material && (canonical & 1U) == 0U)
                || (material.source == GroundMaterialSource::monster_coupon
                    && !abyss_material && (canonical & 1U) != 0U)
                || (material.source == GroundMaterialSource::abyss_reward
                    && abyss_material);
            if (material.ordinal != canonical || !valid_source
                    || room.secondary_ground_count
                        >= room.secondary_ground.size()) {
                return false;
            }
            auto& packed =
                room.secondary_ground[room.secondary_ground_count++];
            packed.tag = checkpoint::SecondaryGroundTag::material;
            packed.ordinal = canonical;
            packed.source = static_cast<std::uint8_t>(material.source);
            packed.position = checkpoint_position(material.position);
            packed.material = material.material;
            continue;
        }
        if (abyss_material || (canonical & 1U) == 0U) continue;
        const std::uint16_t spawn = static_cast<std::uint16_t>(canonical / 2U);
        if (spawn >= ground_health_potions_.size()) continue;
        const GroundHealthPotion& potion = ground_health_potions_[spawn];
        if (!potion.active) continue;
        if (potion.spawn_ordinal != spawn
                || potion.claim_ordinal != canonical
                || room.secondary_ground_count
                    >= room.secondary_ground.size()) {
            return false;
        }
        auto& packed = room.secondary_ground[room.secondary_ground_count++];
        packed.tag = checkpoint::SecondaryGroundTag::health_potion;
        packed.ordinal = canonical;
        packed.source = 0U;
        packed.position = checkpoint_position(potion.position);
        packed.material = items::MaterialId::count;
    }
    const bool preserve_room_experience = durable_override == nullptr
        || (post_mutation && pending_save_preserves_room_experience());
    room.pending_room_experience = preserve_room_experience
            && !full_clear_pending
            && room.lifecycle == checkpoint::RoomProgressLifecycle::active
        ? pending_room_experience_ : 0U;
    return checkpoint::valid_room_progress_checkpoint_structural(
        room, destination.state);
}

bool DungeonSession::pending_save_preserves_room_experience()
    const noexcept {
    if (!pending_save_.has_value()) return false;
    switch (pending_save_->kind) {
    case PendingSaveKind::loot_pickup:
    case PendingSaveKind::material_pickup:
    case PendingSaveKind::equipment:
    case PendingSaveKind::craft:
    case PendingSaveKind::recipe:
    case PendingSaveKind::reinforcement:
    case PendingSaveKind::skill_loadout:
    case PendingSaveKind::health_potion_pickup:
    case PendingSaveKind::room_unlock:
        return true;
    case PendingSaveKind::transition:
    case PendingSaveKind::passive_tree:
    case PendingSaveKind::room_clear:
    case PendingSaveKind::abyss_start:
    case PendingSaveKind::abyss_fail:
    case PendingSaveKind::abyss_clear:
    case PendingSaveKind::abyss_reward_materialized:
    case PendingSaveKind::abyss_reward_claim:
    case PendingSaveKind::abyss_abandon:
    case PendingSaveKind::death_retreat:
    case PendingSaveKind::death_continue:
    case PendingSaveKind::abyss_early_exit:
        return false;
    }
    return false;
}

void DungeonSession::clear_buffered_gameplay_input() noexcept {
    if (combat_.has_value()) combat_->clear_buffered_input();
}

bool DungeonSession::restore_room_progress_checkpoint(
    const ::arpg::checkpoint::SaveCheckpointSlot& source) noexcept {
    if (source.room_progress.lifecycle
            == checkpoint::RoomProgressLifecycle::none) {
        return true;
    }
    std::unique_ptr<::arpg::checkpoint::SaveCheckpointSlot> baseline{
        new (std::nothrow) ::arpg::checkpoint::SaveCheckpointSlot{}};
    if (baseline == nullptr) return false;
    try {
        baseline->state.item_ownership.items.reserve(
            stable_state_.item_ownership.items.size());
    } catch (...) {
        return false;
    }
    if (!capture_save_checkpoint(*baseline,
                source.persistence_revision == 0U
                    ? std::uint64_t{1U} : source.persistence_revision)) {
        return false;
    }
    std::unique_ptr<DungeonSession> candidate{};
    try {
        candidate.reset(new (std::nothrow) DungeonSession{
            rules_, stable_state_});
    } catch (...) {
        return false;
    }
    if (candidate == nullptr || candidate->phase_ == RoomPhase::faulted
            || candidate->combat_.has_value() != combat_.has_value()) {
        return false;
    }
    if (combat_.has_value()) {
        candidate->combat_->align_checkpoint_restore_authority_from(*combat_);
    }
    if ((baseline->room_progress.lifecycle
                != checkpoint::RoomProgressLifecycle::none
            && !candidate->restore_room_progress_checkpoint_in_place(*baseline))
            || !candidate->restore_room_progress_checkpoint_in_place(source)
            || candidate->combat_.has_value() != combat_.has_value()) {
        return false;
    }
    adopt_restored_session(std::move(*candidate));
    return true;
}

void DungeonSession::adopt_restored_session(DungeonSession&& source) noexcept {
    while (events_.try_pop().has_value()) {}
    while (combat_events_.try_pop().has_value()) {}
    pending_save_.reset();
    if (source.phase_ == RoomPhase::death_pending) {
        clear_transient_room_state();
        phase_ = RoomPhase::death_pending;
        return;
    }

    room_environment_ = std::move(source.room_environment_);
    assert(combat_.has_value() && source.combat_.has_value());
    combat_->adopt_restored_state(std::move(*source.combat_));
    advance_room_instance_generation();
    room_drop_state_ = std::move(source.room_drop_state_);
    rolled_drop_bits_ = source.rolled_drop_bits_;
    rolled_material_bits_ = source.rolled_material_bits_;
    room_progress_ = source.room_progress_;
    room_progression_ = source.room_progression_;
    pending_room_experience_ = source.pending_room_experience_;
    phase_ = source.phase_;
}

bool DungeonSession::restore_room_progress_checkpoint_in_place(
    const ::arpg::checkpoint::SaveCheckpointSlot& source) noexcept {
    const checkpoint::RoomProgressCheckpoint& room = source.room_progress;
    if (room.lifecycle == checkpoint::RoomProgressLifecycle::none) return true;
    if (!checkpoint::valid_room_progress_checkpoint_structural(
            room, source.state)
            || stable_state_.current_room.index != room.room_index
            || stable_state_.current_room.seed != room.room_seed) {
        return false;
    }
    if (room.lifecycle
            == checkpoint::RoomProgressLifecycle::death_pending
            && stable_state_.death.lifecycle
                == checkpoint::DeathLifecycle::pending_continue
            && (!combat_.has_value() || room_environment_ == nullptr)) {
        const checkpoint::DeathCheckpoint durable_death = stable_state_.death;
        const bool durable_is_abyss = stable_state_.current_room.is_abyss;
        const abyss::AbyssLifecycle durable_abyss_lifecycle =
            stable_state_.abyss.lifecycle;
        stable_state_.death = {};
        stable_state_.current_room.is_abyss = durable_death.death_was_abyss;
        if (durable_death.death_was_abyss) {
            stable_state_.abyss.lifecycle = abyss::AbyssLifecycle::started;
        }
        construct_current_room();
        stable_state_.death = durable_death;
        stable_state_.current_room.is_abyss = durable_is_abyss;
        stable_state_.abyss.lifecycle = durable_abyss_lifecycle;
        if (phase_ == RoomPhase::faulted) return false;
    }
    if (!combat_.has_value() || room_environment_ == nullptr) {
        return false;
    }
    combat::RoomMonsterField* const field =
        combat_->room_monster_field();
    if (field == nullptr
            || field->total_count() != room.generated_monsters
            || field->plan().generator_version
                != room.monster_generator_version
            || field->plan().blueprint_hash != room.monster_blueprint_hash
            || room_environment_->generator_version
                != room.environment_generator_version
            || room_environment_->blueprint_hash
                != room.environment_blueprint_hash) {
        return false;
    }

    std::uint32_t projected_defeated = field->defeated_count();
    for (std::uint32_t ordinal = 0U;
            ordinal < room.generated_monsters; ++ordinal) {
        const auto monster_ordinal =
            static_cast<combat::MonsterOrdinal>(ordinal);
        const combat::MonsterPersistentState* const current =
            field->persistent_state(monster_ordinal);
        const bool defeated = (room.defeat_bits[ordinal / 64U]
            & (std::uint64_t{1U} << (ordinal % 64U))) != 0U;
        if (current == nullptr || (!defeated && current->defeated)) {
            return false;
        }
        if (defeated && !current->defeated) ++projected_defeated;
    }
    if (projected_defeated != room.defeated_monsters) return false;

    for (std::uint16_t index = 0U;
            index < room.equipment_ground_count; ++index) {
        const auto& packed = room.equipment_ground[index];
        if (packed.ordinal >= ground_items_.size()
                || packed.source > static_cast<std::uint8_t>(
                    GroundItemSource::abyss_chest)) return false;
    }
    for (std::uint16_t index = 0U;
            index < room.secondary_ground_count; ++index) {
        const auto& packed = room.secondary_ground[index];
        if (packed.tag == checkpoint::SecondaryGroundTag::material) {
            const std::uint16_t material_ordinal = packed.ordinal;
            const bool abyss_material = packed.ordinal
                >= kCheckpointAbyssSecondaryOrdinalBegin;
            if (material_ordinal >= ground_materials_.size()
                    || packed.source > static_cast<std::uint8_t>(
                        GroundMaterialSource::abyss_reward)) {
                return false;
            }
            const GroundMaterialSource material_source =
                static_cast<GroundMaterialSource>(packed.source);
            const bool valid_source =
                (material_source == GroundMaterialSource::monster_common
                    && !abyss_material && (packed.ordinal & 1U) == 0U)
                || (material_source == GroundMaterialSource::monster_coupon
                    && !abyss_material && (packed.ordinal & 1U) != 0U)
                || (material_source == GroundMaterialSource::abyss_reward
                    && abyss_material);
            if (!valid_source) return false;
            continue;
        }
        const std::uint16_t spawn =
            static_cast<std::uint16_t>(packed.ordinal / 2U);
        if ((packed.ordinal & 1U) == 0U
                || packed.ordinal >= kCheckpointAbyssSecondaryOrdinalBegin
                || spawn >= ground_health_potions_.size()
                || health_potion_claim_ordinal(spawn) != packed.ordinal) {
            return false;
        }
    }

    for (std::uint32_t ordinal = 0U;
            ordinal < room.generated_monsters; ++ordinal) {
        const bool defeated = (room.defeat_bits[ordinal / 64U]
            & (std::uint64_t{1U} << (ordinal % 64U))) != 0U;
        const auto monster_ordinal = static_cast<combat::MonsterOrdinal>(
            ordinal);
        const combat::MonsterPersistentState* const current =
            field->persistent_state(monster_ordinal);
        if (defeated && current != nullptr && !current->defeated
                && !field->mark_defeated(monster_ordinal)) return false;
    }
    if (!combat_->restore_room_checkpoint(room.combat)) {
        return false;
    }

    room_progress_.initial_monster_count = room.generated_monsters;
    room_progress_.defeated_monster_count = room.defeated_monsters;
    room_progress_.required_kills = room.required_kills;
    room_progress_.exits_unlocked = room.exits_unlocked;
    room_progress_.full_clear = room.full_clear;
    room_progress_.defeated_monster_bits = room.defeat_bits;
    room_drop_state_.clear();
    if (!room_drop_state_.reset(field->plan())) return false;
    rolled_drop_bits_ = {};
    rolled_material_bits_ = {};
    pending_room_experience_ = room.pending_room_experience;
    for (std::uint16_t index = 0U;
            index < room.equipment_ground_count; ++index) {
        const auto& packed = room.equipment_ground[index];
        const GroundItem ground{true, packed.ordinal,
            static_cast<GroundItemSource>(packed.source),
            packed.reward_ordinal, combat_position(packed.position),
            packed.item};
        if (!room_drop_state_.place_equipment(ground)) return false;
    }
    for (std::uint16_t index = 0U;
            index < room.secondary_ground_count; ++index) {
        const auto& packed = room.secondary_ground[index];
        if (packed.tag == checkpoint::SecondaryGroundTag::material) {
            const std::uint16_t material_ordinal = packed.ordinal;
            const GroundMaterial ground{true,
                material_ordinal,
                static_cast<GroundMaterialSource>(packed.source),
                combat_position(packed.position), packed.material};
            if (!room_drop_state_.place_material(ground)) return false;
            continue;
        }
        const std::uint16_t spawn =
            static_cast<std::uint16_t>(packed.ordinal / 2U);
        const GroundHealthPotion potion{
            true, spawn, packed.ordinal, combat_position(packed.position)};
        if (!room_drop_state_.place_health_potion(potion)) return false;
    }
    room_drop_state_.restore_claim_bits(
        room.equipment_claim_bits, room.secondary_claim_bits);
    while (events_.try_pop().has_value()) {}
    while (combat_events_.try_pop().has_value()) {}
    pending_save_.reset();
    if (room.lifecycle == checkpoint::RoomProgressLifecycle::death_pending) {
        clear_transient_room_state();
        phase_ = RoomPhase::death_pending;
        return true;
    }
    phase_ = room.full_clear ? RoomPhase::awaiting_exit : RoomPhase::combat;
    return true;
}

void DungeonSession::construct_current_room() noexcept {
    retry_health_potion_abyss_clear_before_combat_ = false;
    if (stable_state_.death.lifecycle
            == checkpoint::DeathLifecycle::pending_continue) {
        clear_transient_room_state();
        if (!validate_pending_death_state()) {
            enter_fault(DungeonFault::death_sequence_mismatch);
            return;
        }
        phase_ = RoomPhase::death_pending;
        return;
    }
    if (!checkpoint::valid_death_checkpoint_structural(
            stable_state_.death)) {
        clear_transient_room_state();
        enter_fault(DungeonFault::death_sequence_mismatch);
        return;
    }
    room_drop_state_.clear();
    rolled_drop_bits_ = {};
    rolled_material_bits_ = {};
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
    if (stable_state_.abyss.lifecycle == abyss::AbyssLifecycle::started) {
        construct_started_abyss_room();
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

bool DungeonSession::validate_pending_death_state() const noexcept {
    const auto& state = stable_state_;
    const auto& death = state.death;
    if (state.commit_generation < 2U || state.death_sequence == 0U
            || state.current_room.is_abyss
            || static_cast<std::uint8_t>(state.current_room.entry)
                > static_cast<std::uint8_t>(EntrySide::right)
            || state.biases != std::array<std::uint32_t, 4>{}
            || state.last_transition != TransitionKind::death_retreat
            || state.last_direction != ExitDirection::none
            || death.death_depth != state.current_room.depth
            || death.death_floor_room_index
                != state.current_room.floor_room_index
            || death.death_ecology != state.current_room.ecology
            || (death.source_kind
                    == checkpoint::DeathSourceKind::abyss_environment
                && !death.death_was_abyss)
            || !valid_last_abyss_resolution(
                state.last_abyss_resolution)) {
        return false;
    }

    checkpoint::RoomDescriptor death_anchor = state.current_room;
    death_anchor.is_abyss = death.death_was_abyss;
    if (!valid_death_checkpoint_dungeon(
            death, death_anchor, state.commit_generation - 1U,
            state.death_sequence, rules_)) {
        return false;
    }

    if (!death.death_was_abyss) {
        return canonical_empty_abyss(state.abyss)
            || valid_failed_abyss(state);
    }
    if (!valid_failed_abyss(state)) return false;
    const std::uint8_t total = abyss::reward_profile_for(
        state.abyss.danger, 1U).item_count;
    return state.last_abyss_resolution.valid
        && state.last_abyss_resolution.room_seed
            == state.current_room.seed
        && state.last_abyss_resolution.rule == state.abyss.rule
        && state.last_abyss_resolution.total == total
        && state.last_abyss_resolution.generated == 0U
        && state.last_abyss_resolution.claimed == 0U
        && state.last_abyss_resolution.abandoned == total
        && state.last_abyss_resolution.lifecycle
            == abyss::AbyssLifecycle::failed;
}

void DungeonSession::clear_transient_room_state() noexcept {
    retry_health_potion_abyss_clear_before_combat_ = false;
    combat_.reset();
    room_environment_.reset();
    clear_staged_room_population();
    room_progress_ = {};
    room_drop_state_.clear();
    rolled_drop_bits_ = {};
    rolled_material_bits_ = {};
    encounter_plan_ = {};
    wave_index_ = 0U;
    wave_delay_ticks_ = 0U;
    pending_save_.reset();
    pending_item_build_.reset();
    pending_abyss_combat_.reset();
    pending_abyss_reward_.reset();
    clear_abyss_exit_confirmation();
    room_progression_ = stable_state_.progression;
    pending_room_experience_ = 0U;
    last_room_experience_ = 0U;
    last_levels_gained_ = 0U;
    death_continue_failed_ = false;
    death_detected_emitted_ = false;
    last_passive_tree_error_ = passives::PassiveTreeError::none;
}

void DungeonSession::construct_cleared_abyss_room() noexcept {
    if (!checkpoint::valid_abyss_door_origin(stable_state_,
            abyss::is_abyss_roll(stable_state_.current_room.seed))
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
        stable_state_.current_room.depth,
        empty_wave,
        true,
        abyss::combat_config_for(abyss::AbyssRuleId::none),
        player_build.build,
        evasion_stream.next_u64(),
        stable_state_.current_room.ecology == checkpoint::DungeonElement::fire);
    if (!navigation_config.has_value()) {
        enter_fault(DungeonFault::invalid_rules);
        return;
    }
    if (!stage_current_room_population(*navigation_config)
            || !activate_staged_room_population(false)) {
        return;
    }
    combat::RoomMonsterField* const field = combat_->room_monster_field();
    if (field == nullptr) {
        enter_fault(DungeonFault::invalid_monster_plan);
        return;
    }
    room_progress_.defeated_monster_count =
        room_progress_.initial_monster_count;
    room_progress_.exits_unlocked = true;
    room_progress_.full_clear = true;
    for (std::uint32_t ordinal = 0U;
            ordinal < room_progress_.initial_monster_count; ++ordinal) {
        room_progress_.defeated_monster_bits[ordinal / 64U] |=
            std::uint64_t{1U} << (ordinal % 64U);
        if (!field->mark_defeated(
                static_cast<combat::MonsterOrdinal>(ordinal))) {
            enter_fault(DungeonFault::invalid_monster_plan);
            return;
        }
    }
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
                if (!ground_items_[index].active
                        && !room_drop_state_.equipment_claimed(index)) {
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

        if (!room_drop_state_.place_equipment(*expected)) {
            enter_fault(DungeonFault::population_capacity);
            return;
        }
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
        if (!ground_items_[index].active
                && !room_drop_state_.equipment_claimed(index)) {
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
    const auto player_build = build_for(stable_state_);
    if (player_build.status != PlayerBuildStatus::valid) {
        enter_fault(DungeonFault::invalid_item_state);
        return;
    }
    auto evasion_stream = core::DeterministicRng::derive_stream(
        stable_state_.current_room.seed, kPlayerEvasionSeedDomain);
    const combat::EncounterWave empty_wave{};
    const auto config = make_combat_encounter_config(
        stable_state_.current_room.entry,
        rules_.rules_version,
        stable_state_.current_room.depth,
        empty_wave,
        true,
        abyss::combat_config_for(abyss::AbyssRuleId::none),
        player_build.build,
        evasion_stream.next_u64(),
        stable_state_.current_room.ecology == checkpoint::DungeonElement::fire);
    if (!config.has_value()) {
        enter_fault(DungeonFault::invalid_rules);
        return;
    }
    if (!stage_current_room_population(*config)
            || !activate_staged_room_population()) {
        return;
    }
    phase_ = RoomPhase::locked;
}

void DungeonSession::construct_started_abyss_room() noexcept {
    if (!stable_state_.current_room.is_abyss) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return;
    }
    const auto selection = abyss::select_abyss_rule(
        stable_state_.current_room.seed, stable_state_.current_room.depth);
    if (!selection.has_value()
            || selection->danger != stable_state_.abyss.danger
            || selection->rule != stable_state_.abyss.rule
            || selection->rules_version != stable_state_.abyss.rules_version) {
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
    const auto config = make_combat_encounter_config(
        stable_state_.current_room.entry,
        rules_.rules_version,
        stable_state_.current_room.depth,
        empty_wave,
        true,
        abyss::combat_config_for(selection->rule),
        player_build.build,
        evasion_stream.next_u64(),
        stable_state_.current_room.ecology
            == checkpoint::DungeonElement::fire);
    if (!config.has_value() || !stage_current_room_population(*config)
            || !activate_staged_room_population()) {
        if (phase_ != RoomPhase::faulted) {
            enter_fault(DungeonFault::abyss_generation_failed);
        }
        return;
    }
    phase_ = RoomPhase::locked;
}

bool DungeonSession::stage_current_room_population(
    combat::CombatEncounterConfig config) noexcept {
    clear_staged_room_population();
    auto monster_field = std::unique_ptr<combat::RoomMonsterField>{
        new (std::nothrow) combat::RoomMonsterField{}};
    auto environment =
        std::unique_ptr<combat::RoomEnvironmentBlueprint>{
            new (std::nothrow) combat::RoomEnvironmentBlueprint{}};
    if (monster_field == nullptr || environment == nullptr) {
        enter_fault(DungeonFault::population_capacity);
        return false;
    }

    const RoomMonsterPlanBuildResult population = build_room_monster_plan(
        stable_state_.current_room, rules_, kRoomMonsterGeneratorVersion,
        monster_field->plan_storage_for_construction());
    if (population.fault != DungeonFault::none) {
        enter_fault(population.fault);
        return false;
    }
    if (monster_field->seal_plan(population)
            != combat::RoomMonsterFieldFault::none) {
        enter_fault(DungeonFault::invalid_monster_plan);
        return false;
    }

    const RoomEnvironmentBuildResult environment_result =
        build_room_environment(
            stable_state_.current_room, rules_,
            kRoomEnvironmentGeneratorVersion, monster_field->plan(),
            *environment);
    if (environment_result.fault != DungeonFault::none) {
        enter_fault(environment_result.fault);
        return false;
    }

    staged_room_progress_ = {};
    staged_room_progress_.initial_monster_count =
        monster_field->total_count();
    staged_room_progress_.required_kills = required_kills(
        staged_room_progress_.initial_monster_count);
    staged_room_combat_ = config;
    staged_room_monster_field_ = std::move(monster_field);
    staged_room_environment_ = std::move(environment);
    return true;
}

bool DungeonSession::activate_staged_room_population(
    const bool publish_population_event) noexcept {
    if (!staged_room_combat_.has_value()
            || staged_room_monster_field_ == nullptr
            || staged_room_environment_ == nullptr
            || staged_room_progress_.initial_monster_count == 0U) {
        clear_staged_room_population();
        enter_fault(DungeonFault::invalid_monster_plan);
        return false;
    }

    if (!room_drop_state_.reset(staged_room_monster_field_->plan())) {
        clear_staged_room_population();
        enter_fault(DungeonFault::population_capacity);
        return false;
    }
    combat_.reset();
    room_environment_.reset();
    room_environment_ = std::move(staged_room_environment_);
    combat_.emplace(*staged_room_combat_,
        std::move(staged_room_monster_field_),
        combat::room_obstacle_plan_view(*room_environment_));
    staged_room_combat_.reset();
    if (combat_->fault() != combat::CombatFault::none) {
        const DungeonFault fault = combat_->fault()
                == combat::CombatFault::invalid_obstacle_plan
            ? DungeonFault::environment_navigation
            : DungeonFault::invalid_monster_plan;
        combat_.reset();
        room_environment_.reset();
        staged_room_progress_ = {};
        enter_fault(fault);
        return false;
    }
    advance_room_instance_generation();

    room_progress_ = staged_room_progress_;
    staged_room_progress_ = {};
    encounter_plan_ = {};
    encounter_plan_.wave_count = 1U;
    wave_index_ = 0U;
    wave_delay_ticks_ = 0U;
    if (publish_population_event
            && !emit(DungeonEventKind::population_generated)) {
        return false;
    }
    return true;
}

void DungeonSession::advance_room_instance_generation() noexcept {
    room_instance_generation_ = room_instance_generation_
            == (std::numeric_limits<std::uint64_t>::max)()
        ? 1U : room_instance_generation_ + 1U;
}

void DungeonSession::clear_staged_room_population() noexcept {
    staged_room_combat_.reset();
    staged_room_monster_field_.reset();
    staged_room_environment_.reset();
    staged_room_progress_ = {};
}

void DungeonSession::reset_to_normal_room(bool clear_queues) noexcept {
    retry_health_potion_abyss_clear_before_combat_ = false;
    if (clear_queues) {
        while (events_.try_pop().has_value()) {
        }
        while (combat_events_.try_pop().has_value()) {
        }
    }
    combat_.reset();
    room_environment_.reset();
    room_drop_state_.clear();
    clear_staged_room_population();
    room_progress_ = {};
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
            || !checkpoint::valid_abyss_door_origin(stable_state_,
                abyss::is_abyss_roll(stable_state_.current_room.seed))
            || stable_state_.abyss.lifecycle
                != abyss::AbyssLifecycle::available) {
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
    const combat::EncounterWave empty_wave{};
    const auto combat_config = make_combat_encounter_config(
        stable_state_.current_room.entry,
        rules_.rules_version,
        stable_state_.current_room.depth,
        empty_wave,
        true,
        abyss::combat_config_for(selection->rule),
        player_build.build,
        evasion_stream.next_u64(),
        stable_state_.current_room.ecology == checkpoint::DungeonElement::fire);
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
        if (!stage_current_room_population(*combat_config)) return false;
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
        clear_staged_room_population();
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
        if (!apply_abyss_failure_resolution(next, stable_state_)) {
            enter_fault(DungeonFault::invalid_abyss_state);
            return RequestResult::faulted;
        }
        next.item_ownership.material_claimed_drop_bits = {};
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

void DungeonSession::relay_combat_defeats() noexcept {
    if (!combat_.has_value()) {
        return;
    }
    const combat::CombatFault combat_fault = combat_->fault();
    if (combat_fault == combat::CombatFault::defeat_ledger_overflow) {
        enter_fault(DungeonFault::defeat_ledger_overflow);
        return;
    }
    if (combat_fault != combat::CombatFault::none) {
        enter_fault(combat_fault == combat::CombatFault::invalid_obstacle_plan
            ? DungeonFault::environment_navigation
            : DungeonFault::invalid_monster_plan);
        return;
    }

    while (const auto defeat = combat_->try_pop_defeat_record()) {
        const std::uint32_t ordinal = defeat->monster_ordinal;
        if (ordinal >= room_progress_.initial_monster_count
                || ordinal >= limits::kRoomMonsterCapacity) {
            enter_fault(DungeonFault::invalid_monster_plan);
            return;
        }
        const std::size_t word = ordinal / 64U;
        const std::uint64_t mask = std::uint64_t{1U} << (ordinal % 64U);
        if ((room_progress_.defeated_monster_bits[word] & mask) != 0U) {
            enter_fault(DungeonFault::invalid_monster_plan);
            return;
        }
        room_progress_.defeated_monster_bits[word] |= mask;
        ++room_progress_.defeated_monster_count;
        if (defeat->reward_eligible) {
            const std::size_t monster_index = static_cast<std::size_t>(
                defeat->monster_id);
            if (monster_index
                    < progression_rules_.monster_experience.size()) {
                saturating_add(pending_room_experience_,
                    affix_experience(
                        progression_rules_.monster_experience[monster_index],
                        defeat->affix_score));
            }
        }
    }
    const combat::RoomMonsterField* field = combat_->room_monster_field();
    if (field == nullptr
            && room_progress_.initial_monster_count == 0U) return;
    if (field == nullptr
            || field->defeated_count()
                != room_progress_.defeated_monster_count) {
        enter_fault(DungeonFault::invalid_monster_plan);
    }
}

void DungeonSession::relay_combat_events() noexcept {
    if (!combat_.has_value()) return;
    relay_combat_defeats();
    if (phase_ == RoomPhase::faulted) return;
    while (auto event = combat_->try_pop_event()) {
        if (event->kind == combat::CombatEventKind::defeated
                && event->reward_eligible) {
            roll_ground_materials(*event);
            if (claim_defeat_reward(*event)) {
                roll_ground_drop(*event);
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
    static_cast<void>(prepare_death_retreat());
}

bool DungeonSession::prepare_death_retreat() noexcept {
    const bool abyss_death = stable_state_.current_room.is_abyss
        && stable_state_.abyss.lifecycle == abyss::AbyssLifecycle::started;
    if (phase_ != RoomPhase::combat || pending_save_.has_value()
            || !combat_.has_value()
            || !combat_->death_snapshot().has_value()
            || (stable_state_.current_room.is_abyss && !abyss_death)
            || stable_state_.death.lifecycle
                != checkpoint::DeathLifecycle::none
            || !checkpoint::valid_death_checkpoint_structural(
                stable_state_.death)) {
        enter_fault(DungeonFault::death_sequence_mismatch);
        return false;
    }
    if (stable_state_.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return false;
    }
    if (stable_state_.death_sequence
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::death_sequence_overflow);
        return false;
    }
    if (stable_state_.current_room.index
            == (std::numeric_limits<std::uint64_t>::max)()) {
        diagnostics_.room_index_overflow = true;
        enter_fault(DungeonFault::room_index_overflow);
        return false;
    }
    const std::size_t required_event_slots = death_detected_emitted_ ? 1U : 2U;
    if (!can_emit(required_event_slots)) {
        enter_fault(DungeonFault::event_overflow);
        return false;
    }

    const combat::CombatDeathSnapshot frozen = *combat_->death_snapshot();
    const std::uint64_t next_sequence = stable_state_.death_sequence + 1U;
    const DeathRetreatTargetResult target = make_death_retreat_target(
        stable_state_, next_sequence, rules_);
    if (target.fault != DungeonFault::none) {
        enter_fault(target.fault);
        return false;
    }

    PendingSave& pending = pending_save_.prepare();
    if (!copy_run_state_reusing_items(pending.next_state, stable_state_)) {
        pending_save_.reset();
        enter_fault(DungeonFault::invalid_item_state);
        return false;
    }
    DungeonRunState& next = pending.next_state;
    ++next.commit_generation;
    next.death_sequence = next_sequence;
    next.biases = {};
    if (abyss_death
            && !apply_abyss_failure_resolution(next, stable_state_)) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return false;
    }
    next.current_room.is_abyss = false;
    next.last_transition = TransitionKind::death_retreat;
    next.last_direction = ExitDirection::none;
    next.item_ownership.material_claimed_drop_bits = {};
    next.death = make_death_checkpoint(
        frozen, stable_state_.current_room, target.room);
    if (!valid_death_checkpoint_dungeon(
            next.death, stable_state_.current_room,
            stable_state_.commit_generation, next_sequence, rules_)) {
        pending_save_.reset();
        enter_fault(DungeonFault::death_sequence_mismatch);
        return false;
    }
    pending.kind = PendingSaveKind::death_retreat;
    pending.expected_generation = next.commit_generation;
    pending.transition = TransitionKind::death_retreat;
    pending.direction = ExitDirection::none;
    pending.resume_phase = RoomPhase::combat;
    pending.pickup_ordinal = 0xFFFFU;
    pending.death_snapshot = frozen;
    phase_ = RoomPhase::committing;
    if (!death_detected_emitted_) {
        if (!emit(DungeonEventKind::death_detected,
                &stable_state_, &pending_save_->next_state,
                TransitionKind::death_retreat, ExitDirection::none)) {
            pending_save_.reset();
            return false;
        }
        death_detected_emitted_ = true;
    }
    return true;
}

bool DungeonSession::build_death_continue_next(
    DungeonRunState& next) const noexcept {
    if (stable_state_.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()
            || stable_state_.death.lifecycle
                != checkpoint::DeathLifecycle::pending_continue
            || !validate_pending_death_state()) {
        return false;
    }
    if (!copy_run_state_reusing_items(next, stable_state_)) return false;
    ++next.commit_generation;
    next.current_room = stable_state_.death.target_room;
    next.abyss = {};
    next.death = {};
    return checkpoint::valid_death_checkpoint_structural(next.death);
}

bool DungeonSession::copy_run_state_reusing_items(
    DungeonRunState& destination,
    const DungeonRunState& source) noexcept {
    auto& destination_items = destination.item_ownership.items;
    const auto& source_items = source.item_ownership.items;
    if (destination_items.capacity() < source_items.size()) return false;
    destination.root_seed = source.root_seed;
    destination.commit_generation = source.commit_generation;
    destination.biases = source.biases;
    destination.current_room = source.current_room;
    destination.abyss = source.abyss;
    destination.last_abyss_resolution = source.last_abyss_resolution;
    destination.last_transition = source.last_transition;
    destination.last_direction = source.last_direction;
    destination.progression = source.progression;
    destination.passive_tree = source.passive_tree;
    destination.skill_loadout = source.skill_loadout;
    if (source_items.empty()) destination_items.clear();
    else destination_items.assign(source_items.begin(), source_items.end());
    destination.item_ownership.equipment = source.item_ownership.equipment;
    destination.item_ownership.materials = source.item_ownership.materials;
    destination.item_ownership.material_discovery_bits =
        source.item_ownership.material_discovery_bits;
    destination.item_ownership.material_claimed_drop_bits =
        source.item_ownership.material_claimed_drop_bits;
    destination.item_ownership.claimed_drop_bits =
        source.item_ownership.claimed_drop_bits;
    destination.item_ownership.next_item_sequence =
        source.item_ownership.next_item_sequence;
    destination.death_sequence = source.death_sequence;
    destination.death = source.death;
    return true;
}

void DungeonSession::publish_run_state_reusing_items(
    DungeonRunState& destination,
    DungeonRunState& source) noexcept {
    destination.item_ownership.items.swap(source.item_ownership.items);
    destination.root_seed = source.root_seed;
    destination.commit_generation = source.commit_generation;
    destination.biases = source.biases;
    destination.current_room = source.current_room;
    destination.abyss = source.abyss;
    destination.last_abyss_resolution = source.last_abyss_resolution;
    destination.last_transition = source.last_transition;
    destination.last_direction = source.last_direction;
    destination.progression = source.progression;
    destination.passive_tree = source.passive_tree;
    destination.skill_loadout = source.skill_loadout;
    destination.item_ownership.equipment = source.item_ownership.equipment;
    destination.item_ownership.materials = source.item_ownership.materials;
    destination.item_ownership.material_discovery_bits =
        source.item_ownership.material_discovery_bits;
    destination.item_ownership.material_claimed_drop_bits =
        source.item_ownership.material_claimed_drop_bits;
    destination.item_ownership.claimed_drop_bits =
        source.item_ownership.claimed_drop_bits;
    destination.item_ownership.next_item_sequence =
        source.item_ownership.next_item_sequence;
    destination.death_sequence = source.death_sequence;
    destination.death = source.death;
}

RequestResult DungeonSession::request_death_continue() noexcept {
    if (health_potion_abyss_clear_retry_gate_active()) {
        return RequestResult::rejected;
    }
    if (phase_ == RoomPhase::faulted) return RequestResult::faulted;
    if (phase_ != RoomPhase::death_pending || pending_save_.has_value()
            || stable_state_.death.lifecycle
                != checkpoint::DeathLifecycle::pending_continue) {
        return RequestResult::rejected;
    }
    if (stable_state_.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return RequestResult::faulted;
    }
    if (!validate_pending_death_state()) {
        enter_fault(DungeonFault::death_sequence_mismatch);
        return RequestResult::faulted;
    }
    if (!can_emit(2U)) {
        enter_fault(DungeonFault::event_overflow);
        return RequestResult::faulted;
    }

    PendingSave& pending = pending_save_.prepare();
    if (!build_death_continue_next(pending.next_state)) {
        pending_save_.reset();
        enter_fault(DungeonFault::invalid_item_state);
        return RequestResult::faulted;
    }
    pending.kind = PendingSaveKind::death_continue;
    pending.expected_generation = pending.next_state.commit_generation;
    pending.transition = TransitionKind::death_retreat;
    pending.direction = ExitDirection::none;
    pending.resume_phase = RoomPhase::death_pending;
    pending.pickup_ordinal = 0xFFFFU;
    pending.death_snapshot.reset();
    death_continue_failed_ = false;
    phase_ = RoomPhase::committing;
    if (!emit(DungeonEventKind::death_continue_requested,
            &stable_state_, &pending_save_->next_state,
            TransitionKind::death_retreat, ExitDirection::none)) {
        pending_save_.reset();
        return RequestResult::faulted;
    }
    return RequestResult::accepted;
}

bool DungeonSession::pending_death_cache_consistent() const noexcept {
    if (!pending_save_.has_value()) return true;
    if (pending_save_->kind == PendingSaveKind::death_continue) {
        if (pending_save_->death_snapshot.has_value()
                || pending_save_->transition != TransitionKind::death_retreat
                || pending_save_->direction != ExitDirection::none
                || pending_save_->resume_phase != RoomPhase::death_pending
                || pending_save_->pickup_ordinal != 0xFFFFU) {
            return false;
        }
        if (combat_.has_value()) {
            if (!combat_->death_snapshot().has_value()) return false;
            checkpoint::RoomDescriptor death_room = stable_state_.current_room;
            death_room.is_abyss = stable_state_.death.death_was_abyss;
            const checkpoint::DeathCheckpoint expected_death =
                make_death_checkpoint(*combat_->death_snapshot(), death_room,
                    stable_state_.death.target_room);
            if (!same_death_checkpoint(expected_death, stable_state_.death)) {
                return false;
            }
        }
        DungeonRunState& exact = death_validation_scratch_;
        return build_death_continue_next(exact)
            && pending_save_->expected_generation
                == exact.commit_generation
            && same_run_state(pending_save_->next_state, exact);
    }
    const bool death_pending = pending_save_->kind
        == PendingSaveKind::death_retreat;
    if (!death_pending) {
        return !pending_save_.has_value()
            || !pending_save_->death_snapshot.has_value();
    }
    if (!pending_save_->death_snapshot.has_value()) return false;
    if (!combat_.has_value() || !combat_->death_snapshot().has_value()
            || !same_combat_death_snapshot(
                *pending_save_->death_snapshot, *combat_->death_snapshot())) {
        return false;
    }
    const auto& next = pending_save_->next_state;
    const checkpoint::DeathCheckpoint expected = make_death_checkpoint(
        *pending_save_->death_snapshot, stable_state_.current_room,
        next.death.target_room);
    if (!same_death_checkpoint(expected, next.death)
            || stable_state_.death_sequence
                == (std::numeric_limits<std::uint64_t>::max)()
            || stable_state_.commit_generation
                == (std::numeric_limits<std::uint64_t>::max)()) {
        return false;
    }
    DungeonRunState& exact = death_validation_scratch_;
    if (!copy_run_state_reusing_items(exact, stable_state_)) return false;
    ++exact.commit_generation;
    ++exact.death_sequence;
    exact.biases = {};
    if (stable_state_.current_room.is_abyss
            && !apply_abyss_failure_resolution(exact, stable_state_)) {
        return false;
    }
    exact.current_room.is_abyss = false;
    exact.last_transition = TransitionKind::death_retreat;
    exact.last_direction = ExitDirection::none;
    exact.item_ownership.material_claimed_drop_bits = {};
    exact.death = expected;
    return same_run_state(next, exact)
        && valid_death_checkpoint_dungeon(
            next.death, stable_state_.current_room,
            stable_state_.commit_generation, next.death_sequence, rules_);
}

bool DungeonSession::claim_defeat_reward(
    const combat::CombatEvent& event) noexcept {
    const std::uint16_t ordinal = event.spawn_ordinal;
    if (ordinal >= kAuthoritativeEquipmentDropCapacity
            || room_drop_state_.equipment_claimed(ordinal)
            || (legacy_equipment_claim_representable(ordinal)
                && bit_is_set(stable_state_.item_ownership.claimed_drop_bits,
                    ordinal))
            || bit_is_set(rolled_drop_bits_, ordinal)) {
        return false;
    }
    set_bit(rolled_drop_bits_, ordinal);
    return true;
}

void DungeonSession::roll_ground_drop(
    const combat::CombatEvent& event) noexcept {
    const std::uint16_t ordinal = event.spawn_ordinal;
    if (ordinal >= kAuthoritativeEquipmentDropCapacity) return;

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
    const GroundItem ground{
        true,
        ordinal,
        GroundItemSource::monster_drop,
        0xFFU,
        {event.position.x, event.position.y, 0.0F},
        *generated,
    };
    if (!room_drop_state_.place_equipment(ground)) {
        enter_fault(DungeonFault::population_capacity);
    }
}

bool DungeonSession::claim_material_roll(std::uint16_t ordinal) noexcept {
    if (ordinal >= kAuthoritativeSecondaryDropCapacity
            || room_drop_state_.secondary_claimed(ordinal)
            || (legacy_secondary_claim_representable(ordinal)
                && bit_is_set(stable_state_.item_ownership
                        .material_claimed_drop_bits,
                    ordinal))
            || bit_is_set(rolled_material_bits_, ordinal)) {
        return false;
    }
    set_bit(rolled_material_bits_, ordinal);
    return true;
}

bool DungeonSession::place_ground_material(
    std::uint16_t ordinal,
    GroundMaterialSource source,
    combat::Vec3 position,
    items::MaterialId material) noexcept {
    if (ordinal >= ground_materials_.size()
            || items::material_index(material) >= items::kMaterialCount) {
        enter_fault(DungeonFault::invalid_item_state);
        return false;
    }
    if (ground_materials_[ordinal].active) {
        saturating_increment(diagnostics_.material_ground_saturation_count);
        return false;
    }
    const GroundMaterial ground{true, ordinal, source, position, material};
    if (!room_drop_state_.place_material(ground)) {
        enter_fault(DungeonFault::population_capacity);
        return false;
    }
    return true;
}

bool DungeonSession::place_ground_health_potion(
    std::uint16_t spawn_ordinal, combat::Vec3 position) noexcept {
    if (spawn_ordinal >= ground_health_potions_.size()) return false;
    GroundHealthPotion& slot = ground_health_potions_[spawn_ordinal];
    if (slot.active) {
        saturating_increment(
            diagnostics_.health_potion_ground_saturation_count);
        return false;
    }
    const GroundHealthPotion potion{true, spawn_ordinal,
        health_potion_claim_ordinal(spawn_ordinal), position};
    if (!room_drop_state_.place_health_potion(potion)) {
        enter_fault(DungeonFault::population_capacity);
        return false;
    }
    return true;
}

bool DungeonSession::has_claimable_health_potion() const noexcept {
    if (!combat_.has_value() || combat_->death_snapshot().has_value()) {
        return false;
    }
    const combat::CombatSnapshot snapshot = combat_->snapshot();
    if (!health_potion_auto_use_eligible(
            snapshot.player.hp, snapshot.player.max_hp)) {
        return false;
    }
    for (const GroundHealthPotion& ground : ground_health_potions_) {
        if (ground.active) return true;
    }
    return false;
}

bool DungeonSession::append_clear_health_potion_claims(
    PendingSave& pending) noexcept {
    if ((pending.kind != PendingSaveKind::room_clear
            && pending.kind != PendingSaveKind::abyss_clear)
            || !combat_.has_value()) {
        return false;
    }
    pending.health_potion_claim.reset();
    if (combat_->death_snapshot().has_value()) return true;
    const combat::CombatSnapshot snapshot = combat_->snapshot();
    if (!health_potion_auto_use_eligible(
            snapshot.player.hp, snapshot.player.max_hp)) {
        return true;
    }

    PendingHealthPotionClaim claim{};
    claim.expected_hp = snapshot.player.hp;
    claim.expected_max_hp = snapshot.player.max_hp;
    int projected_hp = snapshot.player.hp;
    const int restore = combat::scale_basis_points(
        snapshot.player.max_hp, kHealthPotionRestoreBp,
        combat::BasisPointRounding::ceil);
    for (std::uint16_t spawn = 0U;
         spawn < ground_health_potions_.size(); ++spawn) {
        const GroundHealthPotion& ground = ground_health_potions_[spawn];
        if (!ground.active) continue;
        const std::uint16_t claim_ordinal =
            health_potion_claim_ordinal(spawn);
        if (ground.spawn_ordinal != spawn
                || ground.claim_ordinal != claim_ordinal
                || room_drop_state_.secondary_claimed(claim_ordinal)
                || (legacy_secondary_claim_representable(claim_ordinal)
                    && bit_is_set(stable_state_.item_ownership
                            .material_claimed_drop_bits,
                        claim_ordinal))) {
            return false;
        }
        claim.spawn_ordinals[claim.count++] = spawn;
        if (legacy_secondary_claim_representable(claim_ordinal)) {
            set_bit(pending.next_state.item_ownership
                    .material_claimed_drop_bits,
                claim_ordinal);
        }
        projected_hp = (std::min)(
            snapshot.player.max_hp, projected_hp + restore);
        const bool strictly_above_threshold =
            static_cast<std::int64_t>(projected_hp) * 10000
                > static_cast<std::int64_t>(snapshot.player.max_hp)
                    * kHealthPotionAutoUseThresholdBp;
        if (strictly_above_threshold
                || claim.count == kPendingHealthPotionClaimCapacity) {
            break;
        }
    }
    if (claim.count != 0U) pending.health_potion_claim = claim;
    return true;
}

void DungeonSession::roll_ground_materials(
    const combat::CombatEvent& event) noexcept {
    const std::uint16_t spawn_ordinal = event.spawn_ordinal;
    if (spawn_ordinal >= kAuthoritativeEquipmentDropCapacity) return;
    const std::uint16_t common_ordinal = static_cast<std::uint16_t>(
        spawn_ordinal * 2U);
    const std::uint16_t coupon_ordinal = static_cast<std::uint16_t>(
        common_ordinal + 1U);
    const combat::Vec3 position{event.position.x, event.position.y, 0.0F};
    if (claim_material_roll(common_ordinal)) {
        const auto common = roll_material_drop(
            stable_state_.current_room.seed,
            spawn_ordinal,
            stable_state_.current_room.depth,
            event.affix_score);
        if (common.has_value()) {
            static_cast<void>(place_ground_material(
                common_ordinal, GroundMaterialSource::monster_common,
                position, *common));
        }
    }
    if (phase_ == RoomPhase::faulted) return;
    if (claim_material_roll(coupon_ordinal)) {
        const bool abyss_monster = stable_state_.current_room.is_abyss
            && stable_state_.abyss.lifecycle
                == abyss::AbyssLifecycle::started;
        const auto coupon = roll_coupon_drop(
            stable_state_.current_room.seed,
            spawn_ordinal,
            stable_state_.current_room.depth,
            event.affix_score,
            abyss_monster);
        if (coupon.has_value()) {
            static_cast<void>(place_ground_material(
                coupon_ordinal, GroundMaterialSource::monster_coupon,
                position, *coupon));
        } else if (roll_health_potion_drop(
                stable_state_.current_room.seed, spawn_ordinal)) {
            static_cast<void>(place_ground_health_potion(
                spawn_ordinal, position));
        }
    }
}

bool DungeonSession::materialize_abyss_clear_materials() noexcept {
    const std::uint8_t count = abyss_material_reward_count(
        stable_state_.abyss.danger);
    if (count == 0U || count > 3U) return false;
    for (std::uint8_t index = 0U; index < count; ++index) {
        const std::uint16_t ordinal = static_cast<std::uint16_t>(
            kAbyssSecondaryOrdinalBegin + index);
        if (!claim_material_roll(ordinal)) {
            if (room_drop_state_.secondary_claimed(ordinal)) {
                continue;
            }
            const auto expected_material = roll_abyss_material(
                stable_state_.current_room.seed,
                stable_state_.current_room.depth,
                index);
            const auto expected_position = abyss_reward_position(index);
            const GroundMaterial& ground = ground_materials_[ordinal];
            if (!expected_material.has_value()
                    || !expected_position.has_value()
                    || !ground.active
                    || ground.ordinal != ordinal
                    || ground.source != GroundMaterialSource::abyss_reward
                    || ground.material != *expected_material
                    || ground.position.x != expected_position->x
                    || ground.position.y != expected_position->y
                    || ground.position.z != expected_position->z) {
                return false;
            }
            continue;
        }
        const auto material = roll_abyss_material(
            stable_state_.current_room.seed,
            stable_state_.current_room.depth,
            index);
        const auto position = abyss_reward_position(index);
        if (!material.has_value() || !position.has_value()
                || !place_ground_material(
                    ordinal, GroundMaterialSource::abyss_reward,
                    *position, *material)) {
            return false;
        }
    }
    return true;
}

bool DungeonSession::has_ground_materials() const noexcept {
    for (const GroundMaterial& material : ground_materials_) {
        if (material.active) return true;
    }
    return false;
}

void DungeonSession::vacuum_room_materials() noexcept {
    if (!pending_save_.has_value()
            || (pending_save_->kind != PendingSaveKind::room_clear
                && pending_save_->kind != PendingSaveKind::abyss_clear)) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    auto& ownership = pending_save_->next_state.item_ownership;
    for (const GroundMaterial& ground : ground_materials_) {
        if (!ground.active) continue;
        const std::size_t material_index = items::material_index(
            ground.material);
        if (ground.ordinal >= kAuthoritativeSecondaryDropCapacity
                || material_index >= items::kMaterialCount
                || room_drop_state_.secondary_claimed(ground.ordinal)
                || ownership.materials[material_index]
                    == (std::numeric_limits<std::uint64_t>::max)()) {
            pending_save_.reset();
            enter_fault(DungeonFault::invalid_item_state);
            return;
        }
        ++ownership.materials[material_index];
        ownership.material_discovery_bits |= static_cast<std::uint16_t>(
            std::uint16_t{1U} << material_index);
        if (legacy_secondary_claim_representable(ground.ordinal)) {
            set_bit(ownership.material_claimed_drop_bits, ground.ordinal);
        }
    }
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
    if (!combat_.has_value() || pending_save_.has_value()
            || !room_progress_.exits_unlocked
            || room_progress_.initial_monster_count == 0U
            || room_progress_.defeated_monster_count
                != room_progress_.initial_monster_count
            || remaining_targets() != 0U) {
        enter_fault(started_abyss
            ? DungeonFault::invalid_abyss_state
            : DungeonFault::invalid_item_state);
        return;
    }
    constexpr std::size_t kClearPublicationEventCount = 1U;
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

    abyss::AbyssRewardProfile reward{};
    if (started_abyss) {
        const std::uint8_t base_item_level = static_cast<std::uint8_t>(
            std::min<std::uint64_t>(
                stable_state_.current_room.depth, 100U));
        reward = abyss::reward_profile_for(
            stable_state_.abyss.danger, base_item_level);
        if (reward.item_count == 0U || reward.item_count > 3U
                || !materialize_abyss_clear_materials()) {
            enter_fault(DungeonFault::invalid_abyss_state);
            return;
        }
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
        if (started_abyss) {
            next.abyss.lifecycle = abyss::AbyssLifecycle::cleared;
            next.abyss.reward_total = reward.item_count;
            next.abyss.generated_mask = 0U;
            next.abyss.claimed_mask = 0U;
            next.abyss.abandoned_mask = 0U;
            next.abyss.reward_revision = 0U;
        }
        pending_save_ = PendingSave{
            started_abyss ? PendingSaveKind::abyss_clear
                          : PendingSaveKind::room_clear,
            next.commit_generation,
            std::move(next),
            TransitionKind::none,
            ExitDirection::none,
            started_abyss ? RoomPhase::cleared : RoomPhase::combat,
        };
        vacuum_room_materials();
        if (phase_ != RoomPhase::faulted && pending_save_.has_value()) {
            freeze_pending_material_pickup_receipt(true);
        }
        if (phase_ != RoomPhase::faulted && pending_save_.has_value()
                && !append_clear_health_potion_claims(*pending_save_)) {
            pending_save_.reset();
            enter_fault(started_abyss
                ? DungeonFault::invalid_abyss_state
                : DungeonFault::invalid_item_state);
            return;
        }
        if (phase_ != RoomPhase::faulted && pending_save_.has_value()
                && !freeze_pending_health_potion_pickup_receipt(true)) {
            pending_save_.reset();
            enter_fault(started_abyss
                ? DungeonFault::invalid_abyss_state
                : DungeonFault::invalid_item_state);
            return;
        }
        if (started_abyss && pending_save_.has_value()
                && pending_save_->health_potion_claim.has_value()) {
            pending_save_->resume_phase = RoomPhase::combat;
        }
    } catch (...) {
        pending_save_.reset();
        enter_fault(started_abyss
            ? DungeonFault::invalid_abyss_state
            : DungeonFault::invalid_item_state);
        return;
    }
    if (phase_ == RoomPhase::faulted || !pending_save_.has_value()) return;
    phase_ = RoomPhase::committing;
}

void DungeonSession::freeze_pending_material_pickup_receipt(
    bool room_vacuum) noexcept {
    if (!pending_save_.has_value()) return;
    MaterialPickupReceipt& receipt =
        pending_save_->material_pickup_receipt;
    receipt = {};
    receipt.valid = true;
    receipt.room_vacuum = room_vacuum;
    receipt.commit_generation = pending_save_->next_state.commit_generation;
    for (std::size_t index = 0U; index < receipt.counts.size(); ++index) {
        receipt.counts[index] =
            pending_save_->next_state.item_ownership.materials[index]
            - stable_state_.item_ownership.materials[index];
    }
    for (std::uint16_t ordinal = 0U;
            ordinal < ground_materials_.size(); ++ordinal) {
        const GroundMaterial& ground = ground_materials_[ordinal];
        if (!ground.active || (!room_vacuum
                && ordinal != pending_save_->pickup_ordinal)) {
            continue;
        }
        const std::size_t material_index =
            items::material_index(ground.material);
        if (material_index >= receipt.counts.size()
                || receipt.counts[material_index] == 0U) {
            continue;
        }
        const std::uint16_t bit = static_cast<std::uint16_t>(
            std::uint16_t{1U} << material_index);
        if ((receipt.origin_valid_mask & bit) != 0U) continue;
        receipt.representative_origins[material_index] = ground.position;
        receipt.origin_valid_mask |= bit;
    }
}

bool DungeonSession::freeze_pending_health_potion_pickup_receipt(
    bool room_clear) noexcept {
    if (!pending_save_.has_value()) return false;
    HealthPotionPickupReceipt& receipt =
        pending_save_->health_potion_pickup_receipt;
    receipt = {};
    if (!pending_save_->health_potion_claim.has_value()) return true;
    const PendingHealthPotionClaim& claim =
        *pending_save_->health_potion_claim;
    if (claim.count == 0U || claim.count > receipt.sources.size()) {
        return false;
    }
    receipt.valid = true;
    receipt.room_clear = room_clear;
    receipt.consumed_count = claim.count;
    receipt.commit_generation = pending_save_->next_state.commit_generation;
    for (std::uint8_t index = 0U; index < claim.count; ++index) {
        const std::uint16_t spawn = claim.spawn_ordinals[index];
        if (spawn >= ground_health_potions_.size()) return false;
        const GroundHealthPotion& ground = ground_health_potions_[spawn];
        const std::uint16_t claim_ordinal =
            health_potion_claim_ordinal(spawn);
        if (!ground.active || ground.spawn_ordinal != spawn
                || ground.claim_ordinal != claim_ordinal
                || !std::isfinite(ground.position.x)
                || !std::isfinite(ground.position.y)
                || !std::isfinite(ground.position.z)) {
            return false;
        }
        receipt.sources[index] = {spawn, claim_ordinal, ground.position};
    }
    return true;
}

void DungeonSession::publish_room_clear() noexcept {
    room_progress_.exits_unlocked = true;
    room_progress_.full_clear = true;
    phase_ = RoomPhase::cleared;
    static_cast<void>(emit(DungeonEventKind::room_cleared));
}

bool DungeonSession::can_emit(std::size_t count) const noexcept {
    return count <= events_.capacity() - events_.size();
}

void DungeonSession::enter_fault(DungeonFault fault) noexcept {
    if (fault == DungeonFault::none) {
        return;
    }
    retry_health_potion_abyss_clear_before_combat_ = false;
    diagnostics_.fault = fault;
    phase_ = RoomPhase::faulted;
    clear_abyss_exit_confirmation();
    pending_item_build_.reset();
    pending_abyss_combat_.reset();
    clear_staged_room_population();
    if (!combat_.has_value()) room_environment_.reset();

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

std::uint32_t DungeonSession::remaining_targets() const noexcept {
    return room_progress_.defeated_monster_count
            <= room_progress_.initial_monster_count
        ? room_progress_.initial_monster_count
            - room_progress_.defeated_monster_count
        : 0U;
}

void DungeonSession::prepare_room_unlock() noexcept {
    if (phase_ != RoomPhase::combat || !combat_.has_value()
            || pending_save_.has_value() || room_progress_.exits_unlocked
            || room_progress_.required_kills == 0U
            || room_progress_.defeated_monster_count
                < room_progress_.required_kills) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    if (!can_emit(1U)) {
        saturating_increment(diagnostics_.event_overflow_count);
        enter_fault(DungeonFault::event_overflow);
        return;
    }
    if (stable_state_.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return;
    }
    try {
        DungeonRunState next = stable_state_;
        ++next.commit_generation;
        pending_save_ = PendingSave{
            PendingSaveKind::room_unlock,
            next.commit_generation,
            std::move(next),
            TransitionKind::none,
            ExitDirection::none,
            RoomPhase::combat,
        };
    } catch (...) {
        pending_save_.reset();
        enter_fault(DungeonFault::invalid_item_state);
        return;
    }
    phase_ = RoomPhase::committing;
}

}  // namespace arpg::dungeon
