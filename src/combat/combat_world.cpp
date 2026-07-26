#include "combat/combat_world.hpp"

#include "abyss/abyss_rules.hpp"
#include "combat/attack_catalog.hpp"
#include "combat/active_skill_runtime.hpp"
#include "combat/combat_scaling.hpp"
#include "combat/fire_room_obstacle.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_affix_generation.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/room_bounds.hpp"
#include "skills/active_skill_catalog.hpp"

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
    std::int64_t left, std::int64_t right, std::int64_t& result) noexcept;

std::int64_t saturating_add(
    std::int64_t left, std::int64_t right) noexcept {
    std::int64_t result{};
    if (checked_add(left, right, result)) return result;
    return right < 0 ? (std::numeric_limits<std::int64_t>::min)()
                     : (std::numeric_limits<std::int64_t>::max)();
}

std::int64_t saturating_multiply(
    std::int64_t left, std::int64_t right) noexcept {
    std::int64_t result{};
    if (checked_multiply(left, right, result)) return result;
    return (left < 0) != (right < 0)
        ? (std::numeric_limits<std::int64_t>::min)()
        : (std::numeric_limits<std::int64_t>::max)();
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

std::uint64_t multiply_divide_floor_u64(
    std::uint64_t left,
    std::uint64_t right,
    std::uint64_t divisor) noexcept {
    std::uint64_t quotient{};
    std::uint64_t remainder{};
    std::uint64_t add_quotient = right / divisor;
    std::uint64_t add_remainder = right % divisor;
    while (left != 0U) {
        if ((left & 1U) != 0U) {
            quotient += add_quotient;
            remainder += add_remainder;
            if (remainder >= divisor) {
                remainder -= divisor;
                ++quotient;
            }
        }
        left >>= 1U;
        if (left == 0U) break;
        add_quotient *= 2U;
        add_remainder *= 2U;
        if (add_remainder >= divisor) {
            add_remainder -= divisor;
            ++add_quotient;
        }
    }
    return quotient;
}

std::int64_t fixed_scale_floor(
    std::int64_t value, std::int64_t factor) noexcept {
    return fixed_floor(saturating_multiply(value, factor));
}

int saturating_damage_component(std::int64_t value) noexcept {
    return static_cast<int>(std::clamp(value,
        static_cast<std::int64_t>((std::numeric_limits<int>::min)()),
        static_cast<std::int64_t>((std::numeric_limits<int>::max)())));
}

bool validate_player_build_fields(const PlayerCombatBuild& build) noexcept {
    const auto& values = build.values;
    if (!values.valid || build.weapon_physical < 0
        || build.local_attack_speed_bp < -modifiers::kFixedOne
        || values.armor < 0
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
    physical = saturating_add(base_physical, build.weapon_physical);
    physical = saturating_add(physical, fixed_floor(values.flat_damage[
        modifiers::damage_index(modifiers::DamageType::physical)]));
    std::int64_t scaled = fixed_scale_floor(
        physical, values.damage_increased[
                      modifiers::damage_index(modifiers::DamageType::physical)]);
    scaled = fixed_scale_floor(scaled, values.melee_damage);
    packet.amount[modifiers::damage_index(modifiers::DamageType::physical)] =
        saturating_damage_component(scaled);
    for (std::size_t index = 1; index < modifiers::kDamageTypeCount; ++index) {
        scaled = fixed_scale_floor(
            fixed_floor(values.flat_damage[index]),
            values.damage_increased[index]);
        scaled = fixed_scale_floor(scaled, values.melee_damage);
        packet.amount[index] = saturating_damage_component(scaled);
    }
    return packet;
}

}  // namespace

std::optional<DamagePacket> build_player_hit_packet(
    int base_physical, const PlayerCombatBuild& build) noexcept {
    if (!valid_player_build(build)) return std::nullopt;
    return build_player_hit_packet_checked_unvalidated(base_physical, build);
}

std::optional<ResolvedPlayerDamage> resolve_player_damage_packet(
    DamagePacket packet, const PlayerCombatBuild& build) noexcept {
    if (!valid_player_build(build)) return std::nullopt;

    const auto saturating_add = [](
        std::uint64_t left, std::uint64_t right) noexcept {
        const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
        return left > maximum - right ? maximum : left + right;
    };
    const auto reduced_component = [](
        int raw, std::int32_t reduction) noexcept -> std::optional<std::uint64_t> {
        if (raw <= 0) return std::uint64_t{0};
        const std::int64_t multiplier = modifiers::kFixedOne - reduction;
        std::int64_t reduced_product{};
        if (!checked_multiply(raw, multiplier, reduced_product)) {
            return std::nullopt;
        }
        const std::int64_t reduced = reduced_product / modifiers::kFixedOne
            + (reduced_product % modifiers::kFixedOne != 0 ? 1 : 0);
        return static_cast<std::uint64_t>(reduced);
    };

    std::array<std::uint64_t, modifiers::kDamageTypeCount> reduced{};
    ResolvedPlayerDamage result{};
    const auto physical = reduced_component(
        packet.amount[modifiers::damage_index(modifiers::DamageType::physical)],
        modifiers::rating_to_basis_points(build.values.armor));
    if (!physical.has_value()) return std::nullopt;
    reduced[modifiers::damage_index(modifiers::DamageType::physical)] =
        *physical;
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
        if (!component.has_value()) return std::nullopt;
        reduced[element + 1U] = *component;
    }

    std::uint64_t reduced_total{};
    for (const std::uint64_t component : reduced) {
        reduced_total = saturating_add(reduced_total, component);
    }
    if (reduced_total == 0U) return result;
    if (reduced_total > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)())) {
        return std::nullopt;
    }
    std::int64_t scaled_total{};
    if (!checked_multiply(
            static_cast<std::int64_t>(reduced_total),
            build.values.damage_taken,
            scaled_total)) {
        return std::nullopt;
    }
    const std::uint64_t compatible_total = static_cast<std::uint64_t>(
        scaled_total / modifiers::kFixedOne);

    std::uint64_t distributed_total{};
    for (std::size_t index = 0; index < reduced.size(); ++index) {
        result.by_type[index] = multiply_divide_floor_u64(
            reduced[index], compatible_total, reduced_total);
        distributed_total = saturating_add(
            distributed_total, result.by_type[index]);
    }
    std::uint64_t remainder = compatible_total - distributed_total;
    for (std::size_t index = 0;
         index < reduced.size() && remainder != 0U;
         ++index) {
        if (reduced[index] == 0U) continue;
        ++result.by_type[index];
        --remainder;
    }
    if (remainder != 0U) return std::nullopt;
    for (const std::uint64_t component : result.by_type) {
        result.total = saturating_add(result.total, component);
    }
    return result;
}

std::optional<int> resolve_player_damage(
    DamagePacket packet, const PlayerCombatBuild& build) noexcept {
    const auto resolved = resolve_player_damage_packet(packet, build);
    if (!resolved.has_value()
        || resolved->total > static_cast<std::uint64_t>(
            (std::numeric_limits<int>::max)())) {
        return std::nullopt;
    }
    return static_cast<int>(resolved->total);
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

bool valid_monster_source(MonsterId monster) noexcept {
    return static_cast<std::uint8_t>(monster)
        < static_cast<std::uint8_t>(MonsterId::count);
}

bool valid_abyss_source_detail(std::uint16_t detail_id) noexcept {
    return detail_id <= static_cast<std::uint16_t>(
        abyss::AbyssRuleId::life_sacrifice);
}

bool valid_hazard_source_detail(std::uint16_t detail_id) noexcept {
    return detail_id <= static_cast<std::uint16_t>(
        HazardKind::chaos_expansion);
}

bool valid_affix_source_detail(std::uint16_t detail_id) noexcept {
    return detail_id < static_cast<std::uint16_t>(MonsterAffixId::count);
}

PlayerDamageSource canonical_player_damage_source(
    PlayerDamageSource source) noexcept {
    switch (source.kind) {
    case PlayerDamageSourceKind::monster_attack:
    case PlayerDamageSourceKind::projectile:
        if (!valid_monster_source(source.monster)) return {};
        source.detail_id = 0U;
        return source;
    case PlayerDamageSourceKind::ground_hazard:
        return valid_monster_source(source.monster)
            && valid_hazard_source_detail(source.detail_id)
            ? source : PlayerDamageSource{};
    case PlayerDamageSourceKind::monster_affix:
        return valid_monster_source(source.monster)
            && valid_affix_source_detail(source.detail_id)
            ? source : PlayerDamageSource{};
    case PlayerDamageSourceKind::abyss_environment:
        if (!valid_abyss_source_detail(source.detail_id)) return {};
        source.monster = MonsterId::count;
        return source;
    case PlayerDamageSourceKind::unknown:
    default:
        return {};
    }
}

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

SkillCastResult CombatWorld::request_active_skill(
    skills::ActiveSkillId skill) noexcept {
    if (skill == skills::ActiveSkillId::none) return SkillCastResult::none;
    const std::size_t index = static_cast<std::size_t>(skill);
    if (index >= skills::kActiveSkillCount) return SkillCastResult::invalid_skill;
    if (player_.hp <= 0 || player_.hurt_ticks != 0U
        || player_.hit_stop_ticks != 0U || death_snapshot_.has_value()) {
        return SkillCastResult::player_unavailable;
    }
    if (attack_.id != AttackId::none) return SkillCastResult::basic_attack_active;
    if (active_skill_.cooldowns[index] != 0U) {
        return SkillCastResult::cooling_down;
    }
    if (active_skill_.snapshot.id != skills::ActiveSkillId::none) {
        return SkillCastResult::skill_active;
    }
    if (skill != skills::ActiveSkillId::draw_slash
        && skill != skills::ActiveSkillId::storm_swords) {
        return SkillCastResult::invalid_skill;
    }

    Vec3 locked_center = player_.position;
    if (skill == skills::ActiveSkillId::storm_swords) {
        const float facing = player_.facing == Facing::right ? 1.0F : -1.0F;
        locked_center.x += facing * kStormCenterForward;
    }
    active_skill_.snapshot = ActiveSkillSnapshot{
        skill, ActiveSkillPhase::startup, 0U, locked_center, 0U};
    active_skill_.snapshot.transients_active = true;
    active_skill_.locked_facing = player_.facing;
    active_skill_.cooldowns[index] = skill == skills::ActiveSkillId::draw_slash
        ? skills::kDrawSlashCooldownTicks : skills::kStormSwordsCooldownTicks;
    active_skill_.hit_latch.fill(false);
    apply_active_skill_events_at(0U);
    player_.velocity.x = 0.0F;
    player_.velocity.y = 0.0F;
    player_.state = PlayerState::attack_startup;
    return SkillCastResult::accepted;
}

void CombatWorld::tick(MovementInput movement) noexcept {
    if (death_snapshot_.has_value()) {
        ++tick_;
        return;
    }
    player_damage_history_.begin_tick(tick_);
    tick_active_skill_cooldowns();
    if (player_.hp == 0) {
        attack_ = AttackRuntime{};
        active_skill_ = ActiveSkillRuntime{};
        input_buffer_.clear();
        ++tick_;
        return;
    }
    tick_player_status();
    if (death_snapshot_.has_value()) {
        ++tick_;
        return;
    }
    const bool player_frozen = player_.hit_stop_ticks != 0;
    const bool player_hurt = player_.hurt_ticks != 0;
    if (player_frozen) {
        --player_.hit_stop_ticks;
    }
    if (player_hurt) {
        --player_.hurt_ticks;
    }
    tick_active_skill();
    if (!player_frozen && !player_hurt) {
        if (active_skill_.snapshot.id == skills::ActiveSkillId::none) {
            simulate_player(movement);
        } else {
            simulate_active_skill_movement(movement);
        }
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
        const Vec3 previous_position = monster.position;
        tick_monster_affix_resources(monster);
        tick_active_affixes(index, monster);
        if (monster.affix_warning == MonsterAffixWarning::blink) {
            resolve_fire_brazier_overlap(monster, previous_position);
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
            if (death_snapshot_.has_value()) {
                ++tick_;
                return;
            }
        }
        resolve_fire_brazier_overlap(monster, previous_position);
    }

    if (!player_frozen && !player_hurt && player_.hurt_ticks == 0) {
        resolve_attack_hits();
    }

    simulate_abyss_environment();
    if (death_snapshot_.has_value()) {
        ++tick_;
        return;
    }
    simulate_projectiles();
    if (death_snapshot_.has_value()) {
        ++tick_;
        return;
    }
    simulate_hazards();
    if (death_snapshot_.has_value()) {
        ++tick_;
        return;
    }

    input_buffer_.age(player_frozen || player_hurt);
    ++tick_;
}

void CombatWorld::resolve_fire_brazier_overlap(
    MonsterRuntime& monster, Vec3 previous_position) noexcept {
    if (encounter_config_.fire_room_obstacles) {
        monster.position = fire_room_obstacle::route_monster(
            previous_position, monster.position, player_.position);
    }
}

void CombatWorld::move_player_to(Vec3 candidate) noexcept {
    if (!encounter_config_.fire_room_obstacles
            || !fire_room_obstacle::blocks_player(
                player_.position, candidate)) {
        player_.position = candidate;
    }
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

int CombatWorld::restore_player_health_percent(
    std::uint16_t maximum_health_basis_points) noexcept {
    if (maximum_health_basis_points == 0U || player_.max_hp <= 0
            || player_defeated()) {
        return 0;
    }
    const int requested = scale_basis_points(player_.max_hp,
        maximum_health_basis_points, BasisPointRounding::ceil);
    const int missing = (std::max)(0, player_.max_hp - player_.hp);
    const int actual = (std::min)(requested, missing);
    player_.hp += actual;
    return actual;
}

void CombatWorld::clear_abyss_rule_preserving_resources() noexcept {
    DerivedPlayerBuild derived{};
    if (!derive_player_build(encounter_config_.player_build, derived)) return;

    const int old_max_hp = player_.max_hp;
    const int old_hp = player_.hp;
    const bool alive = old_hp > 0;
    remove_environment_hazards();
    abyss_environment_ = AbyssEnvironmentRuntime{};
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
    player_damage_history_ = PlayerDamageHistory{};
    death_snapshot_.reset();
    initialize_player();
    monsters_.clear();
    for (auto& owner : effect_owners_) owner = {};
    projectiles_.clear();
    hazards_.clear();
    abyss_environment_ = AbyssEnvironmentRuntime{};
    fire_crates_ = {{
        {{-3.20F, 0.0F, 0.0F}, 0U, true},
        {{3.20F, 0.0F, 0.0F}, 0U, true},
    }};
    if (legacy_mode_) {
        initialize_legacy_monsters();
    } else {
        static_cast<void>(load_wave(
            encounter_config_.wave, encounter_config_.reset_player_health));
    }

    attack_ = AttackRuntime{};
    active_skill_ = ActiveSkillRuntime{};
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
    abyss_environment_.rule = encounter_config_.abyss.rule;
    abyss_environment_.active = encounter_config_.abyss.environment.active;
    abyss_environment_.expansion_stage = 0xFFU;
    simulate_abyss_environment();
}

void CombatWorld::initialize_player() noexcept {
    player_ = PlayerRuntime{};
    player_.position = encounter_config_.player_spawn;
    if (encounter_config_.fire_room_obstacles) {
        player_.position = fire_room_obstacle::eject(player_.position);
    }
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
    remove_monster_hazards();
    for (std::size_t index = 0; index < wave.spawn_count; ++index) {
        const auto handle = monsters_.spawn(
            wave.spawns[index], encounter_config_.abyss);
        if (!handle.has_value()) {
            monsters_.clear();
            return false;
        }
    }
    attack_ = AttackRuntime{};
    active_skill_ = ActiveSkillRuntime{};
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
        player_damage_history_ = PlayerDamageHistory{};
        player_damage_history_.begin_tick(tick_);
        death_snapshot_.reset();

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

Vec3 CombatWorld::player_position() const noexcept {
    return player_.position;
}

std::size_t CombatWorld::living_monster_count() const noexcept {
    std::size_t count = 0U;
    for (const MonsterRuntime& monster : monsters_.slots()) {
        if (monster.active && monster.hp > 0) ++count;
    }
    return count;
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
    if (handle.index < active_skill_.hit_latch.size()) {
        active_skill_.hit_latch[handle.index] = false;
    }
    return true;
}

std::optional<CombatEvent> CombatWorld::try_pop_event() noexcept {
    return events_.try_pop();
}

bool CombatWorld::player_defeated() const noexcept {
    return death_snapshot_.has_value() || player_.hp == 0;
}

const std::optional<CombatDeathSnapshot>&
CombatWorld::death_snapshot() const noexcept {
    return death_snapshot_;
}

bool CombatWorld::apply_player_damage(
    DamagePacket packet,
    DamageDelivery delivery,
    PlayerDamageSource source,
    Vec3 source_position,
    FeedbackLevel feedback) noexcept {
    if (death_snapshot_.has_value()) return false;
    source = canonical_player_damage_source(source);
    player_damage_history_.begin_tick(tick_);
    const bool has_positive_component = std::any_of(
        packet.amount.begin(), packet.amount.end(), [](int value) noexcept {
            return value > 0;
        });
    const auto resolved = resolve_player_damage_packet(
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

    if (resolved->total == 0U || player_.hp == 0
            || player_.invulnerability_ticks != 0
            || active_skill_.snapshot.player_invulnerable) {
        return false;
    }

    const std::uint64_t barrier_available = static_cast<std::uint64_t>(
        std::max(0, player_.barrier));
    const std::uint64_t health_available = static_cast<std::uint64_t>(
        std::max(0, player_.hp));
    const std::uint64_t resources = barrier_available + health_available;
    const std::uint64_t actual_total = std::min(resolved->total, resources);
    if (actual_total == 0U) return false;

    ResolvedPlayerDamage actual{};
    for (std::size_t index = 0U; index < actual.by_type.size(); ++index) {
        actual.by_type[index] = multiply_divide_floor_u64(
            resolved->by_type[index], actual_total, resolved->total);
        actual.total += actual.by_type[index];
    }
    std::uint64_t remainder = actual_total - actual.total;
    for (std::size_t index = 0U;
         index < actual.by_type.size() && remainder != 0U; ++index) {
        if (actual.by_type[index] >= resolved->by_type[index]) continue;
        ++actual.by_type[index];
        ++actual.total;
        --remainder;
    }
    if (remainder != 0U) return false;

    const std::uint64_t barrier_loss = std::min(
        barrier_available, actual_total);
    const std::uint64_t health_loss = actual_total - barrier_loss;
    player_.barrier -= static_cast<int>(barrier_loss);
    player_.hp -= static_cast<int>(health_loss);
    player_damage_history_.record(actual);
    if (player_.hp == 0) {
        attack_ = AttackRuntime{};
        active_skill_ = ActiveSkillRuntime{};
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
    hit.value = static_cast<int>(std::min<std::uint64_t>(
        actual_total, static_cast<std::uint64_t>(
            (std::numeric_limits<int>::max)())));
    emit_event(hit);

    CombatEvent hurt_started{};
    hurt_started.kind = CombatEventKind::player_hurt_started;
    hurt_started.tick = tick_;
    hurt_started.hit_count = 1;
    hurt_started.feedback = feedback;
    hurt_started.position = source_position;
    hurt_started.value = hit.value;
    emit_event(hurt_started);

    if (player_.hp == 0) {
        CombatDeathSnapshot snapshot{};
        snapshot.tick = tick_;
        snapshot.source = source;
        snapshot.raw_damage = 0U;
        for (const int component : packet.amount) {
            if (component <= 0) continue;
            const auto value = static_cast<std::uint64_t>(component);
            const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
            snapshot.raw_damage = snapshot.raw_damage > maximum - value
                ? maximum : snapshot.raw_damage + value;
        }
        snapshot.barrier_loss = barrier_loss;
        snapshot.health_loss = health_loss;
        snapshot.final_damage = actual_total;
        snapshot.recent_damage = player_damage_history_.totals();
        std::size_t primary_index = 0U;
        for (std::size_t index = 1U; index < actual.by_type.size(); ++index) {
            if (actual.by_type[index] > actual.by_type[primary_index]) {
                primary_index = index;
            }
        }
        snapshot.primary_type = static_cast<modifiers::DamageType>(primary_index);
        snapshot.defense.hp = player_.hp;
        snapshot.defense.max_hp = player_.max_hp;
        snapshot.defense.barrier = player_.barrier;
        snapshot.defense.max_barrier = player_.max_barrier;
        snapshot.defense.armor = player_.armor;
        snapshot.defense.evasion = player_.evasion;
        snapshot.defense.armor_reduction_bp = player_.armor_reduction_bp;
        snapshot.defense.evasion_rate_bp = player_.evasion_rate_bp;
        snapshot.defense.damage_reduction = player_.damage_reduction;
        snapshot.defense.damage_reduction_cap = player_.damage_reduction_cap;
        death_snapshot_ = snapshot;

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
                               PlayerDamageSource{}, source_position, feedback);
}

bool CombatWorld::apply_monster_direct_hit(
    std::size_t slot,
    DamagePacket packet,
    Vec3 source_position,
    FeedbackLevel feedback,
    bool trigger_chain,
    PlayerDamageSourceKind source_kind) noexcept {
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

    if (!apply_player_damage(packet, DamageDelivery::direct,
                             PlayerDamageSource{source_kind, monster.id, 0U},
                             source_position, feedback)) {
        return false;
    }
    if (death_snapshot_.has_value()) return true;

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
        player_.status.corrosion_source = PlayerDamageSource{
            PlayerDamageSourceKind::monster_affix, monster.id,
            static_cast<std::uint16_t>(MonsterAffixId::chaos_corrosion)};
    } else if (corrosion_damage
                   == player_.status.corrosion_damage_per_second
               && corrosion_damage > 0) {
        player_.status.corrosion_ticks = values.corrosion_ticks;
        player_.status.corrosion_tick_phase = 0U;
        player_.status.corrosion_source = PlayerDamageSource{
            PlayerDamageSourceKind::monster_affix, monster.id,
            static_cast<std::uint16_t>(MonsterAffixId::chaos_corrosion)};
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
            DamageDelivery::ground_or_environment,
            player_.status.corrosion_source, player_.position,
            FeedbackLevel::medium));
        if (death_snapshot_.has_value()) return;
    }
    if (player_.status.corrosion_ticks == 0U) {
        player_.status.corrosion_damage_per_second = 0;
        player_.status.corrosion_tick_phase = 0U;
        player_.status.corrosion_source = PlayerDamageSource{};
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
        if (!hazard.active || hazard.source != HazardSource::monster
            || hazard.kind == HazardKind::death_blast
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
                projectile.position, FeedbackLevel::medium, false,
                PlayerDamageSourceKind::projectile);
            if (death_snapshot_.has_value()) return;
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
        const MonsterRuntime* owner = hazard.source == HazardSource::monster
            ? monsters_.get(hazard.owner) : nullptr;
        if (hazard.source == HazardSource::monster
            && (!hazard.persists_after_owner_death &&
             (owner == nullptr || owner->hp <= 0
              || owner->reaction == ReactionState::defeated))) {
            static_cast<void>(hazards_.destroy(HazardHandle{
                static_cast<std::uint16_t>(index), hazard.generation}));
            continue;
        }
        const bool persistent_environment =
            hazard.source == HazardSource::abyss_environment
            && hazard.kind == HazardKind::chaos_expansion;
        if (!persistent_environment && hazard.lifetime_ticks != 0U) {
            --hazard.lifetime_ticks;
        }
        if (hazard.telegraph_ticks != 0U) {
            --hazard.telegraph_ticks;
            if (hazard.telegraph_ticks != 0U
                || hazard.source == HazardSource::monster) {
                continue;
            }
        }
        {
            if (hazard.damage_cooldown_ticks != 0U) {
                --hazard.damage_cooldown_ticks;
                if (hazard.damage_cooldown_ticks == 0U) {
                    hazard.player_latched = false;
                }
            }
            const float dx = hazard.center.x - player_.position.x;
            const float dy = hazard.center.y - player_.position.y;
            const float dz = hazard.center.z - player_.position.z;
            const bool intersects = hazard.source
                    == HazardSource::abyss_environment
                ? player_.position.z == 0.0F
                    && dx * dx + dy * dy <= hazard.radius * hazard.radius
                : std::fabs(dx) <= hazard.radius + kPlayerRadiusX
                    && std::fabs(dy) <= hazard.radius + kPlayerRadiusY
                    && std::fabs(dz) <= hazard.radius + kPlayerRadiusZ;
            if (intersects && !hazard.player_latched) {
                if (hazard.source == HazardSource::abyss_environment) {
                    hazard.damage = environment_damage_packet(
                        player_.max_hp, hazard.environment_damage_bp,
                        hazard.environment_damage_type);
                }
                PlayerDamageSource damage_source{};
                if (hazard.source == HazardSource::abyss_environment) {
                    damage_source.kind =
                        PlayerDamageSourceKind::abyss_environment;
                    damage_source.detail_id = static_cast<std::uint16_t>(
                        abyss_environment_.rule);
                } else if (owner != nullptr) {
                    damage_source.monster = owner->id;
                    switch (hazard.kind) {
                    case HazardKind::burning:
                        damage_source.kind =
                            PlayerDamageSourceKind::monster_affix;
                        damage_source.detail_id = static_cast<std::uint16_t>(
                            MonsterAffixId::burning_ground);
                        break;
                    case HazardKind::chain_lightning:
                        damage_source.kind =
                            PlayerDamageSourceKind::monster_affix;
                        damage_source.detail_id = static_cast<std::uint16_t>(
                            MonsterAffixId::chain_lightning);
                        break;
                    case HazardKind::death_blast:
                        damage_source.kind =
                            PlayerDamageSourceKind::monster_affix;
                        damage_source.detail_id = static_cast<std::uint16_t>(
                            MonsterAffixId::death_blast);
                        break;
                    default:
                        damage_source.kind =
                            PlayerDamageSourceKind::ground_hazard;
                        damage_source.detail_id = static_cast<std::uint16_t>(
                            hazard.kind);
                        break;
                    }
                }
                apply_player_damage(hazard.damage,
                    DamageDelivery::ground_or_environment, damage_source,
                    hazard.center, FeedbackLevel::heavy);
                if (death_snapshot_.has_value()) return;
                hazard.player_latched = true;
                hazard.damage_cooldown_ticks = hazard.damage_interval_ticks;
            }
            if (!persistent_environment && hazard.active_ticks != 0U) {
                --hazard.active_ticks;
            }
        }
        if (!persistent_environment
            && (hazard.lifetime_ticks == 0U || hazard.active_ticks == 0U)) {
            static_cast<void>(hazards_.destroy(HazardHandle{
                static_cast<std::uint16_t>(index), hazard.generation}));
        }
    }
}

}  // namespace arpg::combat
