#include "combat/monster_persistent_state.hpp"

#include <cstring>

namespace arpg::combat {
namespace {

[[nodiscard]] bool same_float_bits(const float left, const float right) noexcept {
    std::uint32_t left_bits{};
    std::uint32_t right_bits{};
    static_assert(sizeof(left_bits) == sizeof(left));
    std::memcpy(&left_bits, &left, sizeof(left_bits));
    std::memcpy(&right_bits, &right, sizeof(right_bits));
    return left_bits == right_bits;
}

[[nodiscard]] bool same_vec3(const Vec3 left, const Vec3 right) noexcept {
    return same_float_bits(left.x, right.x)
        && same_float_bits(left.y, right.y)
        && same_float_bits(left.z, right.z);
}

[[nodiscard]] bool same_affix_profile(
    const MonsterAffixProfile& left,
    const MonsterAffixProfile& right) noexcept {
    return left.max_hp == right.max_hp
        && left.armor_rating == right.armor_rating
        && left.max_shield == right.max_shield
        && left.shield_recharge_delay_ticks
            == right.shield_recharge_delay_ticks
        && left.damage_bp == right.damage_bp
        && left.attack_timing_bp == right.attack_timing_bp
        && left.move_bp == right.move_bp
        && left.cooldown_bp == right.cooldown_bp
        && left.horizontal_impulse_bp == right.horizontal_impulse_bp;
}

}  // namespace

bool monster_persistent_state_matches(
    const MonsterRuntime& runtime,
    const MonsterPersistentState& state) noexcept {
    return runtime.id == state.id
        && runtime.affixes == state.affixes
        && runtime.monster_ordinal == state.spawn_ordinal
        && runtime.spawn_ordinal == state.spawn_ordinal
        && same_affix_profile(runtime.affix_profile, state.affix_profile)
        && runtime.kind == state.kind
        && same_vec3(runtime.spawn, state.spawn)
        && same_vec3(runtime.position, state.position)
        && same_vec3(runtime.velocity, state.velocity)
        && runtime.facing == state.facing
        && runtime.reaction == state.reaction
        && runtime.armor == state.armor
        && runtime.reaction_ticks == state.reaction_ticks
        && runtime.ai_phase == state.ai_phase
        && runtime.ai_ticks == state.ai_ticks
        && runtime.attack_serial == state.attack_serial
        && runtime.contact_attack_resolved == state.contact_attack_resolved
        && runtime.hp == state.hp
        && runtime.max_hp == state.max_hp
        && runtime.break_value == state.break_value
        && runtime.max_break == state.max_break
        && runtime.shield == state.shield
        && runtime.max_shield == state.max_shield
        && runtime.shield_ticks == state.shield_ticks
        && runtime.max_shield_ticks == state.max_shield_ticks
        && runtime.shield_recharge_ticks == state.shield_recharge_ticks
        && runtime.break_window_ticks == state.break_window_ticks
        && runtime.hit_stop_ticks == state.hit_stop_ticks
        && runtime.owner_transient_counter == state.owner_transient_counter
        && runtime.burning_ground_ticks == state.burning_ground_ticks
        && runtime.blink_assault_ticks == state.blink_assault_ticks
        && runtime.blink_empowered == state.blink_empowered
        && same_vec3(
            runtime.attack_target_position, state.attack_target_position)
        && same_vec3(runtime.attack_vector, state.attack_vector)
        && runtime.affix_warning == state.affix_warning
        && runtime.affix_warning_ticks == state.affix_warning_ticks
        && runtime.effects.same_state(state.effects)
        && runtime.effects_touched == state.effects_touched;
}

void store_monster_persistent_state(
    const MonsterRuntime& runtime,
    MonsterPersistentState& state,
    const bool touched) noexcept {
    state.id = runtime.id;
    state.affixes = runtime.affixes;
    state.spawn_ordinal = runtime.monster_ordinal;
    state.affix_profile = runtime.affix_profile;
    state.kind = runtime.kind;
    state.spawn = runtime.spawn;
    state.position = runtime.position;
    state.velocity = runtime.velocity;
    state.facing = runtime.facing;
    state.reaction = runtime.reaction;
    state.armor = runtime.armor;
    state.reaction_ticks = runtime.reaction_ticks;
    state.ai_phase = runtime.ai_phase;
    state.ai_ticks = runtime.ai_ticks;
    state.attack_serial = runtime.attack_serial;
    state.contact_attack_resolved = runtime.contact_attack_resolved;
    state.hp = runtime.hp;
    state.max_hp = runtime.max_hp;
    state.break_value = runtime.break_value;
    state.max_break = runtime.max_break;
    state.shield = runtime.shield;
    state.max_shield = runtime.max_shield;
    state.shield_ticks = runtime.shield_ticks;
    state.max_shield_ticks = runtime.max_shield_ticks;
    state.shield_recharge_ticks = runtime.shield_recharge_ticks;
    state.break_window_ticks = runtime.break_window_ticks;
    state.hit_stop_ticks = runtime.hit_stop_ticks;
    state.owner_transient_counter = runtime.owner_transient_counter;
    state.burning_ground_ticks = runtime.burning_ground_ticks;
    state.blink_assault_ticks = runtime.blink_assault_ticks;
    state.blink_empowered = runtime.blink_empowered;
    state.attack_target_position = runtime.attack_target_position;
    state.attack_vector = runtime.attack_vector;
    state.affix_warning = runtime.affix_warning;
    state.affix_warning_ticks = runtime.affix_warning_ticks;
    state.effects = runtime.effects;
    state.effects_touched = runtime.effects_touched;
    state.touched = state.touched || touched;
    state.defeated = state.defeated || runtime.hp <= 0
        || runtime.reaction == ReactionState::defeated;
}

void restore_monster_persistent_state(
    const MonsterPersistentState& state,
    MonsterRuntime& runtime) noexcept {
    const bool active = runtime.active;
    const std::uint16_t generation = runtime.generation;
    runtime = MonsterRuntime{};
    runtime.active = active;
    runtime.generation = generation;
    runtime.monster_ordinal = state.spawn_ordinal;
    runtime.id = state.id;
    runtime.affixes = state.affixes;
    runtime.spawn_ordinal = state.spawn_ordinal;
    runtime.affix_profile = state.affix_profile;
    runtime.kind = state.kind;
    runtime.spawn = state.spawn;
    runtime.position = state.position;
    runtime.velocity = state.velocity;
    runtime.facing = state.facing;
    runtime.reaction = state.reaction;
    runtime.armor = state.armor;
    runtime.reaction_ticks = state.reaction_ticks;
    runtime.ai_phase = state.ai_phase;
    runtime.ai_ticks = state.ai_ticks;
    runtime.attack_serial = state.attack_serial;
    runtime.contact_attack_resolved = state.contact_attack_resolved;
    runtime.hp = state.hp;
    runtime.max_hp = state.max_hp;
    runtime.break_value = state.break_value;
    runtime.max_break = state.max_break;
    runtime.shield = state.shield;
    runtime.max_shield = state.max_shield;
    runtime.shield_ticks = state.shield_ticks;
    runtime.max_shield_ticks = state.max_shield_ticks;
    runtime.shield_recharge_ticks = state.shield_recharge_ticks;
    runtime.break_window_ticks = state.break_window_ticks;
    runtime.hit_stop_ticks = state.hit_stop_ticks;
    runtime.owner_transient_counter = state.owner_transient_counter;
    runtime.burning_ground_ticks = state.burning_ground_ticks;
    runtime.blink_assault_ticks = state.blink_assault_ticks;
    runtime.blink_empowered = state.blink_empowered;
    runtime.attack_target_position = state.attack_target_position;
    runtime.attack_vector = state.attack_vector;
    runtime.affix_warning = state.affix_warning;
    runtime.affix_warning_ticks = state.affix_warning_ticks;
    runtime.effects = state.effects;
    runtime.effects_touched = state.effects_touched;
}

}  // namespace arpg::combat
