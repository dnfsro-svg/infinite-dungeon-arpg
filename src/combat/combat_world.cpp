#include "combat/combat_world.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/room_bounds.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

namespace arpg::combat {

namespace {

constexpr std::int64_t fixed_floor(std::int64_t value) noexcept {
    const std::int64_t quotient = value / modifiers::kFixedOne;
    return quotient - (value < 0 && value % modifiers::kFixedOne != 0 ? 1 : 0);
}

bool checked_add(
    std::int64_t left, std::int64_t right, std::int64_t& result) noexcept {
    const auto maximum = (std::numeric_limits<std::int64_t>::max)();
    const auto minimum = (std::numeric_limits<std::int64_t>::min)();
    if ((right > 0 && left > maximum - right)
        || (right < 0 && left < minimum - right)) {
        return false;
    }
    result = left + right;
    return true;
}

bool checked_multiply(
    std::int64_t left, std::int64_t right, std::int64_t& result) noexcept {
    const auto maximum = (std::numeric_limits<std::int64_t>::max)();
    const auto minimum = (std::numeric_limits<std::int64_t>::min)();
    if (left == 0 || right == 0) {
        result = 0;
        return true;
    }
    if ((left == -1 && right == minimum)
        || (right == -1 && left == minimum)) {
        return false;
    }
    if (left > 0) {
        if ((right > 0 && left > maximum / right)
            || (right < 0 && right < minimum / left)) return false;
    } else if ((right > 0 && left < minimum / right)
               || (right < 0 && left < maximum / right)) {
        return false;
    }
    result = left * right;
    return true;
}

std::optional<std::int64_t> fixed_scale_floor(
    std::int64_t value, std::int64_t factor) noexcept {
    std::int64_t product{};
    if (!checked_multiply(value, factor, product)) return std::nullopt;
    return fixed_floor(product);
}

bool validate_player_build_fields(const PlayerCombatBuild& build) noexcept {
    const auto& values = build.values;
    if (!values.valid || build.weapon_physical < 0
        || build.local_attack_speed_bp < 0 || values.armor < 0
        || values.evasion < 0 || values.melee_damage < 0
        || values.max_health < 0 || values.max_health_more < 0
        || values.max_barrier < 0 || values.damage_taken < 0
        || values.movement_speed < 0 || values.attack_speed < 0
        || values.impulse_scale < 0 || values.jump_speed < 0
        || values.air_control < 0) {
        return false;
    }
    for (const auto factor : values.damage_increased) {
        if (factor < 0) return false;
    }
    for (const auto bonus : values.damage_reduction_cap_bonus) {
        if (bonus < 0) return false;
    }
    return true;
}

bool valid_player_build(const PlayerCombatBuild& build) noexcept;

}  // namespace

std::optional<DamagePacket> build_player_hit_packet(
    int base_physical, const PlayerCombatBuild& build) noexcept {
    if (base_physical < 0 || !validate_player_build_fields(build)) {
        return std::nullopt;
    }

    DamagePacket packet{};
    const auto& values = build.values;
    std::int64_t physical{};
    if (!checked_add(base_physical, build.weapon_physical, physical)
        || !checked_add(physical,
                        fixed_floor(values.flat_damage[
                            modifiers::damage_index(
                                modifiers::DamageType::physical)]),
                        physical)) {
        return std::nullopt;
    }
    auto scaled = fixed_scale_floor(
        physical, values.damage_increased[
                      modifiers::damage_index(modifiers::DamageType::physical)]);
    if (!scaled.has_value()) return std::nullopt;
    scaled = fixed_scale_floor(*scaled, values.melee_damage);
    if (!scaled.has_value()) return std::nullopt;
    if (*scaled < (std::numeric_limits<int>::min)()
        || *scaled > (std::numeric_limits<int>::max)()) {
        return std::nullopt;
    }
    packet.amount[modifiers::damage_index(modifiers::DamageType::physical)] =
        static_cast<int>(*scaled);
    for (std::size_t index = 1; index < modifiers::kDamageTypeCount; ++index) {
        scaled = fixed_scale_floor(
            fixed_floor(values.flat_damage[index]),
            values.damage_increased[index]);
        if (!scaled.has_value()) return std::nullopt;
        scaled = fixed_scale_floor(*scaled, values.melee_damage);
        if (!scaled.has_value()
            || *scaled < (std::numeric_limits<int>::min)()
            || *scaled > (std::numeric_limits<int>::max)()) {
            return std::nullopt;
        }
        packet.amount[index] = static_cast<int>(*scaled);
    }
    return packet;
}

std::optional<int> resolve_player_damage(
    DamagePacket packet, const PlayerCombatBuild& build) noexcept {
    if (!validate_player_build_fields(build) || !valid_player_build(build)) {
        return std::nullopt;
    }

    const auto checked_add = [](std::int64_t left, std::int64_t right,
                                std::int64_t& result) noexcept {
        const auto maximum = (std::numeric_limits<std::int64_t>::max)();
        if (right > 0 && left > maximum - right) return false;
        result = left + right;
        return true;
    };
    const auto checked_multiply = [](std::int64_t left, std::int64_t right,
                                     std::int64_t& result) noexcept {
        const auto maximum = (std::numeric_limits<std::int64_t>::max)();
        if (left != 0 && right > maximum / left) return false;
        result = left * right;
        return true;
    };
    const auto reduced_component = [&checked_multiply](
        int raw, std::int32_t reduction) noexcept -> std::optional<std::int64_t> {
        if (raw <= 0) return std::int64_t{0};
        const std::int64_t multiplier = modifiers::kFixedOne - reduction;
        std::int64_t product{};
        if (!checked_multiply(raw, multiplier, product)) return std::nullopt;
        return product / modifiers::kFixedOne
            + (product % modifiers::kFixedOne != 0 ? 1 : 0);
    };

    std::int64_t total = 0;
    const auto physical = reduced_component(
        packet.amount[modifiers::damage_index(modifiers::DamageType::physical)],
        modifiers::rating_to_basis_points(build.values.armor));
    if (!physical.has_value() || !checked_add(total, *physical, total)) {
        return std::nullopt;
    }
    for (std::size_t element = 0; element < modifiers::kElementCount; ++element) {
        if (build.values.damage_reduction_cap_bonus[element] < 0) {
            return std::nullopt;
        }
        const std::int64_t uncapped = std::int64_t{7500}
            + build.values.damage_reduction_cap_bonus[element];
        const auto cap = static_cast<std::int32_t>(
            std::min<std::int64_t>(9500, uncapped));
        const auto reduction = std::clamp(
            build.values.damage_reduction[element], std::int32_t{-6000}, cap);
        const auto component = reduced_component(
            packet.amount[element + 1U], reduction);
        if (!component.has_value()
            || !checked_add(total, *component, total)) {
            return std::nullopt;
        }
    }
    if (build.values.damage_taken < 0) return std::nullopt;
    std::int64_t scaled{};
    if (!checked_multiply(total, build.values.damage_taken, scaled)) {
        return std::nullopt;
    }
    scaled /= modifiers::kFixedOne;
    if (scaled > (std::numeric_limits<int>::max)()) return std::nullopt;
    return static_cast<int>(scaled);
}

std::uint16_t scaled_phase_ticks(
    std::uint16_t base, std::int64_t attack_speed) noexcept {
    if (base == 0U) return 1U;
    const auto speed = std::max<std::int64_t>(1, attack_speed);
    const auto scaled = static_cast<std::int64_t>(base)
        * modifiers::kFixedOne / speed;
    return static_cast<std::uint16_t>(std::max<std::int64_t>(1, scaled));
}

namespace {

constexpr std::array<int, kDummyCount> kDummyHitPoints{{300, 450, 700}};
constexpr std::array<int, kDummyCount> kDummyBreakValues{{0, 0, 120}};
constexpr int kStage4PlayerMaxHp = 1000;
constexpr std::uint16_t kPlayerHurtTicks = 12;
constexpr std::uint16_t kPlayerInvulnerabilityTicks = 30;

struct DerivedPlayerBuild final {
    int max_hp{};
    int max_barrier{};
    std::array<std::int32_t, modifiers::kElementCount> damage_reduction{};
    std::array<std::int32_t, modifiers::kElementCount> damage_reduction_cap{};
    std::int64_t armor{};
    std::int64_t evasion{};
    std::int32_t armor_reduction_bp{};
    std::int32_t evasion_rate_bp{};
};

bool checked_nonnegative_multiply(
    std::int64_t left, std::int64_t right, std::int64_t& result) noexcept {
    if (left < 0 || right < 0) return false;
    const auto maximum = (std::numeric_limits<std::int64_t>::max)();
    if (left != 0 && right > maximum / left) return false;
    result = left * right;
    return true;
}

bool derive_player_build(
    const PlayerCombatBuild& build, DerivedPlayerBuild& result) noexcept {
    const auto& values = build.values;
    if (!validate_player_build_fields(build)) return false;

    const std::int64_t health_flat = values.max_health / modifiers::kFixedOne;
    if (health_flat > (std::numeric_limits<std::int64_t>::max)()
                          - kStage4PlayerMaxHp) {
        return false;
    }
    const std::int64_t health_base = kStage4PlayerMaxHp + health_flat;
    std::int64_t health_product{};
    if (!checked_nonnegative_multiply(
            health_base, values.max_health_more, health_product)) {
        return false;
    }
    const std::int64_t scaled_health =
        health_product / modifiers::kFixedOne;
    if (scaled_health > (std::numeric_limits<int>::max)()) return false;
    result.max_hp = static_cast<int>(std::max<std::int64_t>(1, scaled_health));

    const std::int64_t barrier = values.max_barrier / modifiers::kFixedOne;
    if (barrier > (std::numeric_limits<int>::max)()) return false;
    result.max_barrier = static_cast<int>(barrier);

    const std::int64_t local_multiplier = modifiers::kFixedOne
        + static_cast<std::int64_t>(build.local_attack_speed_bp);
    std::int64_t attack_speed_product{};
    if (!checked_nonnegative_multiply(
            values.attack_speed, local_multiplier, attack_speed_product)) {
        return false;
    }
    const std::int64_t effective_attack_speed = std::max<std::int64_t>(
        1, attack_speed_product / modifiers::kFixedOne);

    constexpr std::array<AttackId, 5> attack_ids{{
        AttackId::j1, AttackId::j2, AttackId::j3,
        AttackId::launcher, AttackId::air_j}};
    for (const AttackId id : attack_ids) {
        const AttackDefinition* definition = find_attack_definition(id);
        if (definition == nullptr) return false;
        const auto phase_ticks_fit = [effective_attack_speed](
            std::uint16_t base) noexcept {
            const std::int64_t scaled = static_cast<std::int64_t>(base)
                * modifiers::kFixedOne / effective_attack_speed;
            return scaled <= (std::numeric_limits<std::uint16_t>::max)();
        };
        if (!phase_ticks_fit(definition->startup_ticks)
            || !phase_ticks_fit(definition->recovery_ticks)) {
            return false;
        }
        const auto packet = build_player_hit_packet(definition->damage, build);
        if (!packet.has_value()) return false;
        std::int64_t packet_total = 0;
        for (const int amount : packet->amount) {
            if (amount <= 0) continue;
            if (!checked_add(packet_total, amount, packet_total)) return false;
        }
        if (packet_total > (std::numeric_limits<int>::max)()) return false;
    }

    result.armor = values.armor;
    result.evasion = values.evasion;
    result.armor_reduction_bp = modifiers::rating_to_basis_points(values.armor);
    result.evasion_rate_bp = modifiers::rating_to_basis_points(values.evasion);
    for (std::size_t index = 0; index < modifiers::kElementCount; ++index) {
        const std::int64_t cap = std::min<std::int64_t>(
            9500, std::int64_t{7500} + values.damage_reduction_cap_bonus[index]);
        result.damage_reduction_cap[index] = static_cast<std::int32_t>(cap);
        result.damage_reduction[index] = std::clamp(
            values.damage_reduction[index], std::int32_t{-6000},
            result.damage_reduction_cap[index]);
    }
    return true;
}

bool valid_player_build(const PlayerCombatBuild& build) noexcept {
    DerivedPlayerBuild ignored{};
    return derive_player_build(build, ignored);
}

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
    DerivedPlayerBuild derived{};
    if (!derive_player_build(encounter_config_.player_build, derived)) {
        encounter_config_.player_build = PlayerCombatBuild{};
    }
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

void CombatWorld::apply_player_build(PlayerCombatBuild build) noexcept {
    DerivedPlayerBuild derived{};
    if (!derive_player_build(build, derived)) return;

    encounter_config_.player_build = build;
    player_.max_hp = derived.max_hp;
    player_.max_barrier = derived.max_barrier;
    player_.damage_reduction = derived.damage_reduction;
    player_.damage_reduction_cap = derived.damage_reduction_cap;
    player_.armor = derived.armor;
    player_.evasion = derived.evasion;
    player_.armor_reduction_bp = derived.armor_reduction_bp;
    player_.evasion_rate_bp = derived.evasion_rate_bp;
    player_.hp = std::clamp(player_.hp, 0, player_.max_hp);
    player_.barrier = std::clamp(player_.barrier, 0, player_.max_barrier);
}

void CombatWorld::initialize_runtime() noexcept {
    evasion_rng_ = core::DeterministicRng{encounter_config_.evasion_seed};
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
    DerivedPlayerBuild derived{};
    if (!derive_player_build(encounter_config_.player_build, derived)) {
        encounter_config_.player_build = PlayerCombatBuild{};
        static_cast<void>(derive_player_build(
            encounter_config_.player_build, derived));
    }
    player_.max_hp = derived.max_hp;
    player_.max_barrier = derived.max_barrier;
    player_.damage_reduction = derived.damage_reduction;
    player_.damage_reduction_cap = derived.damage_reduction_cap;
    player_.armor = derived.armor;
    player_.evasion = derived.evasion;
    player_.armor_reduction_bp = derived.armor_reduction_bp;
    player_.evasion_rate_bp = derived.evasion_rate_bp;
    player_.barrier = player_.max_barrier;
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
        player_.barrier = player_.max_barrier;
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

void CombatWorld::apply_player_damage(
    DamagePacket packet,
    DamageDelivery delivery,
    Vec3 source_position,
    FeedbackLevel feedback) noexcept {
    const bool has_positive_component = std::any_of(
        packet.amount.begin(), packet.amount.end(), [](int value) noexcept {
            return value > 0;
        });
    const auto resolved = resolve_player_damage(
        packet, encounter_config_.player_build);
    if (!resolved.has_value()) return;

    if (delivery == DamageDelivery::direct && has_positive_component
        && player_.evasion_rate_bp > 0) {
        const auto roll = evasion_rng_.next_bounded(modifiers::kFixedOne);
        if (roll.has_value()
            && *roll < static_cast<std::uint64_t>(player_.evasion_rate_bp)) {
            return;
        }
    }

    if (*resolved <= 0 || player_.invulnerability_ticks != 0) {
        return;
    }
    const int damage = *resolved;

    const int absorbed = std::min(player_.barrier, damage);
    player_.barrier -= absorbed;
    const int hp_damage = damage - absorbed;
    player_.hp = hp_damage >= player_.hp ? 1 : player_.hp - hp_damage;
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

void CombatWorld::apply_player_damage(
    int damage,
    Vec3 source_position,
    FeedbackLevel feedback) noexcept {
    apply_player_damage(DamagePacket{damage}, DamageDelivery::direct,
                        source_position, feedback);
}

bool CombatWorld::spawn_projectile(
    MonsterHandle owner,
    Vec3 position,
    Vec3 velocity,
    std::uint16_t lifetime_ticks,
    DamagePacket damage,
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

bool CombatWorld::spawn_projectile(
    MonsterHandle owner,
    Vec3 position,
    Vec3 velocity,
    std::uint16_t lifetime_ticks,
    int damage,
    float radius) noexcept {
    return spawn_projectile(owner, position, velocity, lifetime_ticks,
                            DamagePacket{damage}, radius);
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
    DamagePacket damage) noexcept {
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

bool CombatWorld::spawn_hazard(
    MonsterHandle owner,
    Vec3 center,
    float radius,
    std::uint16_t telegraph_ticks,
    std::uint16_t active_ticks,
    std::uint16_t damage_interval_ticks,
    int damage) noexcept {
    return spawn_hazard(owner, center, radius, telegraph_ticks, active_ticks,
                        damage_interval_ticks, DamagePacket{damage});
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
                projectile.damage, DamageDelivery::direct,
                projectile.position,
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
                apply_player_damage(hazard.damage,
                                    DamageDelivery::ground_or_environment,
                                    hazard.center,
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
