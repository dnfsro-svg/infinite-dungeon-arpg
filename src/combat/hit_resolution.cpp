#include "combat/combat_world.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/combat_collision.hpp"
#include "combat/room_bounds.hpp"
#include "modifiers/damage_types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace arpg::combat {
namespace {

constexpr std::uint16_t kBreakWindowTicks = 180;
constexpr std::int32_t kBasisPoints = 10000;

int ceil_divide(std::int64_t numerator, std::int32_t divisor) noexcept {
    if (numerator <= 0 || divisor <= 0) return 0;
    return static_cast<int>((numerator + divisor - 1) / divisor);
}

std::uint16_t hit_stop_for(FeedbackLevel feedback) noexcept {
    switch (feedback) {
    case FeedbackLevel::light:
        return 3;
    case FeedbackLevel::medium:
        return 5;
    case FeedbackLevel::heavy:
        return 7;
    }
    return 0;
}

}  // namespace

void CombatWorld::emit_event(const CombatEvent& event) noexcept {
    if (!events_.try_push(event)) {
        ++event_overflow_count_;
    }
}

void CombatWorld::apply_attack_assist(
    const AttackDefinition& definition) noexcept {
    const Aabb attack_box = make_world_aabb(
        definition.local_hitbox, player_.position, player_.facing);
    const bool facing_right = player_.facing == Facing::right;
    float best_gap = std::numeric_limits<float>::max();
    float best_correction = 0.0F;

    for (std::size_t index = 0; index < monsters_.slots_.size(); ++index) {
        const MonsterRuntime& dummy = monsters_.slots_[index];
        const float relative_x = dummy.position.x - player_.position.x;
        if (dummy.hp <= 0
            || dummy.reaction == ReactionState::defeated
            || dummy.reaction == ReactionState::respawning
            || (facing_right && relative_x < 0.0F)
                || (!facing_right && relative_x > 0.0F)
                || std::fabs(dummy.position.y - player_.position.y)
                > kAttackAssistMaxDepth) {
            continue;
        }

        const Aabb hurtbox = make_dummy_hurtbox(dummy.kind, dummy.position);
        const float gap = facing_right
                              ? std::max(0.0F, hurtbox.minimum.x
                                                    - attack_box.maximum.x)
                              : std::max(0.0F, attack_box.minimum.x
                                                    - hurtbox.maximum.x);
        if (gap > kAttackAssistMaxGap || gap >= best_gap) {
            continue;
        }

        best_gap = gap;
        const float magnitude = std::min(
            gap, kAttackAssistMaxCorrection);
        best_correction = facing_right ? magnitude : -magnitude;
    }

    if (best_gap != std::numeric_limits<float>::max()) {
        player_.position.x = std::clamp(
            player_.position.x + best_correction,
            room_bounds::min_x, room_bounds::max_x);
    }
}

void CombatWorld::resolve_attack_hits() noexcept {
    if (attack_.id == AttackId::none) {
        return;
    }

    const AttackDefinition* definition = find_attack_definition(attack_.id);
    if (definition == nullptr
        || attack_phase_at(*definition, attack_.elapsed_ticks,
                           attack_.startup_ticks, attack_.recovery_ticks)
               != AttackPhase::active) {
        return;
    }

    const Aabb attack_box = make_world_aabb(
        definition->local_hitbox, player_.position, player_.facing);
    std::array<std::uint8_t, kMonsterCapacity> hit_indices{};
    std::size_t hit_count = 0;
    for (std::size_t index = 0; index < monsters_.slots_.size(); ++index) {
        const MonsterRuntime& dummy = monsters_.slots_[index];
        if (attack_.hit_targets[index]
            || dummy.hp <= 0
            || dummy.reaction == ReactionState::defeated
            || dummy.reaction == ReactionState::respawning) {
            continue;
        }
        if (overlaps_inclusive(
                attack_box,
                make_dummy_hurtbox(dummy.kind, dummy.position))) {
            hit_indices[hit_count] = static_cast<std::uint8_t>(index);
            ++hit_count;
        }
    }

    if (hit_count == 0) {
        return;
    }

    const std::uint16_t hit_stop = hit_stop_for(definition->feedback);
    for (std::size_t collected = 0; collected < hit_count; ++collected) {
        const std::size_t index = hit_indices[collected];
        MonsterRuntime& dummy = monsters_.slots_[index];
        const auto resolved_packet = build_player_hit_packet(
            definition->damage, encounter_config_.player_build);
        if (!resolved_packet.has_value()) continue;
        const DamagePacket& packet = *resolved_packet;
        attack_.hit_targets[index] = true;
        attack_.connected = true;
        const std::size_t physical_index = arpg::modifiers::damage_index(
            arpg::modifiers::DamageType::physical);
        int hp_damage = 0;
        for (std::size_t component = 0U; component < packet.amount.size();
             ++component) {
            int value = std::max(0, packet.amount[component]);
            if (component == physical_index
                && dummy.affix_profile.armor_rating > 0) {
                const std::int32_t reduction_bp =
                    arpg::modifiers::rating_to_basis_points(
                        dummy.affix_profile.armor_rating);
                const std::int32_t remaining_bp = kBasisPoints
                    - std::clamp(reduction_bp, 0, kBasisPoints);
                value = ceil_divide(
                    static_cast<std::int64_t>(value) * remaining_bp,
                    kBasisPoints);
            }
            hp_damage += value;
        }
        const int packet_total = hp_damage;
        if (dummy.shield != 0 && hp_damage > 0) {
            const int absorbed = std::min(dummy.shield, hp_damage);
            dummy.shield -= absorbed;
            hp_damage -= absorbed;
            if (dummy.shield == 0) {
                dummy.shield_ticks = 0;
            }
        }
        if (dummy.affix_profile.shield_recharge_delay_ticks != 0U) {
            dummy.shield_recharge_ticks =
                dummy.affix_profile.shield_recharge_delay_ticks;
        }
        dummy.hp = std::max(0, dummy.hp - hp_damage);
        bool starts_break = false;
        const float relative_x = player_.position.x - dummy.position.x;
        const bool front_attack = legacy_mode_
                                   || (dummy.facing == Facing::right
                                           ? relative_x >= 0.0F
                                           : relative_x <= 0.0F);
        bool accepts_impact = dummy.armor != ArmorState::armored
                              || !front_attack;
        if (dummy.hp != 0 && dummy.armor == ArmorState::armored
            && front_attack) {
            dummy.break_value = std::max(
                0, dummy.break_value - definition->break_damage);
            if (dummy.break_value == 0) {
                dummy.armor = ArmorState::broken;
                dummy.break_window_ticks = kBreakWindowTicks;
                starts_break = true;
                accepts_impact = true;
            }
        }
        dummy.hit_stop_ticks = std::max(dummy.hit_stop_ticks, hit_stop);

        CombatEvent hit{};
        hit.kind = CombatEventKind::hit;
        hit.tick = tick_;
        hit.attack = definition->id;
        hit.target_index = static_cast<std::uint8_t>(index);
        hit.hit_count = 1;
        hit.feedback = definition->feedback;
        hit.position = dummy.position;
        hit.value = packet_total;
        emit_event(hit);

        if (starts_break) {
            CombatEvent break_started{};
            break_started.kind = CombatEventKind::break_started;
            break_started.tick = tick_;
            break_started.attack = definition->id;
            break_started.target_index = static_cast<std::uint8_t>(index);
            break_started.feedback = definition->feedback;
            break_started.position = dummy.position;
            emit_event(break_started);
        }

        if (dummy.hp == 0 || accepts_impact) {
            apply_dummy_impact(index, *definition);
        }
    }

    player_.hit_stop_ticks = std::max(player_.hit_stop_ticks, hit_stop);
    if (!attack_.impact_event_emitted) {
        attack_.impact_event_emitted = true;
        CombatEvent summary{};
        summary.kind = CombatEventKind::impact_summary;
        summary.tick = tick_;
        summary.attack = definition->id;
        summary.hit_count = static_cast<std::uint8_t>(hit_count);
        summary.feedback = definition->feedback;
        summary.position = monsters_.slots_[hit_indices[0]].position;
        emit_event(summary);
    }
}

}  // namespace arpg::combat
