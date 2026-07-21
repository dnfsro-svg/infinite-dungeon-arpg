#pragma once

#include "abyss/abyss_types.hpp"
#include "passives/passive_tree_types.hpp"
#include "progression/progression_types.hpp"
#include "items/item_types.hpp"
#include "skills/skill_loadout.hpp"

#include <array>
#include <cstdint>
#include <limits>

namespace arpg::dungeon::checkpoint {

enum class ExitDirection : std::uint8_t {
    up = 0,
    down = 1,
    left = 2,
    right = 3,
    none = 0xFF,
};

enum class EntrySide : std::uint8_t {
    initial = 0,
    top = 1,
    bottom = 2,
    left = 3,
    right = 4,
};

enum class DungeonElement : std::uint8_t {
    fire = 0,
    water = 1,
    lightning = 2,
    chaos = 3,
};

enum class TransitionKind : std::uint8_t {
    door = 0,
    descent = 1,
    death_retreat = 2,
    none = 0xFF,
};

struct RoomDescriptor final {
    std::uint64_t index{};
    std::uint64_t seed{};
    std::uint64_t depth{1};
    std::uint64_t floor_room_index{1};
    EntrySide entry{EntrySide::initial};
    DungeonElement ecology{DungeonElement::fire};
    bool has_hole{};
    bool is_abyss{};
};

enum class DeathLifecycle : std::uint8_t {
    none = 0,
    pending_continue = 1,
};

enum class DeathSourceKind : std::uint8_t {
    monster_attack = 0,
    projectile = 1,
    ground_hazard = 2,
    monster_affix = 3,
    abyss_environment = 4,
    unknown = 5,
};

enum class DeathDamageType : std::uint8_t {
    physical = 0,
    fire = 1,
    water = 2,
    lightning = 3,
    chaos = 4,
};

inline constexpr std::uint8_t kDeathCheckpointDataVersion = 1U;

struct DeathCheckpoint final {
    DeathLifecycle lifecycle{DeathLifecycle::none};
    std::uint8_t data_version{};
    std::uint64_t death_depth{};
    std::uint64_t death_floor_room_index{};
    DungeonElement death_ecology{DungeonElement::fire};
    bool death_was_abyss{};
    DeathSourceKind source_kind{DeathSourceKind::unknown};
    std::uint8_t source_monster_id{0xFFU};
    std::uint16_t source_detail_id{};
    DeathDamageType damage_type{DeathDamageType::physical};
    std::uint64_t raw_damage{};
    std::uint64_t barrier_loss{};
    std::uint64_t health_loss{};
    std::uint64_t final_damage{};
    std::array<std::uint64_t, 5> recent_damage{};
    std::int32_t hp{};
    std::int32_t max_hp{};
    std::int32_t barrier{};
    std::int32_t max_barrier{};
    std::int64_t armor{};
    std::int64_t evasion{};
    std::int32_t armor_reduction_bp{};
    std::int32_t evasion_rate_bp{};
    std::array<std::int32_t, 4> damage_reduction{};
    std::array<std::int32_t, 4> damage_reduction_cap{};
    RoomDescriptor target_room{0U, 0U, 0U, 0U,
        EntrySide::initial, DungeonElement::fire, false, false};
};

namespace detail {

[[nodiscard]] constexpr bool valid_death_element(
    DungeonElement value) noexcept {
    return value >= DungeonElement::fire && value <= DungeonElement::chaos;
}

[[nodiscard]] constexpr bool valid_death_source_kind(
    DeathSourceKind value) noexcept {
    return value >= DeathSourceKind::monster_attack
        && value <= DeathSourceKind::unknown;
}

[[nodiscard]] constexpr bool valid_death_damage_type(
    DeathDamageType value) noexcept {
    return value >= DeathDamageType::physical
        && value <= DeathDamageType::chaos;
}

[[nodiscard]] constexpr bool zero_u64_array(
    const std::array<std::uint64_t, 5>& values) noexcept {
    for (const std::uint64_t value : values) {
        if (value != 0U) return false;
    }
    return true;
}

[[nodiscard]] constexpr bool zero_i32_array(
    const std::array<std::int32_t, 4>& values) noexcept {
    for (const std::int32_t value : values) {
        if (value != 0) return false;
    }
    return true;
}

[[nodiscard]] constexpr bool canonical_none(
    const DeathCheckpoint& death) noexcept {
    return death.data_version == 0U
        && death.death_depth == 0U
        && death.death_floor_room_index == 0U
        && death.death_ecology == DungeonElement::fire
        && !death.death_was_abyss
        && death.source_kind == DeathSourceKind::unknown
        && death.source_monster_id == 0xFFU
        && death.source_detail_id == 0U
        && death.damage_type == DeathDamageType::physical
        && death.raw_damage == 0U
        && death.barrier_loss == 0U
        && death.health_loss == 0U
        && death.final_damage == 0U
        && zero_u64_array(death.recent_damage)
        && death.hp == 0
        && death.max_hp == 0
        && death.barrier == 0
        && death.max_barrier == 0
        && death.armor == 0
        && death.evasion == 0
        && death.armor_reduction_bp == 0
        && death.evasion_rate_bp == 0
        && zero_i32_array(death.damage_reduction)
        && zero_i32_array(death.damage_reduction_cap)
        && death.target_room.index == 0U
        && death.target_room.seed == 0U
        && death.target_room.depth == 0U
        && death.target_room.floor_room_index == 0U
        && death.target_room.entry == EntrySide::initial
        && death.target_room.ecology == DungeonElement::fire
        && !death.target_room.has_hole
        && !death.target_room.is_abyss;
}

}  // namespace detail

[[nodiscard]] constexpr bool valid_death_checkpoint_structural(
    const DeathCheckpoint& death) noexcept {
    if (death.lifecycle == DeathLifecycle::none) {
        return detail::canonical_none(death);
    }
    if (death.lifecycle != DeathLifecycle::pending_continue
        || death.data_version != kDeathCheckpointDataVersion
        || death.death_depth == 0U
        || !detail::valid_death_element(death.death_ecology)
        || !detail::valid_death_source_kind(death.source_kind)
        || !detail::valid_death_damage_type(death.damage_type)) {
        return false;
    }

    switch (death.source_kind) {
    case DeathSourceKind::monster_attack:
    case DeathSourceKind::projectile:
        if (death.source_monster_id == 0xFFU
            || death.source_detail_id != 0U) return false;
        break;
    case DeathSourceKind::ground_hazard:
    case DeathSourceKind::monster_affix:
        if (death.source_monster_id == 0xFFU) return false;
        break;
    case DeathSourceKind::abyss_environment:
        if (death.source_monster_id != 0xFFU) return false;
        break;
    case DeathSourceKind::unknown:
        if (death.source_monster_id != 0xFFU
            || death.source_detail_id != 0U) return false;
        break;
    }

    if (death.raw_damage == 0U || death.health_loss == 0U
        || death.barrier_loss
            > (std::numeric_limits<std::uint64_t>::max)()
                - death.health_loss
        || death.final_damage != death.barrier_loss + death.health_loss) {
        return false;
    }
    std::uint64_t recent_total = 0U;
    for (const std::uint64_t value : death.recent_damage) {
        if (recent_total
            > (std::numeric_limits<std::uint64_t>::max)() - value) {
            return false;
        }
        recent_total += value;
    }
    if (recent_total < death.final_damage
        || death.hp != 0 || death.max_hp <= 0
        || death.barrier != 0 || death.max_barrier < 0
        || death.armor < 0 || death.evasion < 0
        || death.armor_reduction_bp < 0
        || death.armor_reduction_bp > 10000
        || death.evasion_rate_bp < 0
        || death.evasion_rate_bp > 10000) {
        return false;
    }
    if (death.barrier_loss > static_cast<std::uint64_t>(death.max_barrier)
        || death.health_loss > static_cast<std::uint64_t>(death.max_hp)) {
        return false;
    }
    for (std::size_t index = 0U;
         index < death.damage_reduction.size(); ++index) {
        if (death.damage_reduction_cap[index] < 7500
            || death.damage_reduction_cap[index] > 9500
            || death.damage_reduction[index] < -6000
            || death.damage_reduction[index]
                > death.damage_reduction_cap[index]) {
            return false;
        }
    }
    const std::uint64_t expected_target_depth = death.death_depth > 1U
        ? death.death_depth - 1U : 1U;
    return death.target_room.index != 0U
        && death.target_room.seed != 0U
        && death.target_room.depth == expected_target_depth
        && death.target_room.floor_room_index == 0U
        && death.target_room.entry == EntrySide::initial
        && detail::valid_death_element(death.target_room.ecology)
        && !death.target_room.is_abyss;
}

struct AbyssCheckpoint final {
    abyss::AbyssLifecycle lifecycle{abyss::AbyssLifecycle::none};
    abyss::AbyssDanger danger{abyss::AbyssDanger::low};
    abyss::AbyssRuleId rule{abyss::AbyssRuleId::none};
    std::uint32_t rules_version{};
    std::uint8_t reward_total{};
    std::uint8_t generated_mask{};
    std::uint8_t claimed_mask{};
    std::uint8_t abandoned_mask{};
    std::uint32_t reward_revision{};
};

struct LastAbyssResolution final {
    bool valid{};
    std::uint64_t room_seed{};
    abyss::AbyssRuleId rule{abyss::AbyssRuleId::none};
    std::uint8_t total{};
    std::uint8_t generated{};
    std::uint8_t claimed{};
    std::uint8_t abandoned{};
};

struct DungeonRunState final {
    std::uint64_t root_seed{};
    std::uint64_t commit_generation{1};
    std::array<std::uint32_t, 4> biases{};
    RoomDescriptor current_room{};
    AbyssCheckpoint abyss{};
    LastAbyssResolution last_abyss_resolution{};
    TransitionKind last_transition{TransitionKind::none};
    ExitDirection last_direction{ExitDirection::none};
    progression::ProgressionState progression{};
    passives::PassiveTreeState passive_tree{};
    items::ItemOwnershipState item_ownership{};
    std::uint64_t death_sequence{};
    DeathCheckpoint death{};
    skills::SkillLoadoutState skill_loadout{
        skills::default_skill_loadout()};
};

[[nodiscard]] constexpr bool valid_abyss_door_origin(
    const DungeonRunState& state,
    bool rolled) noexcept {
    if (!rolled || !state.current_room.is_abyss
            || state.last_transition != TransitionKind::door) {
        return false;
    }
    switch (state.last_direction) {
    case ExitDirection::up:
        return state.current_room.entry == EntrySide::bottom;
    case ExitDirection::down:
        return state.current_room.entry == EntrySide::top;
    case ExitDirection::left:
        return state.current_room.entry == EntrySide::right;
    case ExitDirection::right:
        return state.current_room.entry == EntrySide::left;
    case ExitDirection::none:
        return false;
    }
    return false;
}

}  // namespace arpg::dungeon::checkpoint
