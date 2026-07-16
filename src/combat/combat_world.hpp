#pragma once

#include "combat/combat_types.hpp"
#include "combat/input_buffer.hpp"
#include "combat/monster_pool.hpp"
#include "core/deterministic_rng.hpp"
#include "modifiers/effect_set.hpp"
#include "core/bounded_queue.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::test {
struct DungeonSessionTestAccess;
}

namespace arpg::combat {

class CombatWorld final {
public:
    explicit CombatWorld(CombatLabConfig config = {}) noexcept;
    explicit CombatWorld(CombatEncounterConfig config) noexcept;

    [[nodiscard]] bool queue_action(Action action) noexcept;
    void tick(MovementInput movement) noexcept;
    void reset() noexcept;
    void apply_player_build(PlayerCombatBuild build) noexcept;
    [[nodiscard]] bool load_wave(
        const EncounterWave& wave,
        bool reset_player_health = true) noexcept;
    [[nodiscard]] bool destroy_monster(MonsterHandle handle) noexcept;
    [[nodiscard]] std::size_t active_monster_count() const noexcept;
    [[nodiscard]] std::size_t active_projectile_count() const noexcept;
    [[nodiscard]] CombatSnapshot snapshot() const noexcept;
    [[nodiscard]] std::optional<CombatEvent> try_pop_event() noexcept;

private:
    struct PlayerRuntime final {
        Vec3 position{};
        Vec3 velocity{};
        Facing facing{Facing::right};
        PlayerState state{PlayerState::idle};
        std::uint8_t combo_stage{};
        std::uint16_t hit_stop_ticks{};
        bool air_attack_available{true};
        int hp{};
        int max_hp{};
        int barrier{};
        int max_barrier{};
        std::array<std::int32_t, modifiers::kElementCount> damage_reduction{};
        std::array<std::int32_t, modifiers::kElementCount> damage_reduction_cap{};
        std::int64_t armor{};
        std::int64_t evasion{};
        std::int32_t armor_reduction_bp{};
        std::int32_t evasion_rate_bp{};
        std::uint16_t hurt_ticks{};
        std::uint16_t invulnerability_ticks{};
        PlayerStatusRuntime status{};
    };

    struct AttackRuntime final {
        AttackId id{AttackId::none};
        std::uint16_t elapsed_ticks{};
        std::uint16_t startup_ticks{};
        std::uint16_t recovery_ticks{};
        bool connected{};
        bool impact_event_emitted{};
        std::array<bool, kMonsterCapacity> hit_targets{};
    };

    void simulate_player(MovementInput movement) noexcept;
    void simulate_target(std::size_t index) noexcept;
    void simulate_monster(std::size_t slot) noexcept;
    void simulate_projectiles() noexcept;
    void simulate_hazards() noexcept;
    void resolve_monster_contact_attack(std::size_t slot) noexcept;
    void apply_dummy_impact(
        std::size_t index,
        const AttackDefinition& definition) noexcept;
    void respawn_dummy(std::size_t index) noexcept;
    void apply_attack_assist(const AttackDefinition& definition) noexcept;
    void resolve_attack_hits() noexcept;
    bool apply_player_damage(
        DamagePacket damage,
        DamageDelivery delivery,
        Vec3 source_position,
        FeedbackLevel feedback) noexcept;
    bool apply_player_damage(
        int damage, Vec3 source_position, FeedbackLevel feedback) noexcept;
    void apply_monster_direct_hit(
        std::size_t slot,
        DamagePacket packet,
        Vec3 source_position,
        FeedbackLevel feedback) noexcept;
    void tick_player_status() noexcept;
    [[nodiscard]] bool spawn_projectile(
        MonsterHandle owner,
        Vec3 position,
        Vec3 velocity,
        std::uint16_t lifetime_ticks,
        DamagePacket damage,
        float radius) noexcept;
    [[nodiscard]] bool spawn_projectile(
        MonsterHandle owner,
        Vec3 position,
        Vec3 velocity,
        std::uint16_t lifetime_ticks,
        int damage,
        float radius) noexcept;
    [[nodiscard]] bool spawn_hazard(
        MonsterHandle owner,
        Vec3 center,
        float radius,
        std::uint16_t telegraph_ticks,
        std::uint16_t active_ticks,
        std::uint16_t damage_interval_ticks,
        DamagePacket damage) noexcept;
    [[nodiscard]] bool spawn_hazard(
        MonsterHandle owner,
        Vec3 center,
        float radius,
        std::uint16_t telegraph_ticks,
        std::uint16_t active_ticks,
        std::uint16_t damage_interval_ticks,
        int damage) noexcept;
    void remove_owned_projectiles(MonsterHandle owner) noexcept;
    void remove_owned_hazards(MonsterHandle owner) noexcept;
    void emit_event(const CombatEvent& event) noexcept;
    void initialize_runtime() noexcept;

    void initialize_player() noexcept;
    void initialize_legacy_monsters() noexcept;
    modifiers::EffectSet* find_effects(std::size_t monster_slot) noexcept;
    modifiers::EffectSet* ensure_effects(std::size_t monster_slot) noexcept;
    void apply_effect_commands(
        MonsterRuntime& monster,
        modifiers::EffectSet& effects) noexcept;
    [[nodiscard]] modifiers::EffectDefinition water_barrier_effect(
        const MonsterRuntime& target) const noexcept;
    void simulate_melee_ai(
        std::size_t slot,
        MonsterRuntime& monster,
        const MonsterDefinition& definition) noexcept;
    void simulate_ranged_ai(
        std::size_t slot,
        MonsterRuntime& monster,
        const MonsterDefinition& definition) noexcept;
    void simulate_special_ai(
        std::size_t slot,
        MonsterRuntime& monster,
        const MonsterDefinition& definition) noexcept;

    friend struct ::arpg::test::CombatWorldTestAccess;
    friend struct ::arpg::test::DungeonSessionTestAccess;

    CombatEncounterConfig encounter_config_{};
    CombatLabConfig legacy_config_{};
    bool legacy_mode_{true};
    PlayerRuntime player_{};
    MonsterPool monsters_{};
    ProjectilePool projectiles_{};
    HazardPool hazards_{};
    AttackRuntime attack_{};
    InputBuffer input_buffer_{};
    struct EffectOwner final {
        std::size_t monster_slot{};
        std::uint16_t generation{};
        modifiers::EffectSet effects{};
        bool occupied{};
    };
    std::array<EffectOwner, 8> effect_owners_{};
    core::BoundedQueue<CombatEvent, 64> events_{};
    std::uint64_t tick_{};
    std::uint32_t event_overflow_count_{};
    std::uint32_t projectile_saturation_count_{};
    std::uint32_t projectile_invalid_owner_count_{};
    std::uint32_t hazard_saturation_count_{};
    std::uint32_t hazard_invalid_owner_count_{};
    core::DeterministicRng evasion_rng_{0};
};

}  // namespace arpg::combat
