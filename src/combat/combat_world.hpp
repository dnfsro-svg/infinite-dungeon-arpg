#pragma once

#include "combat/active_skill_runtime.hpp"
#include "combat/combat_types.hpp"
#include "combat/input_buffer.hpp"
#include "combat/monster_pool.hpp"
#include "combat/player_damage_history.hpp"
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

struct PlayerAttackHitSpec final {
    AttackId source{AttackId::none};
    int base_physical{};
    int break_damage{};
    ImpactKind impact{ImpactKind::light_hitstun};
    float knockback_speed{};
    float launch_speed{};
    FeedbackLevel feedback{FeedbackLevel::light};
    skills::ActiveSkillId skill{skills::ActiveSkillId::none};
    std::uint8_t strike_index{};
    bool finisher{};
};

class CombatWorld final {
public:
    explicit CombatWorld(CombatLabConfig config = {}) noexcept;
    explicit CombatWorld(CombatEncounterConfig config) noexcept;

    [[nodiscard]] bool queue_action(Action action) noexcept;
    [[nodiscard]] SkillCastResult request_active_skill(
        skills::ActiveSkillId skill) noexcept;
    void tick(MovementInput movement) noexcept;
    void reset() noexcept;
    void apply_player_build(PlayerCombatBuild build) noexcept;
    void restore_player_resources(int hp, int barrier) noexcept;
    void clear_abyss_rule_preserving_resources() noexcept;
    [[nodiscard]] bool load_wave(
        const EncounterWave& wave,
        bool reset_player_health = true) noexcept;
    [[nodiscard]] bool destroy_monster(MonsterHandle handle) noexcept;
    [[nodiscard]] Vec3 player_position() const noexcept;
    [[nodiscard]] std::size_t living_monster_count() const noexcept;
    [[nodiscard]] std::size_t active_monster_count() const noexcept;
    [[nodiscard]] std::size_t active_projectile_count() const noexcept;
    [[nodiscard]] CombatSnapshot snapshot() const noexcept;
    [[nodiscard]] std::optional<CombatEvent> try_pop_event() noexcept;
    [[nodiscard]] bool player_defeated() const noexcept;
    [[nodiscard]] const std::optional<CombatDeathSnapshot>&
    death_snapshot() const noexcept;

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
    void simulate_active_skill_movement(MovementInput movement) noexcept;
    void tick_active_skill_cooldowns() noexcept;
    void tick_active_skill() noexcept;
    void apply_active_skill_events_at(std::uint16_t tick) noexcept;
    void pull_storm_swords_targets() noexcept;
    void clear_active_skill() noexcept;
    void resolve_draw_slash_hits() noexcept;
    void resolve_storm_swords_hits(bool finisher) noexcept;
    [[nodiscard]] bool resolve_player_attack_hit(
        std::size_t index,
        const PlayerAttackHitSpec& spec,
        std::array<bool, kMonsterCapacity>& hit_latch) noexcept;
    void simulate_target(std::size_t index) noexcept;
    void simulate_monster(std::size_t slot) noexcept;
    void simulate_projectiles() noexcept;
    void simulate_hazards() noexcept;
    void simulate_abyss_environment() noexcept;
    [[nodiscard]] static DamagePacket environment_damage_packet(
        int actual_max_hp,
        std::uint16_t basis_points,
        modifiers::DamageType damage_type) noexcept;
    void resolve_monster_contact_attack(std::size_t slot) noexcept;
    void apply_dummy_impact(
        std::size_t index,
        const AttackDefinition& definition) noexcept;
    void defeat_monster(
        std::size_t slot, AttackId attack, bool reward_eligible) noexcept;
    void respawn_dummy(std::size_t index) noexcept;
    void apply_attack_assist(const AttackDefinition& definition) noexcept;
    void resolve_attack_hits() noexcept;
    bool apply_player_damage(
        DamagePacket damage,
        DamageDelivery delivery,
        PlayerDamageSource source,
        Vec3 source_position,
        FeedbackLevel feedback) noexcept;
    bool apply_player_damage(
        int damage, Vec3 source_position, FeedbackLevel feedback) noexcept;
    bool apply_monster_direct_hit(
        std::size_t slot,
        DamagePacket packet,
        Vec3 source_position,
        FeedbackLevel feedback,
        bool trigger_chain = true,
        PlayerDamageSourceKind source_kind =
            PlayerDamageSourceKind::monster_attack) noexcept;
    void tick_player_status() noexcept;
    [[nodiscard]] bool spawn_projectile(
        MonsterHandle owner,
        Vec3 position,
        Vec3 velocity,
        std::uint16_t lifetime_ticks,
        DamagePacket damage,
        float radius,
        bool trigger_chain_on_end = false,
        MonsterAffixSet owner_affixes = {}) noexcept;
    [[nodiscard]] bool spawn_projectile(
        MonsterHandle owner,
        Vec3 position,
        Vec3 velocity,
        std::uint16_t lifetime_ticks,
        int damage,
        float radius,
        bool trigger_chain_on_end = false,
        MonsterAffixSet owner_affixes = {}) noexcept;
    [[nodiscard]] bool spawn_hazard(
        MonsterHandle owner,
        HazardKind kind,
        Vec3 center,
        float radius,
        std::uint16_t telegraph_ticks,
        std::uint16_t active_ticks,
        std::uint16_t damage_interval_ticks,
        DamagePacket damage,
        bool persists_after_owner_death = false) noexcept;
    [[nodiscard]] bool spawn_hazard(
        MonsterHandle owner,
        HazardKind kind,
        Vec3 center,
        float radius,
        std::uint16_t telegraph_ticks,
        std::uint16_t active_ticks,
        std::uint16_t damage_interval_ticks,
        int damage,
        bool persists_after_owner_death = false) noexcept;
    [[nodiscard]] bool spawn_environment_hazard(
        HazardKind kind,
        Vec3 center,
        float radius,
        std::uint16_t telegraph_ticks,
        std::uint16_t active_ticks,
        std::uint16_t damage_interval_ticks,
        DamagePacket damage,
        std::uint16_t environment_damage_bp,
        modifiers::DamageType environment_damage_type) noexcept;
    void remove_monster_hazards() noexcept;
    void remove_environment_hazards() noexcept;
    void tick_active_affixes(std::size_t slot, MonsterRuntime& monster) noexcept;
    [[nodiscard]] bool trigger_chain_lightning(
        MonsterHandle owner, MonsterAffixSet affixes, Vec3 center) noexcept;
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
    AbyssEnvironmentRuntime abyss_environment_{};
    AttackRuntime attack_{};
    ActiveSkillRuntime active_skill_{};
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
    PlayerDamageHistory player_damage_history_{};
    std::optional<CombatDeathSnapshot> death_snapshot_{};
};

}  // namespace arpg::combat
