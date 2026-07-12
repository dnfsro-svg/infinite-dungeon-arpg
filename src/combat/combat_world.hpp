#pragma once

#include "combat/combat_types.hpp"
#include "combat/input_buffer.hpp"
#include "combat/monster_pool.hpp"
#include "core/bounded_queue.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::combat {

class CombatWorld final {
public:
    explicit CombatWorld(CombatLabConfig config = {}) noexcept;
    explicit CombatWorld(CombatEncounterConfig config) noexcept;

    [[nodiscard]] bool queue_action(Action action) noexcept;
    void tick(MovementInput movement) noexcept;
    void reset() noexcept;
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
        std::uint16_t hurt_ticks{};
        std::uint16_t invulnerability_ticks{};
    };

    struct AttackRuntime final {
        AttackId id{AttackId::none};
        std::uint16_t elapsed_ticks{};
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
    void apply_player_damage(
        int damage, Vec3 source_position, FeedbackLevel feedback) noexcept;
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
        int damage) noexcept;
    void remove_owned_projectiles(MonsterHandle owner) noexcept;
    void remove_owned_hazards(MonsterHandle owner) noexcept;
    void emit_event(const CombatEvent& event) noexcept;
    void initialize_runtime() noexcept;

    void initialize_player() noexcept;
    void initialize_legacy_monsters() noexcept;

    friend struct ::arpg::test::CombatWorldTestAccess;

    CombatEncounterConfig encounter_config_{};
    CombatLabConfig legacy_config_{};
    bool legacy_mode_{true};
    PlayerRuntime player_{};
    MonsterPool monsters_{};
    ProjectilePool projectiles_{};
    HazardPool hazards_{};
    AttackRuntime attack_{};
    InputBuffer input_buffer_{};
    core::BoundedQueue<CombatEvent, 64> events_{};
    std::uint64_t tick_{};
    std::uint32_t event_overflow_count_{};
    std::uint32_t projectile_saturation_count_{};
    std::uint32_t projectile_invalid_owner_count_{};
    std::uint32_t hazard_saturation_count_{};
    std::uint32_t hazard_invalid_owner_count_{};
};

}  // namespace arpg::combat
