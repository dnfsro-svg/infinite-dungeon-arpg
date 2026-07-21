#include "combat/combat_world.hpp"

#include "abyss/abyss_rules.hpp"
#include "modifiers/damage_types.hpp"

#include <algorithm>
#include <limits>

namespace arpg::combat {
namespace {

void saturating_increment_environment(std::uint32_t& counter) noexcept {
    if (counter != (std::numeric_limits<std::uint32_t>::max)()) {
        ++counter;
    }
}

HazardKind environment_kind(abyss::AbyssRuleId rule) noexcept {
    switch (rule) {
    case abyss::AbyssRuleId::thunderstorm:
        return HazardKind::thunderstorm;
    case abyss::AbyssRuleId::hunting_flames:
        return HazardKind::hunting_flame;
    case abyss::AbyssRuleId::chaos_expansion:
        return HazardKind::chaos_expansion;
    default:
        return HazardKind::native;
    }
}

}  // namespace

DamagePacket CombatWorld::environment_damage_packet(
    int actual_max_hp,
    std::uint16_t basis_points,
    modifiers::DamageType damage_type) noexcept {
    DamagePacket packet{};
    const auto amount = abyss::percent_of_actual_max_hp(
        actual_max_hp, basis_points);
    const std::size_t index = modifiers::damage_index(damage_type);
    if (amount.has_value() && index < packet.amount.size()
        && modifiers::is_elemental(damage_type)) {
        packet.amount[index] = *amount;
    }
    return packet;
}

namespace {

float configured_radius(
    const abyss::AbyssEnvironmentConfig& config,
    std::size_t index) noexcept {
    if (index >= config.radius_count || index >= config.radius_milliunits.size()) {
        return 0.0F;
    }
    return static_cast<float>(config.radius_milliunits[index]) / 1000.0F;
}

HazardRuntime* find_environment_hazard(
    HazardPool& hazards, HazardKind kind) noexcept {
    for (HazardRuntime& hazard : hazards.slots()) {
        if (hazard.active && hazard.source == HazardSource::abyss_environment
            && hazard.kind == kind) {
            return &hazard;
        }
    }
    return nullptr;
}

}  // namespace

bool CombatWorld::spawn_environment_hazard(
    HazardKind kind,
    Vec3 center,
    float radius,
    std::uint16_t telegraph_ticks,
    std::uint16_t active_ticks,
    std::uint16_t damage_interval_ticks,
    DamagePacket damage,
    std::uint16_t environment_damage_bp,
    modifiers::DamageType environment_damage_type) noexcept {
    constexpr MonsterHandle kNoMonsterOwner{};
    if (!hazards_.spawn(HazardSource::abyss_environment, kNoMonsterOwner,
                        kind, center, radius, telegraph_ticks, active_ticks,
                        damage_interval_ticks, damage, true,
                        environment_damage_bp,
                        environment_damage_type).has_value()) {
        saturating_increment_environment(hazard_saturation_count_);
        return false;
    }
    return true;
}

void CombatWorld::simulate_abyss_environment() noexcept {
    const auto& config = encounter_config_.abyss.environment;
    if (!abyss_environment_.active || !config.active
        || config.radius_count == 0U) {
        return;
    }

    const HazardKind kind = environment_kind(abyss_environment_.rule);
    if (kind == HazardKind::native) return;

    if (config.expansion_interval_ticks != 0U) {
        const std::uint64_t uncapped_stage =
            tick_ / config.expansion_interval_ticks;
        const std::uint8_t last_stage = static_cast<std::uint8_t>(
            config.radius_count - 1U);
        const std::uint8_t stage = static_cast<std::uint8_t>(
            (std::min)(uncapped_stage,
                       static_cast<std::uint64_t>(last_stage)));
        const bool reached_new_stage =
            stage != abyss_environment_.expansion_stage;
        abyss_environment_.expansion_stage = stage;
        abyss_environment_.stage_tick = static_cast<std::uint16_t>(
            tick_ % config.expansion_interval_ticks);
        abyss_environment_.locked_center = Vec3{};
        abyss_environment_.warning = false;

        HazardRuntime* hazard = find_environment_hazard(hazards_, kind);
        if (hazard != nullptr) {
            hazard->radius = configured_radius(config, stage);
            return;
        }
        if (!reached_new_stage) return;
        static_cast<void>(spawn_environment_hazard(
            kind, abyss_environment_.locked_center,
            configured_radius(config, stage), 0U,
            (std::numeric_limits<std::uint16_t>::max)(),
            config.damage_interval_ticks,
            environment_damage_packet(
                player_.max_hp, config.damage_bp, config.damage_type),
            config.damage_bp, config.damage_type));
        return;
    }

    if (config.cycle_ticks == 0U) return;
    abyss_environment_.cycle_tick = static_cast<std::uint16_t>(
        tick_ % config.cycle_ticks);
    if (tick_ == 0U || abyss_environment_.cycle_tick != 0U) {
        if (HazardRuntime* hazard = find_environment_hazard(hazards_, kind)) {
            abyss_environment_.warning = hazard->telegraph_ticks != 0U;
        } else {
            abyss_environment_.warning = false;
        }
        return;
    }

    abyss_environment_.locked_center = player_.position;
    abyss_environment_.stage_tick = 0U;
    const std::uint16_t active_ticks = config.duration_ticks == 0U
        ? 1U : config.duration_ticks;
    const std::uint16_t interval = config.damage_interval_ticks == 0U
        ? 1U : config.damage_interval_ticks;
    const std::uint16_t telegraph_ticks = config.warning_ticks
        == (std::numeric_limits<std::uint16_t>::max)()
        ? config.warning_ticks
        : static_cast<std::uint16_t>(config.warning_ticks + 1U);
    abyss_environment_.warning = spawn_environment_hazard(
        kind, abyss_environment_.locked_center,
        configured_radius(config, 0U), telegraph_ticks, active_ticks,
        interval, environment_damage_packet(
            player_.max_hp, config.damage_bp, config.damage_type),
        config.damage_bp, config.damage_type);
}

void CombatWorld::remove_environment_hazards() noexcept {
    for (std::size_t index = 0U; index < hazards_.slots().size(); ++index) {
        const HazardRuntime& hazard = hazards_.slots()[index];
        if (!hazard.active || hazard.source != HazardSource::abyss_environment) {
            continue;
        }
        static_cast<void>(hazards_.destroy(HazardHandle{
            static_cast<std::uint16_t>(index), hazard.generation}));
    }
}

void CombatWorld::remove_monster_hazards() noexcept {
    for (std::size_t index = 0U; index < hazards_.slots().size(); ++index) {
        const HazardRuntime& hazard = hazards_.slots()[index];
        if (!hazard.active || hazard.source != HazardSource::monster) {
            continue;
        }
        static_cast<void>(hazards_.destroy(HazardHandle{
            static_cast<std::uint16_t>(index), hazard.generation}));
    }
}

}  // namespace arpg::combat
