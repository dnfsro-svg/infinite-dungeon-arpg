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

int saturating_damage_add(int left, int right) noexcept {
    const int maximum = (std::numeric_limits<int>::max)();
    if (left <= 0) return std::max(0, right);
    if (right <= 0) return left;
    return left > maximum - right ? maximum : left + right;
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
        Vec3 candidate = player_.position;
        candidate.x = std::clamp(
            candidate.x + best_correction,
            room_bounds::min_x, room_bounds::max_x);
        move_player_to(candidate);
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
    resolve_fire_crate_hits(attack_box);
    std::array<std::uint8_t, kMonsterCapacity> hit_indices{};
    std::size_t hit_count = 0;
    for (std::size_t index = 0; index < monsters_.slots_.size(); ++index) {
        const MonsterRuntime& dummy = monsters_.slots_[index];
        if (attack_.hit_targets.contains(dummy.monster_ordinal)
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

    const PlayerAttackHitSpec hit_spec{
        definition->id, definition->damage, definition->break_damage,
        definition->impact, definition->knockback_speed,
        definition->launch_speed, definition->feedback};
    for (std::size_t collected = 0; collected < hit_count; ++collected) {
        const std::size_t index = hit_indices[collected];
        attack_.connected = resolve_player_attack_hit(
            index, hit_spec, attack_.hit_targets) || attack_.connected;
    }

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

void CombatWorld::resolve_fire_crate_hits(Aabb attack_box) noexcept {
    if (room_obstacles_ != nullptr) {
        static_cast<void>(room_obstacles_->damage_overlapping(
            attack_box, tick_));
        return;
    }
    if (!encounter_config_.fire_room_obstacles) return;
    constexpr float kCrateHalfExtent = 0.80F;
    for (FireRoomCrateSnapshot& crate : fire_crates_) {
        if (!crate.intact) continue;
        const Aabb crate_box{{crate.position.x - kCrateHalfExtent,
                                  crate.position.y - kCrateHalfExtent, 0.0F},
            {crate.position.x + kCrateHalfExtent,
                crate.position.y + kCrateHalfExtent, 1.0F}};
        if (!overlaps_inclusive(attack_box, crate_box)) continue;
        crate.intact = false;
        crate.broken_tick = tick_;
    }
}

bool CombatWorld::resolve_player_attack_hit(
    std::size_t index,
    const PlayerAttackHitSpec& spec,
    MonsterOrdinalSet& hit_latch) noexcept {
    if (index >= monsters_.slots_.size()) return false;
    MonsterRuntime& dummy = monsters_.slots_[index];
    if (hit_latch.contains(dummy.monster_ordinal)) return false;
    if (!dummy.active || dummy.hp <= 0
        || dummy.reaction == ReactionState::defeated
        || dummy.reaction == ReactionState::respawning) {
        return false;
    }
    const auto resolved_packet = build_player_hit_packet(
        spec.base_physical, encounter_config_.player_build);
    if (!resolved_packet.has_value()) return false;

    if (!hit_latch.insert(dummy.monster_ordinal)) return false;
    if (room_monster_field_ != nullptr) {
        dummy.engagement_latch = 1U;
        if (dummy.ai_phase == MonsterAiPhase::idle) {
            dummy.ai_phase = MonsterAiPhase::move;
            dummy.ai_ticks = 0U;
        }
    }
    const DamagePacket& packet = *resolved_packet;
    const std::size_t physical_index = arpg::modifiers::damage_index(
        arpg::modifiers::DamageType::physical);
    int hp_damage = 0;
    for (std::size_t component = 0U; component < packet.amount.size();
         ++component) {
        int value = std::max(0, packet.amount[component]);
        if (component == physical_index && dummy.affix_profile.armor_rating > 0) {
            const std::int32_t reduction_bp = arpg::modifiers::rating_to_basis_points(
                dummy.affix_profile.armor_rating);
            const std::int32_t remaining_bp = kBasisPoints
                - std::clamp(reduction_bp, 0, kBasisPoints);
            value = ceil_divide(
                static_cast<std::int64_t>(value) * remaining_bp, kBasisPoints);
        }
        hp_damage = saturating_damage_add(hp_damage, value);
    }
    const int packet_total = hp_damage;
    if (dummy.shield != 0 && hp_damage > 0) {
        const int absorbed = std::min(dummy.shield, hp_damage);
        dummy.shield -= absorbed;
        hp_damage -= absorbed;
        if (dummy.shield == 0) dummy.shield_ticks = 0;
    }
    if (dummy.affix_profile.shield_recharge_delay_ticks != 0U) {
        dummy.shield_recharge_ticks = dummy.affix_profile.shield_recharge_delay_ticks;
    }
    dummy.hp = std::max(0, dummy.hp - hp_damage);
    const float relative_x = player_.position.x - dummy.position.x;
    const bool front_attack = legacy_mode_
        || (dummy.facing == Facing::right ? relative_x >= 0.0F
                                           : relative_x <= 0.0F);
    bool accepts_impact = dummy.armor != ArmorState::armored || !front_attack;
    bool starts_break = false;
    if (dummy.hp != 0 && dummy.armor == ArmorState::armored && front_attack) {
        dummy.break_value = std::max(0, dummy.break_value - spec.break_damage);
        if (dummy.break_value == 0) {
            dummy.armor = ArmorState::broken;
            dummy.break_window_ticks = kBreakWindowTicks;
            starts_break = true;
            accepts_impact = true;
        }
    }
    const std::uint16_t hit_stop = hit_stop_for(spec.feedback);
    dummy.hit_stop_ticks = std::max(dummy.hit_stop_ticks, hit_stop);

    CombatEvent hit{};
    hit.kind = CombatEventKind::hit;
    hit.tick = tick_;
    hit.attack = spec.source;
    hit.skill = spec.skill;
    hit.strike_index = spec.strike_index;
    hit.finisher = spec.finisher;
    hit.target_ordinal = dummy.monster_ordinal;
    hit.hit_count = 1;
    hit.feedback = spec.feedback;
    hit.position = dummy.position;
    hit.value = packet_total;
    emit_event(hit);
    if (starts_break) {
        CombatEvent break_started{};
        break_started.kind = CombatEventKind::break_started;
        break_started.tick = tick_;
        break_started.attack = spec.source;
        break_started.skill = spec.skill;
        break_started.strike_index = spec.strike_index;
        break_started.finisher = spec.finisher;
        break_started.target_ordinal = dummy.monster_ordinal;
        break_started.feedback = spec.feedback;
        break_started.position = dummy.position;
        emit_event(break_started);
    }
    const AttackDefinition impact{
        spec.source, 0U, 0U, 0U, spec.base_physical, spec.break_damage,
        spec.impact, {}, 0.0F, spec.knockback_speed, spec.launch_speed,
        spec.feedback};
    if (dummy.hp == 0 || accepts_impact) apply_dummy_impact(index, impact);
    player_.hit_stop_ticks = std::max(player_.hit_stop_ticks, hit_stop);
    return true;
}

}  // namespace arpg::combat
