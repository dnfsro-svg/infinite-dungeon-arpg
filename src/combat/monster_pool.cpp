#include "combat/monster_pool.hpp"

#include "combat/combat_scaling.hpp"
#include "combat/monster_catalog.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace arpg::combat {
namespace {

DummyKind kind_for(MonsterId id) noexcept {
    switch (id) {
    case MonsterId::fire_bomber:
    case MonsterId::lightning_shooter:
    case MonsterId::water_support:
        return DummyKind::normal;
    case MonsterId::fire_charger:
    case MonsterId::water_bulwark:
    case MonsterId::chaos_hazard:
        return DummyKind::heavy;
    case MonsterId::lightning_dasher:
    case MonsterId::chaos_chaser:
    case MonsterId::count:
        return DummyKind::light;
    }
    return DummyKind::normal;
}

}  // namespace

void MonsterPool::advance_generation(MonsterRuntime& runtime) noexcept {
    if (runtime.generation == 0xFFFFU) {
        runtime.generation = 1U;
    } else {
        ++runtime.generation;
        if (runtime.generation == 0U) {
            runtime.generation = 1U;
        }
    }
}

void MonsterPool::clear() noexcept {
    active_count_ = 0U;
    for (MonsterRuntime& runtime : slots_) {
        advance_generation(runtime);
        const std::uint16_t generation = runtime.generation;
        runtime = MonsterRuntime{};
        runtime.generation = generation;
    }
}

std::optional<MonsterHandle> MonsterPool::spawn(
    const MonsterSpawnSpec& spec) noexcept {
    return spawn(spec, abyss::AbyssCombatConfig{});
}

std::optional<MonsterHandle> MonsterPool::spawn(
    const MonsterSpawnSpec& spec,
    const abyss::AbyssCombatConfig& abyss_config) noexcept {
    const MonsterDefinition* definition = monster_definition(spec.id);
    if (definition == nullptr) {
        return std::nullopt;
    }

    MonsterAffixProfile profile = evaluate_monster_affixes(
        *definition, spec.affixes);
    profile.armor_rating = scale_basis_points(
        profile.armor_rating, abyss_config.monster_armor_bp);

    for (std::size_t index = 0; index < slots_.size(); ++index) {
        MonsterRuntime& runtime = slots_[index];
        if (runtime.active) {
            continue;
        }

        advance_generation(runtime);
        const std::uint16_t generation = runtime.generation;
        runtime = MonsterRuntime{};
        runtime.active = true;
        runtime.generation = generation;
        runtime.id = spec.id;
        runtime.affixes = spec.affixes;
        runtime.spawn_ordinal = spec.spawn_ordinal;
        runtime.affix_profile = profile;
        runtime.kind = kind_for(spec.id);
        runtime.spawn = spec.position;
        runtime.position = spec.position;
        runtime.max_hp = profile.max_hp;
        runtime.hp = profile.max_hp;
        runtime.max_break = definition->max_break;
        runtime.break_value = definition->max_break;
        runtime.max_shield = profile.max_shield != 0
            ? profile.max_shield
            : definition->shield_points != 0 ? definition->shield_points : 90;
        runtime.max_shield_ticks = profile.shield_recharge_delay_ticks != 0U
            ? profile.shield_recharge_delay_ticks
            : definition->shield_duration_ticks != 0
                ? definition->shield_duration_ticks : 120;
        const int extra_shield = scale_basis_points(
            runtime.max_hp,
            abyss_config.monster_extra_shield_bp,
            BasisPointRounding::ceil);
        runtime.max_shield = extra_shield
                > (std::numeric_limits<int>::max)() - runtime.max_shield
            ? (std::numeric_limits<int>::max)()
            : runtime.max_shield + extra_shield;
        runtime.shield = extra_shield;
        runtime.shield_ticks = 0;
        runtime.armor = definition->max_break > 0
            ? ArmorState::armored : ArmorState::none;
        ++active_count_;
        return MonsterHandle{
            static_cast<std::uint16_t>(index), generation};
    }
    return std::nullopt;
}

std::optional<MonsterHandle> MonsterPool::spawn(
    MonsterId id,
    Vec3 position) noexcept {
    MonsterSpawnSpec spec{};
    spec.id = id;
    spec.position = position;
    return spawn(spec);
}

bool MonsterPool::destroy(MonsterHandle handle) noexcept {
    MonsterRuntime* runtime = get(handle);
    if (runtime == nullptr) {
        return false;
    }
    advance_generation(*runtime);
    const std::uint16_t generation = runtime->generation;
    *runtime = MonsterRuntime{};
    runtime->generation = generation;
    --active_count_;
    return true;
}

std::size_t MonsterPool::active_count() const noexcept {
    return active_count_;
}

MonsterRuntime* MonsterPool::get(MonsterHandle handle) noexcept {
    if (handle.index >= slots_.size()) {
        return nullptr;
    }
    MonsterRuntime& runtime = slots_[handle.index];
    if (!runtime.active || runtime.generation != handle.generation) {
        return nullptr;
    }
    return &runtime;
}

const MonsterRuntime* MonsterPool::get(MonsterHandle handle) const noexcept {
    if (handle.index >= slots_.size()) {
        return nullptr;
    }
    const MonsterRuntime& runtime = slots_[handle.index];
    if (!runtime.active || runtime.generation != handle.generation) {
        return nullptr;
    }
    return &runtime;
}

const std::array<MonsterRuntime, kMonsterCapacity>& MonsterPool::slots() const noexcept {
    return slots_;
}

void ProjectilePool::advance_generation(ProjectileRuntime& runtime) noexcept {
    if (runtime.generation == 0xFFFFU) {
        runtime.generation = 1U;
    } else {
        ++runtime.generation;
        if (runtime.generation == 0U) {
            runtime.generation = 1U;
        }
    }
}

void ProjectilePool::clear() noexcept {
    active_count_ = 0U;
    for (ProjectileRuntime& runtime : slots_) {
        advance_generation(runtime);
        const std::uint16_t generation = runtime.generation;
        runtime = ProjectileRuntime{};
        runtime.generation = generation;
    }
}

std::optional<ProjectileHandle> ProjectilePool::spawn(
    MonsterHandle owner,
    Vec3 position,
    Vec3 velocity,
    std::uint16_t lifetime_ticks,
    DamagePacket damage,
    float radius,
    bool trigger_chain_on_end,
    MonsterAffixSet owner_affixes) noexcept {
    if (owner.index >= kMonsterCapacity || owner.generation == 0U) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        ProjectileRuntime& runtime = slots_[index];
        if (runtime.active) {
            continue;
        }

        advance_generation(runtime);
        const std::uint16_t generation = runtime.generation;
        runtime = ProjectileRuntime{};
        runtime.active = true;
        runtime.generation = generation;
        runtime.owner = owner;
        runtime.position = position;
        runtime.velocity = velocity;
        runtime.lifetime_ticks = lifetime_ticks;
        runtime.damage = damage;
        runtime.radius = radius;
        runtime.trigger_chain_on_end = trigger_chain_on_end;
        runtime.owner_affixes = owner_affixes;
        ++active_count_;
        return ProjectileHandle{
            static_cast<std::uint16_t>(index), generation};
    }
    return std::nullopt;
}

std::optional<ProjectileHandle> ProjectilePool::spawn(
    MonsterHandle owner,
    Vec3 position,
    Vec3 velocity,
    std::uint16_t lifetime_ticks,
    int damage,
    float radius,
    bool trigger_chain_on_end,
    MonsterAffixSet owner_affixes) noexcept {
    return spawn(owner, position, velocity, lifetime_ticks,
                 DamagePacket{damage}, radius, trigger_chain_on_end,
                 owner_affixes);
}

bool ProjectilePool::destroy(ProjectileHandle handle) noexcept {
    ProjectileRuntime* runtime = get(handle);
    if (runtime == nullptr) {
        return false;
    }
    advance_generation(*runtime);
    const std::uint16_t generation = runtime->generation;
    *runtime = ProjectileRuntime{};
    runtime->generation = generation;
    --active_count_;
    return true;
}

std::size_t ProjectilePool::active_count() const noexcept {
    return active_count_;
}

ProjectileRuntime* ProjectilePool::get(ProjectileHandle handle) noexcept {
    if (handle.index >= slots_.size()) {
        return nullptr;
    }
    ProjectileRuntime& runtime = slots_[handle.index];
    if (!runtime.active || runtime.generation != handle.generation) {
        return nullptr;
    }
    return &runtime;
}

const ProjectileRuntime* ProjectilePool::get(
    ProjectileHandle handle) const noexcept {
    if (handle.index >= slots_.size()) {
        return nullptr;
    }
    const ProjectileRuntime& runtime = slots_[handle.index];
    if (!runtime.active || runtime.generation != handle.generation) {
        return nullptr;
    }
    return &runtime;
}

const std::array<ProjectileRuntime, kProjectileCapacity>&
ProjectilePool::slots() const noexcept {
    return slots_;
}

std::array<ProjectileRuntime, kProjectileCapacity>&
ProjectilePool::slots() noexcept {
    return slots_;
}

void HazardPool::advance_generation(HazardRuntime& runtime) noexcept {
    if (runtime.generation == 0xFFFFU) {
        runtime.generation = 1U;
    } else {
        ++runtime.generation;
        if (runtime.generation == 0U) {
            runtime.generation = 1U;
        }
    }
}

void HazardPool::clear() noexcept {
    active_count_ = 0U;
    for (HazardRuntime& runtime : slots_) {
        advance_generation(runtime);
        const std::uint16_t generation = runtime.generation;
        runtime = HazardRuntime{};
        runtime.generation = generation;
    }
}

std::optional<HazardHandle> HazardPool::spawn(
    HazardSource source,
    MonsterHandle owner,
    HazardKind kind,
    Vec3 center,
    float radius,
    std::uint16_t telegraph_ticks,
    std::uint16_t active_ticks,
    std::uint16_t damage_interval_ticks,
    DamagePacket damage,
    bool persists_after_owner_death) noexcept {
    if ((source == HazardSource::monster
         && (owner.index >= kMonsterCapacity || owner.generation == 0U))
        || radius <= 0.0F || active_ticks == 0U) {
        return std::nullopt;
    }
    bool has_damage = false;
    for (const int value : damage.amount) has_damage = has_damage || value > 0;
    if (!has_damage) return std::nullopt;
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        HazardRuntime& runtime = slots_[index];
        if (runtime.active) {
            continue;
        }
        advance_generation(runtime);
        const std::uint16_t generation = runtime.generation;
        runtime = HazardRuntime{};
        runtime.active = true;
        runtime.generation = generation;
        runtime.owner = owner;
        runtime.source = source;
        runtime.kind = kind;
        runtime.center = center;
        runtime.radius = radius;
        runtime.telegraph_ticks = telegraph_ticks;
        runtime.active_ticks = active_ticks;
        runtime.lifetime_ticks = static_cast<std::uint16_t>(
            telegraph_ticks + active_ticks);
        runtime.damage_interval_ticks = damage_interval_ticks == 0U
            ? 1U : damage_interval_ticks;
        runtime.damage = damage;
        runtime.persists_after_owner_death = persists_after_owner_death;
        ++active_count_;
        return HazardHandle{static_cast<std::uint16_t>(index), generation};
    }
    return std::nullopt;
}

std::optional<HazardHandle> HazardPool::spawn(
    MonsterHandle owner,
    HazardKind kind,
    Vec3 center,
    float radius,
    std::uint16_t telegraph_ticks,
    std::uint16_t active_ticks,
    std::uint16_t damage_interval_ticks,
    DamagePacket damage,
    bool persists_after_owner_death) noexcept {
    return spawn(HazardSource::monster, owner, kind, center, radius,
                 telegraph_ticks, active_ticks, damage_interval_ticks, damage,
                 persists_after_owner_death);
}

std::optional<HazardHandle> HazardPool::spawn(
    MonsterHandle owner,
    HazardKind kind,
    Vec3 center,
    float radius,
    std::uint16_t telegraph_ticks,
    std::uint16_t active_ticks,
    std::uint16_t damage_interval_ticks,
    int damage,
    bool persists_after_owner_death) noexcept {
    return spawn(owner, kind, center, radius, telegraph_ticks, active_ticks,
                 damage_interval_ticks, DamagePacket{damage},
                 persists_after_owner_death);
}

bool HazardPool::destroy(HazardHandle handle) noexcept {
    HazardRuntime* runtime = get(handle);
    if (runtime == nullptr) {
        return false;
    }
    advance_generation(*runtime);
    const std::uint16_t generation = runtime->generation;
    *runtime = HazardRuntime{};
    runtime->generation = generation;
    --active_count_;
    return true;
}

std::size_t HazardPool::active_count() const noexcept {
    return active_count_;
}

HazardRuntime* HazardPool::get(HazardHandle handle) noexcept {
    if (handle.index >= slots_.size()) {
        return nullptr;
    }
    HazardRuntime& runtime = slots_[handle.index];
    if (!runtime.active || runtime.generation != handle.generation) {
        return nullptr;
    }
    return &runtime;
}

const HazardRuntime* HazardPool::get(HazardHandle handle) const noexcept {
    if (handle.index >= slots_.size()) {
        return nullptr;
    }
    const HazardRuntime& runtime = slots_[handle.index];
    if (!runtime.active || runtime.generation != handle.generation) {
        return nullptr;
    }
    return &runtime;
}

const std::array<HazardRuntime, kHazardCapacity>& HazardPool::slots() const noexcept {
    return slots_;
}

std::array<HazardRuntime, kHazardCapacity>& HazardPool::slots() noexcept {
    return slots_;
}

}  // namespace arpg::combat
