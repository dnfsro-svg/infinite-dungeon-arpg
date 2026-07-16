#include "combat/combat_world.hpp"

#include "abyss/abyss_rules.hpp"
#include "combat/attack_catalog.hpp"
#include "combat/combat_scaling.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_affix_generation.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/room_bounds.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

namespace arpg::combat {

namespace {

const MonsterAffixTierValues* affix_values(
    const MonsterAffixSet& affixes, MonsterAffixId id) noexcept {
    for (std::size_t index = 0; index < affixes.count
         && index < affixes.values.size(); ++index) {
        const MonsterAffixInstance& instance = affixes.values[index];
        if (instance.id != id) continue;
        const MonsterAffixDefinition* definition =
            monster_affix_definition(instance.id);
        const std::size_t tier = static_cast<std::size_t>(instance.tier);
        if (definition != nullptr && tier < definition->tiers.size()) {
            return &definition->tiers[tier];
        }
    }
    return nullptr;
}

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

std::optional<DamagePacket> build_player_hit_packet_checked_unvalidated(
    int base_physical, const PlayerCombatBuild& build) noexcept {
    if (base_physical < 0) return std::nullopt;

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

}  // namespace

std::optional<DamagePacket> build_player_hit_packet(
    int base_physical, const PlayerCombatBuild& build) noexcept {
    if (!valid_player_build(build)) return std::nullopt;
    return build_player_hit_packet_checked_unvalidated(base_physical, build);
}

std::optional<int> resolve_player_damage(
    DamagePacket packet, const PlayerCombatBuild& build) noexcept {
    if (!valid_player_build(build)) return std::nullopt;

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
        const auto packet = build_player_hit_packet_checked_unvalidated(
            definition->damage, build);
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

constexpr std::uint16_t kDefeatedRespawnTicks = 90U;

struct DirectHitAffixValues final {
    std::int32_t added_water_bp{};
    std::int32_t slow_bp{};
    std::uint16_t slow_ticks{};
    int corrosion_damage{};
    std::uint16_t corrosion_ticks{};
};

[[nodiscard]] DirectHitAffixValues direct_hit_affix_values(
    const MonsterAffixSet& affixes) noexcept {
    DirectHitAffixValues result{};
    const std::size_t count = std::min<std::size_t>(
        affixes.count, affixes.values.size());
    for (std::size_t index = 0U; index < count; ++index) {
        const MonsterAffixInstance& instance = affixes.values[index];
        const MonsterAffixDefinition* definition = monster_affix_definition(
            instance.id);
        const std::size_t tier = static_cast<std::size_t>(instance.tier);
        if (definition == nullptr || tier >= definition->tiers.size()) continue;
        const MonsterAffixTierValues& values = definition->tiers[tier];
        if (instance.id == MonsterAffixId::chilling
            && values.secondary_bp >= result.slow_bp) {
            result.added_water_bp = values.primary_bp;
            result.slow_bp = values.secondary_bp;
            result.slow_ticks = values.duration_ticks;
        } else if (instance.id == MonsterAffixId::chaos_corrosion
                   && values.damage >= result.corrosion_damage) {
            result.corrosion_damage = values.damage;
            result.corrosion_ticks = values.duration_ticks;
        }
    }
    return result;
}

[[nodiscard]] int positive_packet_total(DamagePacket packet) noexcept {
    std::int64_t total{};
    for (const int amount : packet.amount) {
        total += std::max(amount, 0);
    }
    return static_cast<int>(std::min<std::int64_t>(total,
        (std::numeric_limits<int>::max)()));
}

[[nodiscard]] int ceil_basis_points(int value, std::int32_t basis_points) noexcept {
    if (value <= 0 || basis_points <= 0) return 0;
    const std::int64_t product = static_cast<std::int64_t>(value)
        * static_cast<std::int64_t>(basis_points);
    const std::int64_t rounded = (product + 9999) / 10000;
    return static_cast<int>(std::min<std::int64_t>(rounded,
        (std::numeric_limits<int>::max)()));
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
    return player_.hp > 0 && input_buffer_.push(action);
}

void CombatWorld::tick(MovementInput movement) noexcept {
    if (player_.hp == 0) {
        attack_ = AttackRuntime{};
        input_buffer_.clear();
        ++tick_;
        return;
    }
    tick_player_status();
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
        const std::size_t index = static_cast<std::size_t>(
            &monster - monsters_.slots_.data());
        if (monster.reaction == ReactionState::defeated
            || monster.ai_phase == MonsterAiPhase::defeated) {
            if (legacy_mode_) {
                if (monster.hit_stop_ticks != 0U) {
                    --monster.hit_stop_ticks;
                } else {
                    simulate_target(index);
                }
            }
            continue;
        }
        tick_monster_affix_resources(monster);
        tick_active_affixes(index, monster);
        if (monster.affix_warning == MonsterAffixWarning::blink) {
            continue;
        }
        const bool dummy_frozen = monster.hit_stop_ticks != 0;
        if (dummy_frozen) {
            --monster.hit_stop_ticks;
        } else {
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
    player_.max_hp = scale_basis_points(
        derived.max_hp,
        encounter_config_.abyss.player_max_health_bp,
        BasisPointRounding::ceil);
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

void CombatWorld::restore_player_resources(int hp, int barrier) noexcept {
    const int restored_hp = scale_basis_points(
        hp,
        encounter_config_.abyss.player_resource_restore_bp,
        BasisPointRounding::ceil);
    const int restored_barrier = scale_basis_points(
        barrier,
        encounter_config_.abyss.player_resource_restore_bp,
        BasisPointRounding::ceil);
    player_.hp = restored_hp >= player_.max_hp - player_.hp
        ? player_.max_hp
        : player_.hp + restored_hp;
    player_.barrier = restored_barrier >= player_.max_barrier - player_.barrier
        ? player_.max_barrier
        : player_.barrier + restored_barrier;
}

void CombatWorld::clear_abyss_rule_preserving_resources() noexcept {
    DerivedPlayerBuild derived{};
    if (!derive_player_build(encounter_config_.player_build, derived)) return;

    const int old_max_hp = player_.max_hp;
    const int old_hp = player_.hp;
    const bool alive = old_hp > 0;
    encounter_config_.abyss = {};
    player_.max_hp = derived.max_hp;
    player_.max_barrier = derived.max_barrier;
    player_.damage_reduction = derived.damage_reduction;
    player_.damage_reduction_cap = derived.damage_reduction_cap;
    player_.armor = derived.armor;
    player_.evasion = derived.evasion;
    player_.armor_reduction_bp = derived.armor_reduction_bp;
    player_.evasion_rate_bp = derived.evasion_rate_bp;
    player_.hp = abyss::map_resource_ratio(
        old_hp, old_max_hp, player_.max_hp, alive).value_or(
            std::clamp(old_hp, 0, player_.max_hp));
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
    const int base_max_hp = derived.max_hp;
    player_.max_hp = scale_basis_points(
        base_max_hp,
        encounter_config_.abyss.player_max_health_bp,
        BasisPointRounding::ceil);
    player_.max_barrier = derived.max_barrier;
    player_.damage_reduction = derived.damage_reduction;
    player_.damage_reduction_cap = derived.damage_reduction_cap;
    player_.armor = derived.armor;
    player_.evasion = derived.evasion;
    player_.armor_reduction_bp = derived.armor_reduction_bp;
    player_.evasion_rate_bp = derived.evasion_rate_bp;
    player_.barrier = 0;
    player_.hp = abyss::map_resource_ratio(
        base_max_hp, base_max_hp, player_.max_hp, true).value_or(
            player_.max_hp);
    const int mapped_hp = player_.hp;
    player_.hp = 0;
    restore_player_resources(mapped_hp, player_.max_barrier);
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
            wave.spawns[index], encounter_config_.abyss);
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
        player_.hp = 0;
        player_.barrier = 0;
        restore_player_resources(player_.max_hp, player_.max_barrier);
        player_.hurt_ticks = 0;
        player_.invulnerability_ticks = 0;

        if (health_changed) {
            CombatEvent health_reset{};
            health_reset.kind = CombatEventKind::player_health_reset;
            health_reset.tick = tick_;
            health_reset.position = player_.position;
            health_reset.value = player_.hp;
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

bool CombatWorld::player_defeated() const noexcept {
    return player_.hp == 0;
}

bool CombatWorld::apply_player_damage(
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
    if (!resolved.has_value()) return false;

    if (delivery == DamageDelivery::direct && has_positive_component
        && player_.evasion_rate_bp > 0) {
        const auto roll = evasion_rng_.next_bounded(modifiers::kFixedOne);
        if (roll.has_value()
            && *roll < static_cast<std::uint64_t>(player_.evasion_rate_bp)) {
            return false;
        }
    }

    if (*resolved <= 0 || player_.hp == 0
            || player_.invulnerability_ticks != 0) {
        return false;
    }
    const int damage = *resolved;

    const int absorbed = std::min(player_.barrier, damage);
    player_.barrier -= absorbed;
    const int hp_damage = damage - absorbed;
    player_.hp = hp_damage >= player_.hp ? 0 : player_.hp - hp_damage;
    if (player_.hp == 0) {
        attack_ = AttackRuntime{};
        input_buffer_.clear();
    }
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

    if (player_.hp == 0) {
        CombatEvent defeated{};
        defeated.kind = CombatEventKind::player_defeated;
        defeated.tick = tick_;
        defeated.position = player_.position;
        emit_event(defeated);
    }
    return true;
}

void CombatWorld::defeat_monster(
    std::size_t slot, AttackId attack, bool reward_eligible) noexcept {
    if (slot >= monsters_.slots_.size()) return;
    MonsterRuntime& monster = monsters_.slots_[slot];
    if (!monster.active || monster.reaction == ReactionState::defeated) return;

    const MonsterHandle owner{static_cast<std::uint16_t>(slot),
                              monster.generation};
    const DefeatPayload payload{
        monster.id, monster.spawn_ordinal,
        monster_affix_danger_score(monster.affixes), reward_eligible};
    const Vec3 position = monster.position;

    if (const MonsterAffixTierValues* death = affix_values(
            monster.affixes, MonsterAffixId::death_blast)) {
        DamagePacket damage{};
        damage.amount[modifiers::damage_index(modifiers::DamageType::physical)] =
            death->damage;
        damage = scale_monster_outgoing_damage(
            damage, encounter_config_.abyss.monster_damage_bp);
        if (spawn_hazard(owner, HazardKind::death_blast, position,
                         death->radius, death->interval_ticks, 1U, 1U,
                         damage, true)) {
            CombatEvent warning{};
            warning.kind = CombatEventKind::affix_death_warning;
            warning.tick = tick_;
            warning.target_index = static_cast<std::uint8_t>(slot);
            warning.position = position;
            warning.monster_id = payload.monster_id;
            warning.spawn_ordinal = payload.spawn_ordinal;
            warning.affix_score = payload.affix_score;
            warning.reward_eligible = payload.reward_eligible;
            emit_event(warning);
        }
    }

    remove_owned_projectiles(owner);
    remove_owned_hazards(owner);
    monster.reaction = ReactionState::defeated;
    monster.ai_phase = MonsterAiPhase::defeated;
    monster.reaction_ticks = kDefeatedRespawnTicks;
    monster.break_window_ticks = 0U;
    monster.velocity = Vec3{};

    CombatEvent defeated{};
    defeated.kind = CombatEventKind::defeated;
    defeated.tick = tick_;
    defeated.attack = attack;
    defeated.target_index = static_cast<std::uint8_t>(slot);
    if (const AttackDefinition* definition = find_attack_definition(attack)) {
        defeated.feedback = definition->feedback;
    }
    defeated.position = position;
    defeated.monster_id = payload.monster_id;
    defeated.spawn_ordinal = payload.spawn_ordinal;
    defeated.affix_score = payload.affix_score;
    defeated.reward_eligible = payload.reward_eligible;
    emit_event(defeated);
}

bool CombatWorld::apply_player_damage(
    int damage,
    Vec3 source_position,
    FeedbackLevel feedback) noexcept {
    return apply_player_damage(DamagePacket{damage}, DamageDelivery::direct,
                               source_position, feedback);
}

bool CombatWorld::apply_monster_direct_hit(
    std::size_t slot,
    DamagePacket packet,
    Vec3 source_position,
    FeedbackLevel feedback,
    bool trigger_chain) noexcept {
    if (slot >= monsters_.slots_.size()) return false;
    const MonsterRuntime& monster = monsters_.slots_[slot];
    if (!monster.active || monster.hp <= 0
        || monster.reaction == ReactionState::defeated) return false;

    const DirectHitAffixValues values = direct_hit_affix_values(monster.affixes);
    const int raw_total = positive_packet_total(packet);
    const int added_water = ceil_basis_points(raw_total, values.added_water_bp);
    const std::size_t water = modifiers::damage_index(modifiers::DamageType::water);
    const std::int64_t water_total = static_cast<std::int64_t>(packet.amount[water])
        + static_cast<std::int64_t>(added_water);
    packet.amount[water] = static_cast<int>(std::clamp(water_total,
        static_cast<std::int64_t>((std::numeric_limits<int>::min)()),
        static_cast<std::int64_t>((std::numeric_limits<int>::max)())));
    packet = scale_monster_outgoing_damage(
        packet, encounter_config_.abyss.monster_damage_bp);

    if (!apply_player_damage(packet, DamageDelivery::direct, source_position,
                             feedback)) {
        return false;
    }

    if (values.slow_bp > player_.status.slow_bp) {
        player_.status.slow_bp = values.slow_bp;
        player_.status.slow_ticks = values.slow_ticks;
    } else if (values.slow_bp == player_.status.slow_bp && values.slow_bp > 0) {
        player_.status.slow_ticks = values.slow_ticks;
    } else if (values.slow_bp > 0) {
        player_.status.slow_ticks = std::max(player_.status.slow_ticks,
                                             values.slow_ticks);
    }

    const int corrosion_damage = scale_basis_points(
        values.corrosion_damage, encounter_config_.abyss.monster_damage_bp);
    if (corrosion_damage > player_.status.corrosion_damage_per_second) {
        player_.status.corrosion_damage_per_second = corrosion_damage;
        player_.status.corrosion_ticks = values.corrosion_ticks;
        player_.status.corrosion_tick_phase = 0U;
    } else if (corrosion_damage
                   == player_.status.corrosion_damage_per_second
               && corrosion_damage > 0) {
        player_.status.corrosion_ticks = values.corrosion_ticks;
        player_.status.corrosion_tick_phase = 0U;
    } else if (corrosion_damage > 0) {
        player_.status.corrosion_ticks = std::max(player_.status.corrosion_ticks,
                                                   values.corrosion_ticks);
    }
    if (trigger_chain) {
        static_cast<void>(trigger_chain_lightning(MonsterHandle{
            static_cast<std::uint16_t>(slot), monster.generation},
            monster.affixes, source_position));
    }
    return true;
}

void CombatWorld::tick_player_status() noexcept {
    if (player_.status.slow_ticks != 0U) {
        --player_.status.slow_ticks;
        if (player_.status.slow_ticks == 0U) player_.status.slow_bp = 0;
    }

    if (player_.status.corrosion_ticks == 0U) return;
    --player_.status.corrosion_ticks;
    ++player_.status.corrosion_tick_phase;
    if (player_.status.corrosion_tick_phase == 60U) {
        player_.status.corrosion_tick_phase = 0U;
        static_cast<void>(apply_player_damage(
            DamagePacket{{0, 0, 0, 0,
                player_.status.corrosion_damage_per_second}},
            DamageDelivery::ground_or_environment, player_.position,
            FeedbackLevel::medium));
    }
    if (player_.status.corrosion_ticks == 0U) {
        player_.status.corrosion_damage_per_second = 0;
        player_.status.corrosion_tick_phase = 0U;
    }
}

bool CombatWorld::spawn_projectile(
    MonsterHandle owner,
    Vec3 position,
    Vec3 velocity,
    std::uint16_t lifetime_ticks,
    DamagePacket damage,
    float radius,
    bool trigger_chain_on_end,
    MonsterAffixSet owner_affixes) noexcept {
    if (monsters_.get(owner) == nullptr) {
        saturating_increment(projectile_invalid_owner_count_);
        return false;
    }
    if (!projectiles_.spawn(
            owner, position, velocity, lifetime_ticks, damage, radius,
            trigger_chain_on_end, owner_affixes)
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
    float radius,
    bool trigger_chain_on_end,
    MonsterAffixSet owner_affixes) noexcept {
    return spawn_projectile(owner, position, velocity, lifetime_ticks,
                            DamagePacket{damage}, radius, trigger_chain_on_end,
                            owner_affixes);
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
    HazardKind kind,
    Vec3 center,
    float radius,
    std::uint16_t telegraph_ticks,
    std::uint16_t active_ticks,
    std::uint16_t damage_interval_ticks,
    DamagePacket damage,
    bool persists_after_owner_death) noexcept {
    if (monsters_.get(owner) == nullptr) {
        saturating_increment(hazard_invalid_owner_count_);
        return false;
    }
    if (!hazards_.spawn(owner, kind, center, radius, telegraph_ticks, active_ticks,
                        damage_interval_ticks, damage,
                        persists_after_owner_death).has_value()) {
        saturating_increment(hazard_saturation_count_);
        return false;
    }
    return true;
}

bool CombatWorld::spawn_hazard(
    MonsterHandle owner,
    HazardKind kind,
    Vec3 center,
    float radius,
    std::uint16_t telegraph_ticks,
    std::uint16_t active_ticks,
    std::uint16_t damage_interval_ticks,
    int damage,
    bool persists_after_owner_death) noexcept {
    return spawn_hazard(owner, kind, center, radius, telegraph_ticks, active_ticks,
                        damage_interval_ticks, DamagePacket{damage},
                        persists_after_owner_death);
}

bool CombatWorld::trigger_chain_lightning(
    MonsterHandle owner, MonsterAffixSet affixes, Vec3 center) noexcept {
    const MonsterAffixTierValues* values = affix_values(
        affixes, MonsterAffixId::chain_lightning);
    if (values == nullptr) return false;
    DamagePacket damage{};
    damage.amount[modifiers::damage_index(modifiers::DamageType::lightning)] =
        values->damage;
    damage = scale_monster_outgoing_damage(
        damage, encounter_config_.abyss.monster_damage_bp);
    if (!spawn_hazard(owner, HazardKind::chain_lightning, center,
                      values->radius,
                      static_cast<std::uint16_t>(values->interval_ticks + 1U),
                      1U, 1U,
                      damage)) {
        return false;
    }
    CombatEvent warning{};
    warning.kind = CombatEventKind::affix_chain_warning;
    warning.tick = tick_;
    warning.position = center;
    emit_event(warning);
    return true;
}

void CombatWorld::tick_active_affixes(
    std::size_t slot, MonsterRuntime& monster) noexcept {
    const MonsterHandle owner{static_cast<std::uint16_t>(slot),
                              monster.generation};
    if (const MonsterAffixTierValues* burning = affix_values(
            monster.affixes, MonsterAffixId::burning_ground)) {
        if (monster.burning_ground_ticks < burning->interval_ticks) {
            ++monster.burning_ground_ticks;
        }
        if (monster.burning_ground_ticks >= burning->interval_ticks) {
            monster.burning_ground_ticks = 0U;
            DamagePacket damage{};
            damage.amount[modifiers::damage_index(modifiers::DamageType::fire)] =
                burning->damage;
            damage = scale_monster_outgoing_damage(
                damage, encounter_config_.abyss.monster_damage_bp);
            static_cast<void>(spawn_hazard(owner, HazardKind::burning,
                monster.position, burning->radius, 0U, burning->duration_ticks,
                30U, damage));
        }
    }

    const MonsterAffixTierValues* blink = affix_values(
        monster.affixes, MonsterAffixId::blink_assault);
    if (blink == nullptr) return;
    if (monster.affix_warning == MonsterAffixWarning::blink) {
        if (monster.affix_warning_ticks != 0U) {
            --monster.affix_warning_ticks;
        }
        if (monster.affix_warning_ticks == 0U) {
            float dx = player_.position.x - monster.position.x;
            float dy = player_.position.y - monster.position.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= 0.0001F) {
                dx = monster.facing == Facing::right ? 1.0F : -1.0F;
                dy = 0.0F;
            } else {
                dx /= distance;
                dy /= distance;
            }
            monster.position = Vec3{player_.position.x - dx,
                                    player_.position.y - dy,
                                    player_.position.z};
            monster.position.x = std::clamp(monster.position.x,
                room_bounds::min_x, room_bounds::max_x);
            monster.position.y = std::clamp(monster.position.y,
                room_bounds::min_y, room_bounds::max_y);
            monster.velocity = Vec3{};
            monster.blink_empowered = true;
            monster.affix_warning = MonsterAffixWarning::none;
        }
        return;
    }
    if (monster.blink_assault_ticks < blink->interval_ticks) {
        ++monster.blink_assault_ticks;
    }
    if (monster.blink_assault_ticks < blink->interval_ticks) return;
    monster.blink_assault_ticks = 0U;
    monster.affix_warning = MonsterAffixWarning::blink;
    monster.affix_warning_ticks = blink->duration_ticks;
    CombatEvent warning{};
    warning.kind = CombatEventKind::affix_blink_warning;
    warning.tick = tick_;
    warning.target_index = static_cast<std::uint8_t>(slot);
    warning.position = monster.position;
    emit_event(warning);
}

void CombatWorld::remove_owned_hazards(MonsterHandle owner) noexcept {
    for (std::size_t index = 0; index < kHazardCapacity; ++index) {
        const HazardRuntime& hazard = hazards_.slots()[index];
        if (!hazard.active || hazard.kind == HazardKind::death_blast
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
            apply_monster_direct_hit(
                static_cast<std::size_t>(owner.index), projectile.damage,
                projectile.position, FeedbackLevel::medium, false);
        }
        if (hit_player || outside || expired) {
            if (projectile.trigger_chain_on_end) {
                static_cast<void>(trigger_chain_lightning(owner,
                    projectile.owner_affixes, projectile.position));
            }
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
