#include "combat/monster_pool.hpp"

#include "combat/monster_catalog.hpp"

#include <cstddef>

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
    MonsterId id,
    Vec3 position) noexcept {
    const MonsterDefinition* definition = monster_definition(id);
    if (definition == nullptr) {
        return std::nullopt;
    }

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
        runtime.id = id;
        runtime.kind = kind_for(id);
        runtime.spawn = position;
        runtime.position = position;
        runtime.max_hp = definition->max_hp;
        runtime.hp = definition->max_hp;
        runtime.max_break = definition->max_break;
        runtime.break_value = definition->max_break;
        runtime.max_shield = definition->shield_points != 0
            ? definition->shield_points : 90;
        runtime.max_shield_ticks = definition->shield_duration_ticks != 0
            ? definition->shield_duration_ticks : 120;
        runtime.shield = 0;
        runtime.shield_ticks = 0;
        runtime.armor = definition->max_break > 0
            ? ArmorState::armored : ArmorState::none;
        ++active_count_;
        return MonsterHandle{
            static_cast<std::uint16_t>(index), generation};
    }
    return std::nullopt;
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
    int damage,
    float radius) noexcept {
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
        ++active_count_;
        return ProjectileHandle{
            static_cast<std::uint16_t>(index), generation};
    }
    return std::nullopt;
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

}  // namespace arpg::combat
