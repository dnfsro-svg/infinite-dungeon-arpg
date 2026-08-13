#pragma once

#include "combat/combat_types.hpp"
#include "combat/monster_affix_runtime.hpp"
#include "modifiers/effect_set.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::test {
struct CombatWorldTestAccess;
struct DungeonSessionTestAccess;
}

namespace arpg::combat {

class CombatWorld;

struct MonsterRuntime final {
    bool active{};
    std::uint16_t generation{};
    MonsterOrdinal monster_ordinal{kInvalidMonsterOrdinal};
    MonsterId id{MonsterId::chaos_chaser};
    MonsterAffixSet affixes{};
    std::uint16_t spawn_ordinal{};
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
    std::uint16_t engagement_latch{};
    std::uint16_t burning_ground_ticks{};
    std::uint16_t blink_assault_ticks{};
    bool blink_empowered{};
    Vec3 attack_target_position{};
    Vec3 attack_vector{};
    MonsterAffixWarning affix_warning{MonsterAffixWarning::none};
    std::uint16_t affix_warning_ticks{};
    modifiers::EffectSet effects{};
    bool effects_touched{};
};

class MonsterPool final {
public:
    void clear() noexcept;
    [[nodiscard]] std::optional<MonsterHandle> spawn(
        const MonsterSpawnSpec& spec) noexcept;
    [[nodiscard]] std::optional<MonsterHandle> spawn(
        const MonsterSpawnSpec& spec,
        const abyss::AbyssCombatConfig& abyss_config) noexcept;
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
    friend struct ::arpg::test::CombatWorldTestAccess;
    friend struct ::arpg::test::DungeonSessionTestAccess;
    static void advance_generation(MonsterRuntime& runtime) noexcept;

    std::array<MonsterRuntime, kMonsterCapacity> slots_{};
    std::size_t active_count_{};
};

class ProjectilePool final {
public:
    void clear() noexcept;
    [[nodiscard]] std::optional<ProjectileHandle> spawn(
        MonsterOrdinal owner_ordinal,
        Vec3 position,
        Vec3 velocity,
        std::uint16_t lifetime_ticks,
        DamagePacket damage,
        float radius,
        bool trigger_chain_on_end = false,
        MonsterAffixSet owner_affixes = {}) noexcept;
    [[nodiscard]] std::optional<ProjectileHandle> spawn(
        MonsterOrdinal owner_ordinal,
        Vec3 position,
        Vec3 velocity,
        std::uint16_t lifetime_ticks,
        int damage,
        float radius,
        bool trigger_chain_on_end = false,
        MonsterAffixSet owner_affixes = {}) noexcept;
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
        HazardSource source,
        MonsterOrdinal owner_ordinal,
        HazardKind kind,
        Vec3 center,
        float radius,
        std::uint16_t telegraph_ticks,
        std::uint16_t active_ticks,
        std::uint16_t damage_interval_ticks,
        DamagePacket damage,
        bool persists_after_owner_death = false,
        std::uint16_t environment_damage_bp = 0U,
        modifiers::DamageType environment_damage_type =
            modifiers::DamageType::physical) noexcept;
    [[nodiscard]] std::optional<HazardHandle> spawn(
        MonsterOrdinal owner_ordinal,
        HazardKind kind,
        Vec3 center,
        float radius,
        std::uint16_t telegraph_ticks,
        std::uint16_t active_ticks,
        std::uint16_t damage_interval_ticks,
        DamagePacket damage,
        bool persists_after_owner_death = false) noexcept;
    [[nodiscard]] std::optional<HazardHandle> spawn(
        MonsterOrdinal owner_ordinal,
        HazardKind kind,
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
