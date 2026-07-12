#pragma once

#include "combat/combat_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::test {
struct DungeonSessionTestAccess;
}

namespace arpg::combat {

class CombatWorld;

struct MonsterRuntime final {
    bool active{};
    std::uint16_t generation{};
    MonsterId id{MonsterId::chaos_chaser};
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
    std::uint16_t break_window_ticks{};
    std::uint16_t hit_stop_ticks{};
    std::uint16_t owner_transient_counter{};
    Vec3 attack_target_position{};
    Vec3 attack_vector{};
};

class MonsterPool final {
public:
    void clear() noexcept;
    [[nodiscard]] std::optional<MonsterHandle> spawn(
        MonsterId id, Vec3 position) noexcept;
    [[nodiscard]] bool destroy(MonsterHandle handle) noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;
    [[nodiscard]] MonsterRuntime* get(MonsterHandle handle) noexcept;
    [[nodiscard]] const MonsterRuntime* get(MonsterHandle handle) const noexcept;
    [[nodiscard]] const std::array<MonsterRuntime, kMonsterCapacity>&
    slots() const noexcept;

private:
    friend class CombatWorld;
    friend struct ::arpg::test::DungeonSessionTestAccess;
    static void advance_generation(MonsterRuntime& runtime) noexcept;

    std::array<MonsterRuntime, kMonsterCapacity> slots_{};
    std::size_t active_count_{};
};

class ProjectilePool final {
public:
    void clear() noexcept;
    [[nodiscard]] std::optional<ProjectileHandle> spawn(
        MonsterHandle owner,
        Vec3 position,
        Vec3 velocity,
        std::uint16_t lifetime_ticks,
        int damage,
        float radius) noexcept;
    [[nodiscard]] bool destroy(ProjectileHandle handle) noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;
    [[nodiscard]] ProjectileRuntime* get(ProjectileHandle handle) noexcept;
    [[nodiscard]] const ProjectileRuntime* get(
        ProjectileHandle handle) const noexcept;
    [[nodiscard]] const std::array<ProjectileRuntime, kProjectileCapacity>&
    slots() const noexcept;
    [[nodiscard]] std::array<ProjectileRuntime, kProjectileCapacity>&
    slots() noexcept;

private:
    static void advance_generation(ProjectileRuntime& runtime) noexcept;

    std::array<ProjectileRuntime, kProjectileCapacity> slots_{};
    std::size_t active_count_{};
};

struct HazardHandle final {
    std::uint16_t index{0xFFFF};
    std::uint16_t generation{};
};

class HazardPool final {
public:
    void clear() noexcept;
    [[nodiscard]] std::optional<HazardHandle> spawn(
        MonsterHandle owner,
        Vec3 center,
        float radius,
        std::uint16_t telegraph_ticks,
        std::uint16_t active_ticks,
        std::uint16_t damage_interval_ticks,
        int damage,
        bool persists_after_owner_death = false) noexcept;
    [[nodiscard]] bool destroy(HazardHandle handle) noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;
    [[nodiscard]] HazardRuntime* get(HazardHandle handle) noexcept;
    [[nodiscard]] const HazardRuntime* get(HazardHandle handle) const noexcept;
    [[nodiscard]] const std::array<HazardRuntime, kHazardCapacity>&
    slots() const noexcept;
    [[nodiscard]] std::array<HazardRuntime, kHazardCapacity>& slots() noexcept;

private:
    static void advance_generation(HazardRuntime& runtime) noexcept;

    std::array<HazardRuntime, kHazardCapacity> slots_{};
    std::size_t active_count_{};
};

}  // namespace arpg::combat
