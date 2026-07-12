#include "combat/combat_world.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/room_bounds.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

namespace arpg::combat {

modifiers::EffectSet* CombatWorld::find_effects(
    std::size_t monster_slot) noexcept {
    const auto& monster = monsters_.slots_[monster_slot];
    for (auto& owner : effect_owners_) {
        if (owner.occupied && owner.monster_slot == monster_slot
            && owner.generation == monster.generation) {
            return &owner.effects;
        }
    }
    return nullptr;
}

modifiers::EffectSet* CombatWorld::ensure_effects(
    std::size_t monster_slot) noexcept {
    if (auto* existing = find_effects(monster_slot)) return existing;
    for (auto& owner : effect_owners_) {
        if (owner.occupied) continue;
        owner.monster_slot = monster_slot;
        owner.generation = monsters_.slots_[monster_slot].generation;
        owner.effects.clear();
        owner.occupied = true;
        return &owner.effects;
    }
    return nullptr;
}
namespace {

constexpr std::array<int, kDummyCount> kDummyHitPoints{{300, 450, 700}};
constexpr std::array<int, kDummyCount> kDummyBreakValues{{0, 0, 120}};
constexpr int kStage4PlayerMaxHp = 1000;
constexpr std::uint16_t kPlayerHurtTicks = 12;
constexpr std::uint16_t kPlayerInvulnerabilityTicks = 30;

void saturating_increment(std::uint32_t& counter) noexcept {
    if (counter != (std::numeric_limits<std::uint32_t>::max)()) {
        ++counter;
    }
}

}  // namespace

CombatWorld::CombatWorld(CombatLabConfig config) noexcept
    : legacy_config_(config), legacy_mode_(true) {
    encounter_config_.player_spawn = config.player_spawn;
    encounter_config_.initial_facing = config.initial_facing;
    initialize_runtime();
}

CombatWorld::CombatWorld(CombatEncounterConfig config) noexcept
    : encounter_config_(config), legacy_mode_(false) {
    initialize_runtime();
}

bool CombatWorld::queue_action(Action action) noexcept {
    return input_buffer_.push(action);
}

void CombatWorld::tick(MovementInput movement) noexcept {
    const bool player_frozen = player_.hit_stop_ticks != 0;
    const bool player_hurt = player_.hurt_ticks != 0;
    if (player_frozen) {
        --player_.hit_stop_ticks;
    }
    if (player_hurt) {
        --player_.hurt_ticks;
    }
    if (!player_frozen && !player_hurt) {
        simulate_player(movement);
    }

    if (player_.invulnerability_ticks != 0) {
        --player_.invulnerability_ticks;
    }

    for (MonsterRuntime& monster : monsters_.slots_) {
        if (!monster.active) {
            continue;
        }
        const bool dummy_frozen = monster.hit_stop_ticks != 0;
        if (dummy_frozen) {
            --monster.hit_stop_ticks;
        } else {
            const std::size_t index = static_cast<std::size_t>(
                &monster - monsters_.slots_.data());
            if (legacy_mode_) {
                simulate_target(index);
            } else {
                simulate_monster(index);
            }
        }
    }

    if (!player_frozen && !player_hurt && player_.hurt_ticks == 0) {
        resolve_attack_hits();
    }

    simulate_projectiles();
    simulate_hazards();

    input_buffer_.age(player_frozen || player_hurt);
    ++tick_;
}

void CombatWorld::reset() noexcept {
    initialize_runtime();

    CombatEvent reset_event{};
    reset_event.kind = CombatEventKind::reset;
    reset_event.tick = 0;
    reset_event.position = player_.position;
    emit_event(reset_event);
}

void CombatWorld::initialize_runtime() noexcept {
    initialize_player();
    monsters_.clear();
    for (auto& owner : effect_owners_) owner = {};
    projectiles_.clear();
    hazards_.clear();
    if (legacy_mode_) {
        initialize_legacy_monsters();
    } else {
        static_cast<void>(load_wave(
            encounter_config_.wave, encounter_config_.reset_player_health));
    }

    attack_ = AttackRuntime{};
    input_buffer_.clear();
    input_buffer_.reset_diagnostics();
    while (events_.try_pop().has_value()) {
    }
    tick_ = 0;
    event_overflow_count_ = 0;
    projectile_saturation_count_ = 0;
    projectile_invalid_owner_count_ = 0;
    hazard_saturation_count_ = 0;
    hazard_invalid_owner_count_ = 0;
}

void CombatWorld::initialize_player() noexcept {
    player_ = PlayerRuntime{};
    player_.position = encounter_config_.player_spawn;
    player_.facing = encounter_config_.initial_facing;
    player_.max_hp = kStage4PlayerMaxHp;
    player_.hp = player_.max_hp;
}

void CombatWorld::initialize_legacy_monsters() noexcept {
    for (std::size_t index = 0; index < kDummyCount; ++index) {
        const auto handle = monsters_.spawn(
            index == 0U ? MonsterId::fire_bomber
                        : index == 1U ? MonsterId::fire_charger
                                      : MonsterId::water_bulwark,
            legacy_config_.dummy_spawns[index]);
        if (!handle.has_value()) {
            continue;
        }
        MonsterRuntime* monster = monsters_.get(*handle);
        if (monster == nullptr) {
            continue;
        }
        monster->kind = static_cast<DummyKind>(index);
        monster->max_hp = kDummyHitPoints[index];
        monster->hp = monster->max_hp;
        monster->max_break = kDummyBreakValues[index];
        monster->break_value = monster->max_break;
        monster->armor = index == 2U ? ArmorState::armored : ArmorState::none;
    }
}

bool CombatWorld::load_wave(
    const EncounterWave& wave,
    bool reset_player_health) noexcept {
    if (wave.spawn_count > kEncounterSpawnCapacity) {
        return false;
    }

    for (std::size_t index = 0; index < wave.spawn_count; ++index) {
        if (monster_definition(wave.spawns[index].id) == nullptr) {
            return false;
        }
    }

    monsters_.clear();
    for (auto& owner : effect_owners_) owner = {};
    projectiles_.clear();
    hazards_.clear();
    for (std::size_t index = 0; index < wave.spawn_count; ++index) {
        const auto handle = monsters_.spawn(
            wave.spawns[index].id, wave.spawns[index].position);
        if (!handle.has_value()) {
            monsters_.clear();
            return false;
        }
    }
    attack_ = AttackRuntime{};
    input_buffer_.clear();
    input_buffer_.reset_diagnostics();
    while (events_.try_pop().has_value()) {
    }
    event_overflow_count_ = 0U;
    projectile_saturation_count_ = 0U;
    projectile_invalid_owner_count_ = 0U;
    hazard_saturation_count_ = 0U;
    hazard_invalid_owner_count_ = 0U;
    encounter_config_.wave = wave;
    encounter_config_.reset_player_health = reset_player_health;
    legacy_mode_ = false;
    if (reset_player_health) {
        const bool health_changed = player_.hp != player_.max_hp
                                 || player_.hurt_ticks != 0
                                 || player_.invulnerability_ticks != 0;
        player_.hp = player_.max_hp;
        player_.hurt_ticks = 0;
        player_.invulnerability_ticks = 0;

        if (health_changed) {
            CombatEvent health_reset{};
            health_reset.kind = CombatEventKind::player_health_reset;
            health_reset.tick = tick_;
            health_reset.position = player_.position;
            health_reset.value = player_.max_hp;
            emit_event(health_reset);
        }
    }
    return true;
}

std::size_t CombatWorld::active_monster_count() const noexcept {
    return monsters_.active_count();
}

std::size_t CombatWorld::active_projectile_count() const noexcept {
    return projectiles_.active_count();
}

bool CombatWorld::destroy_monster(MonsterHandle handle) noexcept {
    if (!monsters_.destroy(handle)) {
        return false;
    }
    remove_owned_projectiles(handle);
    remove_owned_hazards(handle);
    if (handle.index < attack_.hit_targets.size()) {
        attack_.hit_targets[handle.index] = false;
    }
    return true;
}

std::optional<CombatEvent> CombatWorld::try_pop_event() noexcept {
    return events_.try_pop();
}

CombatSnapshot CombatWorld::snapshot() const noexcept {
    CombatSnapshot result{};
    result.tick = tick_;
    result.player = PlayerSnapshot{
        player_.position,
        player_.velocity,
        player_.facing,
        player_.state,
        attack_.id,
        attack_.id == AttackId::none
            ? AttackPhase::finished
            : attack_phase_at(
                  *find_attack_definition(attack_.id), attack_.elapsed_ticks),
        attack_.elapsed_ticks,
        player_.combo_stage,
        player_.hit_stop_ticks,
        player_.air_attack_available,
        player_.hp,
        player_.max_hp,
        player_.hurt_ticks,
        player_.invulnerability_ticks,
    };

    for (std::size_t index = 0; index < monsters_.slots().size(); ++index) {
        const MonsterRuntime& dummy = monsters_.slots()[index];
        result.monsters[index] = MonsterSnapshot{
            dummy.active,
            dummy.generation,
            dummy.id,
            dummy.spawn,
            dummy.position,
            dummy.velocity,
            dummy.kind,
            dummy.facing,
            dummy.reaction,
            dummy.armor,
            dummy.hp,
            dummy.max_hp,
            dummy.break_value,
            dummy.max_break,
            dummy.shield,
            dummy.max_shield,
            dummy.shield_ticks,
            dummy.max_shield_ticks,
            dummy.break_window_ticks,
            dummy.hit_stop_ticks,
            dummy.ai_phase,
            dummy.attack_target_position,
            dummy.attack_vector,
        };
    }

    result.monster_count = monsters_.active_count();
    if (projectiles_.active_count() != 0U) {
        for (std::size_t index = 0; index < projectiles_.slots().size(); ++index) {
            const ProjectileRuntime& projectile = projectiles_.slots()[index];
            if (!projectile.active) {
                continue;
            }
            result.projectiles[index] = ProjectileSnapshot{
                projectile.active,
                projectile.generation,
                projectile.owner,
                projectile.position,
                projectile.velocity,
                projectile.lifetime_ticks,
                projectile.damage,
                projectile.radius,
            };
        }
    }
    result.projectile_count = projectiles_.active_count();
    if (hazards_.active_count() != 0U) {
        for (std::size_t index = 0; index < hazards_.slots().size(); ++index) {
            const HazardRuntime& hazard = hazards_.slots()[index];
            if (!hazard.active) {
                continue;
            }
            result.hazards[index] = HazardSnapshot{
                hazard.active,
                hazard.generation,
                hazard.owner,
                hazard.center,
                hazard.radius,
                hazard.telegraph_ticks,
                hazard.active_ticks,
                hazard.lifetime_ticks,
                hazard.damage_interval_ticks,
                hazard.player_latched,
                hazard.damage,
            };
        }
    }
    result.hazard_count = hazards_.active_count();
    std::size_t compatibility_index = 0U;
    for (std::size_t index = 0;
         index < monsters_.slots().size()
             && compatibility_index < kDummyCount;
         ++index) {
        if (!result.monsters[index].active) {
            continue;
        }
        result.dummies[compatibility_index] = result.monsters[index];
        ++compatibility_index;
    }

    std::size_t effect_owner_count = 0;
    std::size_t active_effect_count = 0;
    std::uint32_t effect_overflow_count = 0;
    std::uint32_t effect_command_overflow_count = 0;
    for (const auto& owner : effect_owners_) {
        if (!owner.occupied) continue;
        ++effect_owner_count;
        active_effect_count += owner.effects.active_count();
        effect_overflow_count += owner.effects.diagnostics().effect_overflows;
        effect_command_overflow_count +=
            owner.effects.diagnostics().command_overflows;
    }
    result.diagnostics = CombatDiagnostics{
        input_buffer_.size(),
        input_buffer_.expired_count(),
        input_buffer_.overflow_count(),
        event_overflow_count_,
        projectile_saturation_count_,
        projectile_invalid_owner_count_,
        hazard_saturation_count_,
        hazard_invalid_owner_count_,
        effect_owner_count,
        active_effect_count,
        effect_overflow_count,
        effect_command_overflow_count,
    };
    return result;
}

void CombatWorld::apply_player_damage(
    int damage,
    Vec3 source_position,
    FeedbackLevel feedback) noexcept {
    if (damage <= 0 || player_.invulnerability_ticks != 0) {
        return;
    }

    player_.hp = damage >= player_.hp ? 1 : player_.hp - damage;
    player_.hurt_ticks = kPlayerHurtTicks;
    player_.invulnerability_ticks = kPlayerInvulnerabilityTicks;
    player_.velocity.x = 0.0F;
    player_.velocity.y = 0.0F;

    CombatEvent hit{};
    hit.kind = CombatEventKind::player_hit;
    hit.tick = tick_;
    hit.hit_count = 1;
    hit.feedback = feedback;
    hit.position = source_position;
    hit.value = damage;
    emit_event(hit);

    CombatEvent hurt_started{};
    hurt_started.kind = CombatEventKind::player_hurt_started;
    hurt_started.tick = tick_;
    hurt_started.hit_count = 1;
    hurt_started.feedback = feedback;
    hurt_started.position = source_position;
    hurt_started.value = damage;
    emit_event(hurt_started);
}

bool CombatWorld::spawn_projectile(
    MonsterHandle owner,
    Vec3 position,
    Vec3 velocity,
    std::uint16_t lifetime_ticks,
    int damage,
    float radius) noexcept {
    if (monsters_.get(owner) == nullptr) {
        saturating_increment(projectile_invalid_owner_count_);
        return false;
    }
    if (!projectiles_.spawn(
            owner, position, velocity, lifetime_ticks, damage, radius)
             .has_value()) {
        saturating_increment(projectile_saturation_count_);
        return false;
    }
    return true;
}

void CombatWorld::remove_owned_projectiles(MonsterHandle owner) noexcept {
    for (std::size_t index = 0; index < kProjectileCapacity; ++index) {
        const ProjectileRuntime* projectile = projectiles_.get(ProjectileHandle{
            static_cast<std::uint16_t>(index),
            projectiles_.slots()[index].generation});
        if (projectile == nullptr) {
            continue;
        }
        if (projectile->owner.index != owner.index
            || projectile->owner.generation != owner.generation) {
            continue;
        }
        static_cast<void>(projectiles_.destroy(ProjectileHandle{
            static_cast<std::uint16_t>(index), projectile->generation}));
    }
}

bool CombatWorld::spawn_hazard(
    MonsterHandle owner,
    Vec3 center,
    float radius,
    std::uint16_t telegraph_ticks,
    std::uint16_t active_ticks,
    std::uint16_t damage_interval_ticks,
    int damage) noexcept {
    if (monsters_.get(owner) == nullptr) {
        saturating_increment(hazard_invalid_owner_count_);
        return false;
    }
    if (!hazards_.spawn(owner, center, radius, telegraph_ticks, active_ticks,
                        damage_interval_ticks, damage).has_value()) {
        saturating_increment(hazard_saturation_count_);
        return false;
    }
    return true;
}

void CombatWorld::remove_owned_hazards(MonsterHandle owner) noexcept {
    for (std::size_t index = 0; index < kHazardCapacity; ++index) {
        const HazardRuntime& hazard = hazards_.slots()[index];
        if (!hazard.active || hazard.persists_after_owner_death
            || hazard.owner.index != owner.index
            || hazard.owner.generation != owner.generation) {
            continue;
        }
        static_cast<void>(hazards_.destroy(HazardHandle{
            static_cast<std::uint16_t>(index), hazard.generation}));
    }
}

void CombatWorld::simulate_projectiles() noexcept {
    constexpr float player_radius_x = 0.45F;
    constexpr float player_radius_y = 0.35F;
    constexpr float player_radius_z = 1.60F;

    for (std::size_t index = 0; index < kProjectileCapacity; ++index) {
        ProjectileRuntime* active = projectiles_.get(ProjectileHandle{
            static_cast<std::uint16_t>(index),
            projectiles_.slots()[index].generation});
        if (active == nullptr) {
            continue;
        }
        ProjectileRuntime& projectile = *active;
        const MonsterHandle owner = projectile.owner;
        const MonsterRuntime* owner_runtime = monsters_.get(owner);
        if (owner_runtime == nullptr || owner_runtime->hp <= 0
            || owner_runtime->reaction == ReactionState::defeated) {
            static_cast<void>(projectiles_.destroy(ProjectileHandle{
                static_cast<std::uint16_t>(index), projectile.generation}));
            continue;
        }

        projectile.position.x += projectile.velocity.x;
        projectile.position.y += projectile.velocity.y;
        projectile.position.z += projectile.velocity.z;
        if (projectile.lifetime_ticks != 0) {
            --projectile.lifetime_ticks;
        }

        const float dx = projectile.position.x - player_.position.x;
        const float dy = projectile.position.y - player_.position.y;
        const float dz = projectile.position.z - player_.position.z;
        const float rx = player_radius_x + projectile.radius;
        const float ry = player_radius_y + projectile.radius;
        const float rz = player_radius_z + projectile.radius;
        const bool hit_player = std::fabs(dx) <= rx && std::fabs(dy) <= ry
                              && std::fabs(dz) <= rz;
        const bool outside = projectile.position.x < room_bounds::min_x
                          || projectile.position.x > room_bounds::max_x
                          || projectile.position.y < room_bounds::min_y
                          || projectile.position.y > room_bounds::max_y;
        const bool expired = projectile.lifetime_ticks == 0;
        if (hit_player) {
            apply_player_damage(
                projectile.damage, projectile.position,
                FeedbackLevel::medium);
        }
        if (hit_player || outside || expired) {
            static_cast<void>(projectiles_.destroy(ProjectileHandle{
                static_cast<std::uint16_t>(index), projectile.generation}));
        }
    }
}

void CombatWorld::simulate_hazards() noexcept {
    constexpr float kPlayerRadiusX = 0.45F;
    constexpr float kPlayerRadiusY = 0.35F;
    constexpr float kPlayerRadiusZ = 1.60F;
    for (std::size_t index = 0; index < kHazardCapacity; ++index) {
        HazardRuntime* active = hazards_.get(HazardHandle{
            static_cast<std::uint16_t>(index), hazards_.slots()[index].generation});
        if (active == nullptr) {
            continue;
        }
        HazardRuntime& hazard = *active;
        const MonsterRuntime* owner = monsters_.get(hazard.owner);
        if ((!hazard.persists_after_owner_death &&
             (owner == nullptr || owner->hp <= 0
              || owner->reaction == ReactionState::defeated))) {
            static_cast<void>(hazards_.destroy(HazardHandle{
                static_cast<std::uint16_t>(index), hazard.generation}));
            continue;
        }
        if (hazard.lifetime_ticks != 0U) {
            --hazard.lifetime_ticks;
        }
        if (hazard.telegraph_ticks != 0U) {
            --hazard.telegraph_ticks;
        } else {
            if (hazard.damage_cooldown_ticks != 0U) {
                --hazard.damage_cooldown_ticks;
                if (hazard.damage_cooldown_ticks == 0U) {
                    hazard.player_latched = false;
                }
            }
            const float dx = hazard.center.x - player_.position.x;
            const float dy = hazard.center.y - player_.position.y;
            const float dz = hazard.center.z - player_.position.z;
            const bool intersects = std::fabs(dx) <= hazard.radius + kPlayerRadiusX
                && std::fabs(dy) <= hazard.radius + kPlayerRadiusY
                && std::fabs(dz) <= hazard.radius + kPlayerRadiusZ;
            if (intersects && !hazard.player_latched) {
                apply_player_damage(hazard.damage, hazard.center,
                                    FeedbackLevel::heavy);
                hazard.player_latched = true;
                hazard.damage_cooldown_ticks = hazard.damage_interval_ticks;
            }
            if (hazard.active_ticks != 0U) {
                --hazard.active_ticks;
            }
        }
        if (hazard.lifetime_ticks == 0U || hazard.active_ticks == 0U) {
            static_cast<void>(hazards_.destroy(HazardHandle{
                static_cast<std::uint16_t>(index), hazard.generation}));
        }
    }
}

}  // namespace arpg::combat
