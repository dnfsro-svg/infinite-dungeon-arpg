#pragma once

#include "combat/monster_pool.hpp"

#include <cstdint>

namespace arpg::combat {

struct MonsterPersistentState final {
    MonsterId id{MonsterId::count};
    MonsterAffixSet affixes{};
    MonsterOrdinal spawn_ordinal{kInvalidMonsterOrdinal};
    MonsterAffixProfile affix_profile{};
    DummyKind kind{DummyKind::normal};
    Vec3 spawn{};
    Vec3 position{};
    Vec3 velocity{};
    Facing facing{Facing::right};
    ReactionState reaction{ReactionState::idle};
    ArmorState armor{ArmorState::none};
    std::uint16_t reaction_ticks{};
    MonsterAiPhase ai_phase{MonsterAiPhase::idle};
    std::uint16_t ai_ticks{};
    std::uint32_t attack_serial{};
    bool contact_attack_resolved{};
    int hp{};
    int max_hp{};
    int break_value{};
    int max_break{};
    int shield{};
    int max_shield{};
    std::uint16_t shield_ticks{};
    std::uint16_t max_shield_ticks{};
    std::uint16_t shield_recharge_ticks{};
    std::uint16_t break_window_ticks{};
    std::uint16_t hit_stop_ticks{};
    std::uint16_t owner_transient_counter{};
    std::uint16_t burning_ground_ticks{};
    std::uint16_t blink_assault_ticks{};
    bool blink_empowered{};
    Vec3 attack_target_position{};
    Vec3 attack_vector{};
    MonsterAffixWarning affix_warning{MonsterAffixWarning::none};
    std::uint16_t affix_warning_ticks{};
    modifiers::EffectSet effects{};
    bool effects_touched{};
    bool touched{};
    bool defeated{};
};

[[nodiscard]] bool monster_persistent_state_matches(
    const MonsterRuntime& runtime,
    const MonsterPersistentState& state) noexcept;

void store_monster_persistent_state(
    const MonsterRuntime& runtime,
    MonsterPersistentState& state,
    bool touched) noexcept;

void restore_monster_persistent_state(
    const MonsterPersistentState& state,
    MonsterRuntime& runtime) noexcept;

}  // namespace arpg::combat
