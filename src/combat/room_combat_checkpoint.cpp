#include "combat/room_combat_checkpoint.hpp"

#include "checkpoint/room_checkpoint_validation.hpp"
#include "combat/attack_catalog.hpp"
#include "combat/combat_world.hpp"
#include "combat/fire_room_obstacle.hpp"
#include "combat/monster_persistent_state.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/room_bounds.hpp"
#include "skills/active_skill_catalog.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <type_traits>

namespace arpg::combat {

static_assert(limits::kRoomMonsterCapacity == 1152U);
static_assert(kRoomEnvironmentCellCount == 400U);
static_assert(kRoomEnvironmentCellCount
    == checkpoint::kRoomEnvironmentCellCount);
static_assert(kRoomObstacleCapacity == checkpoint::kRoomEnvironmentCellCount);
static_assert(kFireRoomCrateCapacity == checkpoint::kFireRoomCrateCapacity);
static_assert(kPlayerDamageHistoryTicks
    == checkpoint::kPlayerDamageHistoryTicks);
static_assert(kMonsterOrdinalWordCount
    == checkpoint::kMonsterOrdinalWordCount);
static_assert(std::tuple_size<decltype(MonsterAffixSet{}.values)>::value == 3U);
static_assert(std::tuple_size<decltype(MonsterAffixSet{}.values)>::value
    == std::tuple_size<decltype(
        checkpoint::MonsterAffixSet{}.values)>::value);
static_assert(kAttackCount == checkpoint::kAttackActiveTicks.size());
static_assert(modifiers::kDamageTypeCount == checkpoint::kDamageTypeCount);
static_assert(modifiers::kElementCount == checkpoint::kElementCount);
static_assert(std::tuple_size<decltype(
    checkpoint::RoomCombatCheckpoint{}.monsters)>::value
    == limits::kRoomMonsterCapacity);
static_assert(std::tuple_size<decltype(
    checkpoint::RoomCombatCheckpoint{}.obstacles)>::value
    == kRoomObstacleCapacity);
static_assert(std::tuple_size<decltype(
    checkpoint::PlayerCombatCheckpoint{}.skill_cooldowns)>::value
    == skills::kActiveSkillCount);
static_assert(std::tuple_size<decltype(
    checkpoint::MonsterCombatCheckpoint{}.effects.effects)>::value
    == modifiers::kEffectCapacity);
static_assert(std::tuple_size<decltype(
    checkpoint::MonsterCombatCheckpoint{}.effects.commands)>::value
    == modifiers::kEffectCommandCapacity);

#define ARPG_CHECKPOINT_ENUM_MATCH(runtime_type, checkpoint_type, value) \
    static_assert(static_cast<std::int64_t>(runtime_type::value) \
        == static_cast<std::int64_t>(checkpoint_type::value))

ARPG_CHECKPOINT_ENUM_MATCH(AttackId, checkpoint::AttackId, j1);
ARPG_CHECKPOINT_ENUM_MATCH(AttackId, checkpoint::AttackId, j2);
ARPG_CHECKPOINT_ENUM_MATCH(AttackId, checkpoint::AttackId, j3);
ARPG_CHECKPOINT_ENUM_MATCH(AttackId, checkpoint::AttackId, launcher);
ARPG_CHECKPOINT_ENUM_MATCH(AttackId, checkpoint::AttackId, air_j);
ARPG_CHECKPOINT_ENUM_MATCH(AttackId, checkpoint::AttackId, none);
ARPG_CHECKPOINT_ENUM_MATCH(Facing, checkpoint::Facing, left);
ARPG_CHECKPOINT_ENUM_MATCH(Facing, checkpoint::Facing, right);
ARPG_CHECKPOINT_ENUM_MATCH(PlayerState, checkpoint::PlayerState, idle);
ARPG_CHECKPOINT_ENUM_MATCH(PlayerState, checkpoint::PlayerState, move);
ARPG_CHECKPOINT_ENUM_MATCH(
    PlayerState, checkpoint::PlayerState, attack_startup);
ARPG_CHECKPOINT_ENUM_MATCH(
    PlayerState, checkpoint::PlayerState, attack_active);
ARPG_CHECKPOINT_ENUM_MATCH(
    PlayerState, checkpoint::PlayerState, attack_recovery);
ARPG_CHECKPOINT_ENUM_MATCH(PlayerState, checkpoint::PlayerState, jump_rise);
ARPG_CHECKPOINT_ENUM_MATCH(PlayerState, checkpoint::PlayerState, jump_fall);
ARPG_CHECKPOINT_ENUM_MATCH(PlayerState, checkpoint::PlayerState, landing);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterId, checkpoint::MonsterId, fire_bomber);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterId, checkpoint::MonsterId, fire_charger);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterId, checkpoint::MonsterId, water_bulwark);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterId, checkpoint::MonsterId, water_support);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterId, checkpoint::MonsterId, lightning_shooter);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterId, checkpoint::MonsterId, lightning_dasher);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterId, checkpoint::MonsterId, chaos_chaser);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterId, checkpoint::MonsterId, chaos_hazard);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterId, checkpoint::MonsterId, count);
ARPG_CHECKPOINT_ENUM_MATCH(PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, monster_attack);
ARPG_CHECKPOINT_ENUM_MATCH(PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, projectile);
ARPG_CHECKPOINT_ENUM_MATCH(PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, ground_hazard);
ARPG_CHECKPOINT_ENUM_MATCH(PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, monster_affix);
ARPG_CHECKPOINT_ENUM_MATCH(PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, abyss_environment);
ARPG_CHECKPOINT_ENUM_MATCH(PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, unknown);
ARPG_CHECKPOINT_ENUM_MATCH(
    modifiers::DamageType, checkpoint::DamageType, physical);
ARPG_CHECKPOINT_ENUM_MATCH(
    modifiers::DamageType, checkpoint::DamageType, fire);
ARPG_CHECKPOINT_ENUM_MATCH(
    modifiers::DamageType, checkpoint::DamageType, water);
ARPG_CHECKPOINT_ENUM_MATCH(
    modifiers::DamageType, checkpoint::DamageType, lightning);
ARPG_CHECKPOINT_ENUM_MATCH(
    modifiers::DamageType, checkpoint::DamageType, chaos);
ARPG_CHECKPOINT_ENUM_MATCH(
    modifiers::DamageType, checkpoint::DamageType, count);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, mighty);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, frenzy);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, swift);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, armored);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, shielding);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, multishot);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, burning_ground);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, chilling);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, chain_lightning);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, chaos_corrosion);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, blink_assault);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, death_blast);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixId, checkpoint::MonsterAffixId, count);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterAffixTier, checkpoint::MonsterAffixTier, m1);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterAffixTier, checkpoint::MonsterAffixTier, m2);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterAffixTier, checkpoint::MonsterAffixTier, m3);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixTier, checkpoint::MonsterAffixTier, count);
ARPG_CHECKPOINT_ENUM_MATCH(DummyKind, checkpoint::DummyKind, light);
ARPG_CHECKPOINT_ENUM_MATCH(DummyKind, checkpoint::DummyKind, normal);
ARPG_CHECKPOINT_ENUM_MATCH(DummyKind, checkpoint::DummyKind, heavy);
ARPG_CHECKPOINT_ENUM_MATCH(ReactionState, checkpoint::ReactionState, idle);
ARPG_CHECKPOINT_ENUM_MATCH(ReactionState, checkpoint::ReactionState, hitstun);
ARPG_CHECKPOINT_ENUM_MATCH(ReactionState, checkpoint::ReactionState, airborne);
ARPG_CHECKPOINT_ENUM_MATCH(ReactionState, checkpoint::ReactionState, knockdown);
ARPG_CHECKPOINT_ENUM_MATCH(ReactionState, checkpoint::ReactionState, rising);
ARPG_CHECKPOINT_ENUM_MATCH(ReactionState, checkpoint::ReactionState, defeated);
ARPG_CHECKPOINT_ENUM_MATCH(
    ReactionState, checkpoint::ReactionState, respawning);
ARPG_CHECKPOINT_ENUM_MATCH(ArmorState, checkpoint::ArmorState, none);
ARPG_CHECKPOINT_ENUM_MATCH(ArmorState, checkpoint::ArmorState, armored);
ARPG_CHECKPOINT_ENUM_MATCH(ArmorState, checkpoint::ArmorState, broken);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterAiPhase, checkpoint::MonsterAiPhase, idle);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterAiPhase, checkpoint::MonsterAiPhase, move);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAiPhase, checkpoint::MonsterAiPhase, telegraph);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterAiPhase, checkpoint::MonsterAiPhase, active);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAiPhase, checkpoint::MonsterAiPhase, recovery);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAiPhase, checkpoint::MonsterAiPhase, cooldown);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAiPhase, checkpoint::MonsterAiPhase, defeated);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixWarning, checkpoint::MonsterAffixWarning, none);
ARPG_CHECKPOINT_ENUM_MATCH(
    MonsterAffixWarning, checkpoint::MonsterAffixWarning, blink);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterAffixWarning,
    checkpoint::MonsterAffixWarning, chain_lightning);
ARPG_CHECKPOINT_ENUM_MATCH(MonsterAffixWarning,
    checkpoint::MonsterAffixWarning, death_blast);
ARPG_CHECKPOINT_ENUM_MATCH(HazardKind, checkpoint::HazardKind, native);
ARPG_CHECKPOINT_ENUM_MATCH(HazardKind, checkpoint::HazardKind, burning);
ARPG_CHECKPOINT_ENUM_MATCH(
    HazardKind, checkpoint::HazardKind, chain_lightning);
ARPG_CHECKPOINT_ENUM_MATCH(HazardKind, checkpoint::HazardKind, death_blast);
ARPG_CHECKPOINT_ENUM_MATCH(HazardKind, checkpoint::HazardKind, thunderstorm);
ARPG_CHECKPOINT_ENUM_MATCH(HazardKind, checkpoint::HazardKind, hunting_flame);
ARPG_CHECKPOINT_ENUM_MATCH(
    HazardKind, checkpoint::HazardKind, chaos_expansion);

#undef ARPG_CHECKPOINT_ENUM_MATCH

bool valid_room_combat_checkpoint_structural(
    const RoomCombatCheckpoint& checkpoint,
    const std::uint32_t generated_monsters) noexcept {
    return ::arpg::checkpoint::valid_room_combat_checkpoint_structural(
        checkpoint, generated_monsters);
}

namespace {

[[nodiscard]] bool finite(const Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y)
        && std::isfinite(value.z);
}

[[nodiscard]] bool valid_position(const Vec3 value) noexcept {
    return finite(value) && value.x >= room_bounds::min_x
        && value.x <= room_bounds::max_x && value.y >= room_bounds::min_y
        && value.y <= room_bounds::max_y && value.z >= 0.0F
        && value.z <= 32.0F;
}

[[nodiscard]] bool same_profile(const MonsterAffixProfile& left,
    const MonsterAffixProfile& right) noexcept {
    return left.max_hp == right.max_hp
        && left.armor_rating == right.armor_rating
        && left.max_shield == right.max_shield
        && left.shield_recharge_delay_ticks
            == right.shield_recharge_delay_ticks
        && left.damage_bp == right.damage_bp
        && left.attack_timing_bp == right.attack_timing_bp
        && left.move_bp == right.move_bp
        && left.cooldown_bp == right.cooldown_bp
        && left.horizontal_impulse_bp == right.horizontal_impulse_bp;
}

[[nodiscard]] checkpoint::CheckpointVec3 to_checkpoint(
    const Vec3 source) noexcept {
    return {source.x, source.y, source.z};
}

[[nodiscard]] Vec3 to_runtime(
    const checkpoint::CheckpointVec3 source) noexcept {
    return {source.x, source.y, source.z};
}

#define ARPG_BOUNDED_CHECKPOINT_ENUM(runtime_type, checkpoint_type, last_value) \
    [[nodiscard]] checkpoint_type to_checkpoint( \
        const runtime_type source) noexcept { \
        return static_cast<checkpoint_type>(source); \
    } \
    [[nodiscard]] bool to_runtime(const checkpoint_type source, \
        runtime_type& destination) noexcept { \
        using SourceValue = std::underlying_type_t<checkpoint_type>; \
        const SourceValue value = static_cast<SourceValue>(source); \
        if (value > static_cast<SourceValue>(checkpoint_type::last_value)) { \
            return false; \
        } \
        destination = static_cast<runtime_type>(value); \
        return true; \
    }

ARPG_BOUNDED_CHECKPOINT_ENUM(
    PlayerState, checkpoint::PlayerState, landing)
ARPG_BOUNDED_CHECKPOINT_ENUM(
    MonsterId, checkpoint::MonsterId, chaos_hazard)
ARPG_BOUNDED_CHECKPOINT_ENUM(PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, unknown)
ARPG_BOUNDED_CHECKPOINT_ENUM(
    modifiers::DamageType, checkpoint::DamageType, chaos)
ARPG_BOUNDED_CHECKPOINT_ENUM(
    MonsterAffixId, checkpoint::MonsterAffixId, death_blast)
ARPG_BOUNDED_CHECKPOINT_ENUM(
    MonsterAffixTier, checkpoint::MonsterAffixTier, m3)
ARPG_BOUNDED_CHECKPOINT_ENUM(
    DummyKind, checkpoint::DummyKind, heavy)
ARPG_BOUNDED_CHECKPOINT_ENUM(
    ReactionState, checkpoint::ReactionState, respawning)
ARPG_BOUNDED_CHECKPOINT_ENUM(
    ArmorState, checkpoint::ArmorState, broken)
ARPG_BOUNDED_CHECKPOINT_ENUM(
    MonsterAiPhase, checkpoint::MonsterAiPhase, defeated)
ARPG_BOUNDED_CHECKPOINT_ENUM(MonsterAffixWarning,
    checkpoint::MonsterAffixWarning, death_blast)
ARPG_BOUNDED_CHECKPOINT_ENUM(
    HazardKind, checkpoint::HazardKind, chaos_expansion)

#undef ARPG_BOUNDED_CHECKPOINT_ENUM

[[nodiscard]] checkpoint::AttackId to_checkpoint(
    const AttackId source) noexcept {
    return static_cast<checkpoint::AttackId>(source);
}

[[nodiscard]] bool to_runtime(const checkpoint::AttackId source,
    AttackId& destination) noexcept {
    if (source != checkpoint::AttackId::none
            && source > checkpoint::AttackId::air_j) return false;
    destination = static_cast<AttackId>(source);
    return true;
}

[[nodiscard]] checkpoint::Facing to_checkpoint(
    const Facing source) noexcept {
    return static_cast<checkpoint::Facing>(source);
}

[[nodiscard]] bool to_runtime(const checkpoint::Facing source,
    Facing& destination) noexcept {
    if (source != checkpoint::Facing::left
            && source != checkpoint::Facing::right) return false;
    destination = static_cast<Facing>(source);
    return true;
}

void to_checkpoint(const MonsterOrdinalSet& source,
    checkpoint::MonsterOrdinalSet& destination) noexcept {
    destination.words = source.words;
}

void to_runtime(const checkpoint::MonsterOrdinalSet& source,
    MonsterOrdinalSet& destination) noexcept {
    destination.words = source.words;
}

void to_checkpoint(const MonsterAffixProfile& source,
    checkpoint::MonsterAffixProfile& destination) noexcept {
    destination.max_hp = source.max_hp;
    destination.armor_rating = source.armor_rating;
    destination.max_shield = source.max_shield;
    destination.shield_recharge_delay_ticks =
        source.shield_recharge_delay_ticks;
    destination.damage_bp = source.damage_bp;
    destination.attack_timing_bp = source.attack_timing_bp;
    destination.move_bp = source.move_bp;
    destination.cooldown_bp = source.cooldown_bp;
    destination.horizontal_impulse_bp = source.horizontal_impulse_bp;
}

void to_runtime(const checkpoint::MonsterAffixProfile& source,
    MonsterAffixProfile& destination) noexcept {
    destination.max_hp = source.max_hp;
    destination.armor_rating = source.armor_rating;
    destination.max_shield = source.max_shield;
    destination.shield_recharge_delay_ticks =
        source.shield_recharge_delay_ticks;
    destination.damage_bp = source.damage_bp;
    destination.attack_timing_bp = source.attack_timing_bp;
    destination.move_bp = source.move_bp;
    destination.cooldown_bp = source.cooldown_bp;
    destination.horizontal_impulse_bp = source.horizontal_impulse_bp;
}

void to_checkpoint(const MonsterAffixSet& source,
    checkpoint::MonsterAffixSet& destination) noexcept {
    destination = {};
    destination.count = source.count;
    for (std::size_t index = 0U; index < destination.values.size(); ++index) {
        destination.values[index].id = to_checkpoint(source.values[index].id);
        destination.values[index].tier =
            to_checkpoint(source.values[index].tier);
    }
}

[[nodiscard]] bool to_runtime(const checkpoint::MonsterAffixSet& source,
    MonsterAffixSet& destination) noexcept {
    MonsterAffixSet converted{};
    if (source.count > source.values.size()) return false;
    converted.count = source.count;
    for (std::size_t index = 0U; index < source.values.size(); ++index) {
        if (!to_runtime(source.values[index].id, converted.values[index].id)
                || !to_runtime(source.values[index].tier,
                    converted.values[index].tier)) return false;
    }
    destination = converted;
    return true;
}

void to_checkpoint(const PlayerDamageSource& source,
    checkpoint::PlayerDamageSource& destination) noexcept {
    destination.kind = to_checkpoint(source.kind);
    destination.monster = to_checkpoint(source.monster);
    destination.detail_id = source.detail_id;
}

[[nodiscard]] bool to_runtime(const checkpoint::PlayerDamageSource& source,
    PlayerDamageSource& destination) noexcept {
    PlayerDamageSource converted{};
    if (!to_runtime(source.kind, converted.kind)) return false;
    const bool source_has_monster = converted.kind
            != PlayerDamageSourceKind::abyss_environment
        && converted.kind != PlayerDamageSourceKind::unknown;
    if (source_has_monster) {
        if (!to_runtime(source.monster, converted.monster)) return false;
    } else {
        if (source.monster != checkpoint::MonsterId::count) return false;
        converted.monster = MonsterId::count;
    }
    converted.detail_id = source.detail_id;
    destination = converted;
    return true;
}

void to_checkpoint(const PlayerStatusRuntime& source,
    checkpoint::PlayerStatusRuntime& destination) noexcept {
    destination.slow_bp = source.slow_bp;
    destination.slow_ticks = source.slow_ticks;
    destination.corrosion_damage_per_second =
        source.corrosion_damage_per_second;
    destination.corrosion_ticks = source.corrosion_ticks;
    destination.corrosion_tick_phase = source.corrosion_tick_phase;
    to_checkpoint(source.corrosion_source, destination.corrosion_source);
}

[[nodiscard]] bool to_runtime(const checkpoint::PlayerStatusRuntime& source,
    PlayerStatusRuntime& destination) noexcept {
    PlayerStatusRuntime converted{};
    if (!to_runtime(source.corrosion_source, converted.corrosion_source)) {
        return false;
    }
    converted.slow_bp = source.slow_bp;
    converted.slow_ticks = source.slow_ticks;
    converted.corrosion_damage_per_second =
        source.corrosion_damage_per_second;
    converted.corrosion_ticks = source.corrosion_ticks;
    converted.corrosion_tick_phase = source.corrosion_tick_phase;
    destination = converted;
    return true;
}

void to_checkpoint(const PlayerDefenseSnapshot& source,
    checkpoint::PlayerDefenseSnapshot& destination) noexcept {
    destination.hp = source.hp;
    destination.max_hp = source.max_hp;
    destination.barrier = source.barrier;
    destination.max_barrier = source.max_barrier;
    destination.armor = source.armor;
    destination.evasion = source.evasion;
    destination.armor_reduction_bp = source.armor_reduction_bp;
    destination.evasion_rate_bp = source.evasion_rate_bp;
    destination.damage_reduction = source.damage_reduction;
    destination.damage_reduction_cap = source.damage_reduction_cap;
}

void to_runtime(const checkpoint::PlayerDefenseSnapshot& source,
    PlayerDefenseSnapshot& destination) noexcept {
    destination.hp = source.hp;
    destination.max_hp = source.max_hp;
    destination.barrier = source.barrier;
    destination.max_barrier = source.max_barrier;
    destination.armor = source.armor;
    destination.evasion = source.evasion;
    destination.armor_reduction_bp = source.armor_reduction_bp;
    destination.evasion_rate_bp = source.evasion_rate_bp;
    destination.damage_reduction = source.damage_reduction;
    destination.damage_reduction_cap = source.damage_reduction_cap;
}

void to_checkpoint(const CombatDeathSnapshot& source,
    checkpoint::CombatDeathSnapshot& destination) noexcept {
    destination.tick = source.tick;
    to_checkpoint(source.source, destination.source);
    destination.primary_type = to_checkpoint(source.primary_type);
    destination.raw_damage = source.raw_damage;
    destination.barrier_loss = source.barrier_loss;
    destination.health_loss = source.health_loss;
    destination.final_damage = source.final_damage;
    destination.recent_damage = source.recent_damage;
    to_checkpoint(source.defense, destination.defense);
}

[[nodiscard]] bool to_runtime(const checkpoint::CombatDeathSnapshot& source,
    CombatDeathSnapshot& destination) noexcept {
    CombatDeathSnapshot converted{};
    if (!to_runtime(source.source, converted.source)
            || !to_runtime(source.primary_type, converted.primary_type)) {
        return false;
    }
    converted.tick = source.tick;
    converted.raw_damage = source.raw_damage;
    converted.barrier_loss = source.barrier_loss;
    converted.health_loss = source.health_loss;
    converted.final_damage = source.final_damage;
    converted.recent_damage = source.recent_damage;
    to_runtime(source.defense, converted.defense);
    destination = converted;
    return true;
}

void to_checkpoint(const AbyssEnvironmentRuntime& source,
    checkpoint::AbyssEnvironmentRuntime& destination) noexcept {
    destination.rule = source.rule;
    destination.cycle_tick = source.cycle_tick;
    destination.stage_tick = source.stage_tick;
    destination.locked_center = to_checkpoint(source.locked_center);
    destination.expansion_stage = source.expansion_stage;
    destination.warning = source.warning;
    destination.active = source.active;
}

void to_runtime(const checkpoint::AbyssEnvironmentRuntime& source,
    AbyssEnvironmentRuntime& destination) noexcept {
    destination.rule = source.rule;
    destination.cycle_tick = source.cycle_tick;
    destination.stage_tick = source.stage_tick;
    destination.locked_center = to_runtime(source.locked_center);
    destination.expansion_stage = source.expansion_stage;
    destination.warning = source.warning;
    destination.active = source.active;
}

void capture_neutral_monster(const MonsterRuntime& source,
    checkpoint::MonsterCombatCheckpoint& destination) noexcept {
    destination = {};
    destination.ordinal = source.monster_ordinal;
    destination.id = to_checkpoint(source.id);
    to_checkpoint(source.affixes, destination.affixes);
    to_checkpoint(source.affix_profile, destination.affix_profile);
    destination.kind = to_checkpoint(source.kind);
    destination.spawn = to_checkpoint(source.spawn);
    destination.position = to_checkpoint(source.position);
    destination.velocity = to_checkpoint(source.velocity);
    destination.facing = to_checkpoint(source.facing);
    destination.reaction = to_checkpoint(source.reaction);
    destination.armor = to_checkpoint(source.armor);
    destination.reaction_ticks = source.reaction_ticks;
    destination.ai_phase = to_checkpoint(source.ai_phase);
    destination.ai_ticks = source.ai_ticks;
    destination.attack_serial = source.attack_serial;
    destination.contact_attack_resolved = source.contact_attack_resolved;
    destination.hp = source.hp;
    destination.max_hp = source.max_hp;
    destination.break_value = source.break_value;
    destination.max_break = source.max_break;
    destination.shield = source.shield;
    destination.max_shield = source.max_shield;
    destination.shield_ticks = source.shield_ticks;
    destination.max_shield_ticks = source.max_shield_ticks;
    destination.shield_recharge_ticks = source.shield_recharge_ticks;
    destination.break_window_ticks = source.break_window_ticks;
    destination.owner_transient_counter = source.owner_transient_counter;
    destination.burning_ground_ticks = source.burning_ground_ticks;
    destination.blink_assault_ticks = source.blink_assault_ticks;
    destination.blink_empowered = source.blink_empowered;
    destination.attack_target_position =
        to_checkpoint(source.attack_target_position);
    destination.attack_vector = to_checkpoint(source.attack_vector);
    destination.affix_warning = to_checkpoint(source.affix_warning);
    destination.affix_warning_ticks = source.affix_warning_ticks;
    source.effects.capture_checkpoint(destination.effects);
    destination.effects_touched = source.effects_touched;
}

void capture_neutral_monster(const MonsterPersistentState& source,
    checkpoint::MonsterCombatCheckpoint& destination) noexcept {
    destination = {};
    destination.ordinal = source.spawn_ordinal;
    destination.id = to_checkpoint(source.id);
    to_checkpoint(source.affixes, destination.affixes);
    to_checkpoint(source.affix_profile, destination.affix_profile);
    destination.kind = to_checkpoint(source.kind);
    destination.spawn = to_checkpoint(source.spawn);
    destination.position = to_checkpoint(source.position);
    destination.velocity = to_checkpoint(source.velocity);
    destination.facing = to_checkpoint(source.facing);
    destination.reaction = to_checkpoint(source.reaction);
    destination.armor = to_checkpoint(source.armor);
    destination.reaction_ticks = source.reaction_ticks;
    destination.ai_phase = to_checkpoint(source.ai_phase);
    destination.ai_ticks = source.ai_ticks;
    destination.attack_serial = source.attack_serial;
    destination.contact_attack_resolved = source.contact_attack_resolved;
    destination.hp = source.hp;
    destination.max_hp = source.max_hp;
    destination.break_value = source.break_value;
    destination.max_break = source.max_break;
    destination.shield = source.shield;
    destination.max_shield = source.max_shield;
    destination.shield_ticks = source.shield_ticks;
    destination.max_shield_ticks = source.max_shield_ticks;
    destination.shield_recharge_ticks = source.shield_recharge_ticks;
    destination.break_window_ticks = source.break_window_ticks;
    destination.owner_transient_counter = source.owner_transient_counter;
    destination.burning_ground_ticks = source.burning_ground_ticks;
    destination.blink_assault_ticks = source.blink_assault_ticks;
    destination.blink_empowered = source.blink_empowered;
    destination.attack_target_position =
        to_checkpoint(source.attack_target_position);
    destination.attack_vector = to_checkpoint(source.attack_vector);
    destination.affix_warning = to_checkpoint(source.affix_warning);
    destination.affix_warning_ticks = source.affix_warning_ticks;
    source.effects.capture_checkpoint(destination.effects);
    destination.effects_touched = source.effects_touched;
}

[[nodiscard]] bool to_runtime(
    const checkpoint::MonsterCombatCheckpoint& source,
    MonsterPersistentState& destination) noexcept {
    MonsterPersistentState converted{};
    if (!to_runtime(source.id, converted.id)
            || !to_runtime(source.affixes, converted.affixes)
            || !to_runtime(source.kind, converted.kind)
            || !to_runtime(source.facing, converted.facing)
            || !to_runtime(source.reaction, converted.reaction)
            || !to_runtime(source.armor, converted.armor)
            || !to_runtime(source.ai_phase, converted.ai_phase)
            || !to_runtime(source.affix_warning,
                converted.affix_warning)
            || !converted.effects.restore_checkpoint(source.effects)) {
        return false;
    }
    converted.spawn_ordinal = source.ordinal;
    to_runtime(source.affix_profile, converted.affix_profile);
    converted.spawn = to_runtime(source.spawn);
    converted.position = to_runtime(source.position);
    converted.velocity = to_runtime(source.velocity);
    converted.reaction_ticks = source.reaction_ticks;
    converted.ai_ticks = source.ai_ticks;
    converted.attack_serial = source.attack_serial;
    converted.contact_attack_resolved = source.contact_attack_resolved;
    converted.hp = source.hp;
    converted.max_hp = source.max_hp;
    converted.break_value = source.break_value;
    converted.max_break = source.max_break;
    converted.shield = source.shield;
    converted.max_shield = source.max_shield;
    converted.shield_ticks = source.shield_ticks;
    converted.max_shield_ticks = source.max_shield_ticks;
    converted.shield_recharge_ticks = source.shield_recharge_ticks;
    converted.break_window_ticks = source.break_window_ticks;
    converted.hit_stop_ticks = 0U;
    converted.owner_transient_counter = source.owner_transient_counter;
    converted.burning_ground_ticks = source.burning_ground_ticks;
    converted.blink_assault_ticks = source.blink_assault_ticks;
    converted.blink_empowered = source.blink_empowered;
    converted.attack_target_position =
        to_runtime(source.attack_target_position);
    converted.attack_vector = to_runtime(source.attack_vector);
    converted.affix_warning_ticks = source.affix_warning_ticks;
    converted.effects_touched = source.effects_touched;
    converted.touched = true;
    converted.defeated = false;
    destination = converted;
    return true;
}

[[nodiscard]] bool inside(const Aabb& bounds,
    const checkpoint::CheckpointVec3 position) noexcept {
    return position.x >= bounds.minimum.x && position.x <= bounds.maximum.x
        && position.y >= bounds.minimum.y && position.y <= bounds.maximum.y
        && position.z >= bounds.minimum.z && position.z <= bounds.maximum.z;
}

[[nodiscard]] bool same_crate(const checkpoint::FireRoomCrateSnapshot& left,
    const FireRoomCrateSnapshot& right) noexcept {
    return left.position.x == right.position.x
        && left.position.y == right.position.y
        && left.position.z == right.position.z
        && left.broken_tick == right.broken_tick
        && left.intact == right.intact;
}

}  // namespace

bool CombatWorld::capture_room_checkpoint(
    checkpoint::RoomCombatCheckpoint& out) const noexcept {
    checkpoint::clear_room_combat_checkpoint(out);
    out.tick = tick_;
    out.evasion_rng_state = evasion_rng_.export_state();
    out.player.position = to_checkpoint(player_.position);
    out.player.velocity = to_checkpoint(player_.velocity);
    out.player.facing = to_checkpoint(player_.facing);
    out.player.state = to_checkpoint(player_.state);
    out.player.combo_stage = player_.combo_stage;
    out.player.air_attack_available = player_.air_attack_available;
    out.player.hp = player_.hp;
    out.player.max_hp = player_.max_hp;
    out.player.barrier = player_.barrier;
    out.player.max_barrier = player_.max_barrier;
    out.player.damage_reduction = player_.damage_reduction;
    out.player.damage_reduction_cap = player_.damage_reduction_cap;
    out.player.armor = player_.armor;
    out.player.evasion = player_.evasion;
    out.player.armor_reduction_bp = player_.armor_reduction_bp;
    out.player.evasion_rate_bp = player_.evasion_rate_bp;
    out.player.hurt_ticks = player_.hurt_ticks;
    out.player.invulnerability_ticks = player_.invulnerability_ticks;
    to_checkpoint(player_.status, out.player.status);
    out.player.skill_cooldowns = active_skill_.cooldowns;
    if (active_skill_.snapshot.id != skills::ActiveSkillId::none
            || (attack_.id == AttackId::none
                && player_.state >= PlayerState::attack_startup
                && player_.state <= PlayerState::attack_recovery)) {
        if (player_.position.z > 0.0F) {
            out.player.state = player_.velocity.z > 0.0F
                ? checkpoint::PlayerState::jump_rise
                : checkpoint::PlayerState::jump_fall;
        } else {
            out.player.state = checkpoint::PlayerState::idle;
        }
    }
    to_checkpoint(abyss_environment_, out.abyss_environment);
    out.attack.id = to_checkpoint(attack_.id);
    out.attack.elapsed_ticks = attack_.elapsed_ticks;
    out.attack.startup_ticks = attack_.startup_ticks;
    out.attack.recovery_ticks = attack_.recovery_ticks;
    out.attack.connected = attack_.connected;
    out.attack.impact_event_emitted = attack_.impact_event_emitted;
    to_checkpoint(attack_.hit_targets, out.attack.hit_targets);
    if (out.attack.id == checkpoint::AttackId::none) {
        out.player.combo_stage = 0U;
    }

    if (room_monster_field_ != nullptr) {
        for (std::uint32_t ordinal = 0U;
                ordinal < room_monster_field_->total_count(); ++ordinal) {
            const auto value = static_cast<MonsterOrdinal>(ordinal);
            const MonsterRuntime* active =
                room_monster_field_->active_runtime(value);
            const MonsterPersistentState* persistent =
                room_monster_field_->persistent_state(value);
            if (persistent == nullptr || persistent->defeated
                    || (active == nullptr && !persistent->touched)) continue;
            if (out.monster_count >= out.monsters.size()) return false;
            if (active != nullptr) {
                capture_neutral_monster(
                    *active, out.monsters[out.monster_count]);
            } else {
                capture_neutral_monster(
                    *persistent, out.monsters[out.monster_count]);
            }
            ++out.monster_count;
        }
    } else {
        for (const MonsterRuntime& monster : monsters_.slots()) {
            if (!monster.active || monster.hp <= 0) continue;
            if (out.monster_count >= out.monsters.size()) return false;
            capture_neutral_monster(monster,
                out.monsters[out.monster_count++]);
        }
        std::sort(out.monsters.begin(),
            out.monsters.begin() + out.monster_count,
            [](const checkpoint::MonsterCombatCheckpoint& left,
                    const checkpoint::MonsterCombatCheckpoint& right) noexcept {
                return left.ordinal < right.ordinal;
            });
    }

    if (room_obstacles_ != nullptr) {
        for (std::uint16_t ordinal = 0U;
                ordinal < kRoomEnvironmentRecordCapacity; ++ordinal) {
            const RoomObstacleState* obstacle = room_obstacles_->state(ordinal);
            if (obstacle == nullptr) continue;
            if (out.obstacle_count >= out.obstacles.size()) return false;
            auto& target = out.obstacles[out.obstacle_count++];
            target.ordinal = obstacle->ordinal;
            target.hp = obstacle->hp;
            target.max_hp = obstacle->max_hp;
            target.broken_tick = obstacle->broken_tick;
            target.intact = obstacle->intact;
            obstacle->effects.capture_checkpoint(target.effects);
        }
    }
    for (std::size_t index = 0U; index < out.fire_crates.size(); ++index) {
        out.fire_crates[index].position =
            to_checkpoint(fire_crates_[index].position);
        out.fire_crates[index].broken_tick = fire_crates_[index].broken_tick;
        out.fire_crates[index].intact = fire_crates_[index].intact;
    }
    out.fire_crate_count = room_obstacles_ == nullptr
            && encounter_config_.fire_room_obstacles
        ? static_cast<std::uint8_t>(fire_crates_.size()) : 0U;
    player_damage_history_.capture_checkpoint(out.player_damage_history);
    out.has_death_snapshot = death_snapshot_.has_value();
    if (death_snapshot_.has_value()) {
        to_checkpoint(*death_snapshot_, out.death_snapshot);
    }
    const std::uint32_t generated = room_monster_field_ != nullptr
        ? room_monster_field_->total_count()
        : static_cast<std::uint32_t>(limits::kRoomMonsterCapacity);
    return checkpoint::valid_room_combat_checkpoint_structural(out, generated);
}

bool CombatWorld::restore_room_checkpoint(
    const checkpoint::RoomCombatCheckpoint& checkpoint) noexcept {
    const std::uint32_t generated = room_monster_field_ != nullptr
        ? room_monster_field_->total_count()
        : static_cast<std::uint32_t>(limits::kRoomMonsterCapacity);
    core::DeterministicRng checked_rng{1U};
    PlayerDamageHistory checked_history{};
    Facing checked_facing{};
    PlayerState checked_player_state{};
    PlayerStatusRuntime checked_status{};
    AttackId checked_attack_id{};
    MonsterOrdinalSet checked_hit_targets{};
    AbyssEnvironmentRuntime checked_abyss{};
    CombatDeathSnapshot checked_death{};
    if (!::arpg::checkpoint::valid_room_combat_checkpoint_structural(
                checkpoint, generated)
            || !checked_rng.import_state(checkpoint.evasion_rng_state)
            || !checked_history.restore_checkpoint(
                checkpoint.player_damage_history)
            || !to_runtime(checkpoint.player.facing, checked_facing)
            || !to_runtime(checkpoint.player.state, checked_player_state)
            || !to_runtime(checkpoint.player.status, checked_status)
            || !to_runtime(checkpoint.attack.id, checked_attack_id)
            || (checkpoint.has_death_snapshot
                && !to_runtime(checkpoint.death_snapshot, checked_death))) {
        return false;
    }
    to_runtime(checkpoint.attack.hit_targets, checked_hit_targets);
    to_runtime(checkpoint.abyss_environment, checked_abyss);

    if (checkpoint.player.max_hp != player_.max_hp
            || checkpoint.player.max_barrier != player_.max_barrier
            || checkpoint.player.damage_reduction
                != player_.damage_reduction
            || checkpoint.player.damage_reduction_cap
                != player_.damage_reduction_cap
            || checkpoint.player.armor != player_.armor
            || checkpoint.player.evasion != player_.evasion
            || checkpoint.player.armor_reduction_bp
                != player_.armor_reduction_bp
            || checkpoint.player.evasion_rate_bp
                != player_.evasion_rate_bp) return false;

    if (checked_attack_id != AttackId::none) {
        const AttackDefinition* definition =
            find_attack_definition(checked_attack_id);
        if (definition == nullptr) return false;
        const auto& build = encounter_config_.player_build;
        const std::int64_t attack_speed = build.values.attack_speed
            * (modifiers::kFixedOne
                + static_cast<std::int64_t>(build.local_attack_speed_bp))
            / modifiers::kFixedOne;
        const std::uint8_t expected_combo = checked_attack_id == AttackId::j1
            ? 1U : checked_attack_id == AttackId::j2 ? 2U
                : checked_attack_id == AttackId::j3 ? 3U : 0U;
        const AttackPhase phase = attack_phase_at(*definition,
            checkpoint.attack.elapsed_ticks,
            checkpoint.attack.startup_ticks,
            checkpoint.attack.recovery_ticks);
        const PlayerState expected_state = phase == AttackPhase::startup
            ? PlayerState::attack_startup : phase == AttackPhase::active
                ? PlayerState::attack_active : PlayerState::attack_recovery;
        if (checkpoint.attack.startup_ticks != scaled_phase_ticks(
                    definition->startup_ticks, attack_speed)
                || checkpoint.attack.recovery_ticks != scaled_phase_ticks(
                    definition->recovery_ticks, attack_speed)
                || checkpoint.player.combo_stage != expected_combo
                || (checked_player_state != expected_state
                    && !(checked_attack_id == AttackId::air_j
                        && checked_player_state == PlayerState::landing
                        && checkpoint.player.position.z == 0.0F
                        && checkpoint.player.velocity.z == 0.0F))) {
            return false;
        }
    }

    if (room_monster_field_ == nullptr) {
        for (std::uint32_t ordinal = 0U;
                ordinal < limits::kRoomMonsterCapacity; ++ordinal) {
            if (checkpoint.attack.hit_targets.contains(
                    static_cast<::arpg::checkpoint::MonsterOrdinal>(ordinal))
                    && active_monster_by_ordinal(
                        static_cast<MonsterOrdinal>(ordinal)) == nullptr) {
                return false;
            }
        }
    }

    const auto& abyss_config = encounter_config_.abyss;
    const auto& environment_config = abyss_config.environment;
    if (checked_abyss.rule != abyss_config.rule
            || checked_abyss.active != environment_config.active
            || !finite(checked_abyss.locked_center)
            || (environment_config.radius_count == 0U
                && checked_abyss.expansion_stage != 0xFFU)
            || (environment_config.radius_count != 0U
                && checked_abyss.expansion_stage != 0xFFU
                && checked_abyss.expansion_stage
                    >= environment_config.radius_count)
            || (environment_config.expansion_interval_ticks != 0U
                && checked_abyss.stage_tick
                    >= environment_config.expansion_interval_ticks)
            || (environment_config.cycle_ticks != 0U
                && checked_abyss.cycle_tick
                    >= environment_config.cycle_ticks)) return false;
    const bool zero_center = checked_abyss.locked_center.x == 0.0F
        && checked_abyss.locked_center.y == 0.0F
        && checked_abyss.locked_center.z == 0.0F;
    if (!environment_config.active) {
        if (checked_abyss.cycle_tick != 0U
                || checked_abyss.stage_tick != 0U
                || !zero_center || checked_abyss.warning
                || checked_abyss.expansion_stage != 0xFFU) return false;
    } else if (environment_config.expansion_interval_ticks != 0U) {
        const std::uint64_t alive_tick = checkpoint.tick == 0U
            ? 0U : checkpoint.tick - 1U;
        const std::uint64_t death_tick = checkpoint.has_death_snapshot
            ? checkpoint.death_snapshot.tick : alive_tick;
        const std::uint64_t before_death_tick = death_tick == 0U
            ? 0U : death_tick - 1U;
        const auto matches_expansion_tick = [&](const std::uint64_t tick) {
            const std::uint8_t expected_stage = static_cast<std::uint8_t>(
                (std::min)(tick / environment_config.expansion_interval_ticks,
                    static_cast<std::uint64_t>(
                        environment_config.radius_count - 1U)));
            return checked_abyss.stage_tick
                    == tick % environment_config.expansion_interval_ticks
                && checked_abyss.expansion_stage == expected_stage;
        };
        const bool valid_simulated_tick = checkpoint.has_death_snapshot
            ? matches_expansion_tick(death_tick)
                || matches_expansion_tick(before_death_tick)
            : matches_expansion_tick(alive_tick);
        if (checked_abyss.cycle_tick != 0U || !zero_center
                || checked_abyss.warning
                || checked_abyss.expansion_stage == 0xFFU
                || !valid_simulated_tick) return false;
    } else if (environment_config.cycle_ticks != 0U) {
        const std::uint64_t alive_tick = checkpoint.tick == 0U
            ? 0U : checkpoint.tick - 1U;
        const std::uint64_t death_tick = checkpoint.has_death_snapshot
            ? checkpoint.death_snapshot.tick : alive_tick;
        const std::uint64_t before_death_tick = death_tick == 0U
            ? 0U : death_tick - 1U;
        const auto matches_cycle_tick = [&](const std::uint64_t tick) {
            return checked_abyss.cycle_tick
                == tick % environment_config.cycle_ticks;
        };
        const bool valid_simulated_tick = checkpoint.has_death_snapshot
            ? matches_cycle_tick(death_tick)
                || matches_cycle_tick(before_death_tick)
            : matches_cycle_tick(alive_tick);
        if (checked_abyss.expansion_stage != 0xFFU
                || checked_abyss.stage_tick != 0U
                || !valid_position(checked_abyss.locked_center)
                || !valid_simulated_tick) return false;
    }

    RoomResidentOrdinals desired{};
    if (room_monster_field_ != nullptr) {
        desired = room_monster_field_->required_residents(
            make_room_streaming_region(
                to_runtime(checkpoint.player.position)));
        if (desired.fault != RoomMonsterFieldFault::none
                || desired.count > kMonsterCapacity) return false;
    }

    ::arpg::checkpoint::MonsterOrdinal previous =
        ::arpg::checkpoint::kInvalidMonsterOrdinal;
    for (std::uint16_t index = 0U; index < checkpoint.monster_count; ++index) {
        const auto& source = checkpoint.monsters[index];
        MonsterPersistentState converted{};
        if (!to_runtime(source, converted)
                || (index != 0U && source.ordinal <= previous)) return false;
        previous = source.ordinal;
        if (room_monster_field_ != nullptr) {
            const MonsterPersistentState* current =
                room_monster_field_->persistent_state(source.ordinal);
            const RoomMonsterBlueprint& blueprint =
                room_monster_field_->plan().monsters[source.ordinal];
            if (current == nullptr || current->defeated
                    || converted.id != blueprint.id
                    || !(converted.affixes == blueprint.affixes)
                    || converted.kind != current->kind
                    || !same_profile(converted.affix_profile,
                        current->affix_profile)
                    || converted.max_hp != current->max_hp
                    || converted.max_break != current->max_break
                    || converted.max_shield != current->max_shield
                    || converted.max_shield_ticks
                        != current->max_shield_ticks
                    || converted.spawn.x != blueprint.initial_position.x
                    || converted.spawn.y != blueprint.initial_position.y
                    || converted.spawn.z != blueprint.initial_position.z) {
                return false;
            }
        } else {
            const MonsterRuntime* current =
                active_monster_by_ordinal(source.ordinal);
            if (current == nullptr || converted.id != current->id
                    || !(converted.affixes == current->affixes)
                    || converted.kind != current->kind
                    || !same_profile(converted.affix_profile,
                        current->affix_profile)
                    || converted.max_hp != current->max_hp
                    || converted.max_break != current->max_break
                    || converted.max_shield != current->max_shield
                    || converted.max_shield_ticks
                        != current->max_shield_ticks
                    || converted.spawn.x != current->spawn.x
                    || converted.spawn.y != current->spawn.y
                    || converted.spawn.z != current->spawn.z) return false;
        }
    }

    std::uint16_t previous_obstacle = 0U;
    for (std::uint16_t index = 0U; index < checkpoint.obstacle_count; ++index) {
        const auto& value = checkpoint.obstacles[index];
        const RoomObstacleState* original = room_obstacles_ == nullptr
            ? nullptr : room_obstacles_->state(value.ordinal);
        modifiers::EffectSet effects{};
        if (original == nullptr || (index != 0U
                && value.ordinal <= previous_obstacle)
                || value.max_hp != original->max_hp
                || (original->kind != RoomObstacleKind::breakable
                    && (value.hp != 0U || value.max_hp != 0U
                        || value.broken_tick != 0U || !value.intact))
                || (value.intact
                    ? (original->kind == RoomObstacleKind::breakable
                        && (value.hp == 0U || value.hp > value.max_hp
                            || value.broken_tick != 0U))
                    : (original->kind != RoomObstacleKind::breakable
                        || value.hp != 0U
                        || value.broken_tick > checkpoint.tick))
                || !effects.restore_checkpoint(value.effects)) return false;
        previous_obstacle = value.ordinal;
    }
    if (room_obstacles_ == nullptr && checkpoint.obstacle_count != 0U) {
        return false;
    }
    if (room_obstacles_ != nullptr
            && checkpoint.obstacle_count
                != room_obstacles_->obstacle_count()) return false;

    const std::uint8_t expected_crate_count = room_obstacles_ == nullptr
            && encounter_config_.fire_room_obstacles
        ? static_cast<std::uint8_t>(kFireRoomCrateCapacity) : 0U;
    if (checkpoint.fire_crate_count != expected_crate_count) return false;
    const std::array<Vec3, kFireRoomCrateCapacity> expected_positions{{
        {-3.20F, 0.0F, 0.0F}, {3.20F, 0.0F, 0.0F}}};
    for (std::size_t index = 0U; index < checkpoint.fire_crates.size();
            ++index) {
        const auto& crate = checkpoint.fire_crates[index];
        if (index >= checkpoint.fire_crate_count) {
            if (!same_crate(crate, FireRoomCrateSnapshot{})) return false;
            continue;
        }
        const Vec3 position = to_runtime(crate.position);
        if (!finite(position)
                || position.x != expected_positions[index].x
                || position.y != expected_positions[index].y
                || position.z != expected_positions[index].z
                || (crate.intact && crate.broken_tick != 0U)
                || (!crate.intact && crate.broken_tick > checkpoint.tick)) {
            return false;
        }
    }

    for (std::uint16_t obstacle_index = 0U;
            obstacle_index < checkpoint.obstacle_count; ++obstacle_index) {
        const auto& obstacle = checkpoint.obstacles[obstacle_index];
        if (!obstacle.intact) continue;
        const RoomObstacleState* original =
            room_obstacles_->state(obstacle.ordinal);
        if (original == nullptr
                || inside(original->bounds, checkpoint.player.position)) {
            return false;
        }
        for (std::uint16_t monster_index = 0U;
                monster_index < checkpoint.monster_count; ++monster_index) {
            if (inside(original->bounds,
                    checkpoint.monsters[monster_index].position)) return false;
        }
    }

    tick_ = checkpoint.tick;
    static_cast<void>(evasion_rng_.import_state(checkpoint.evasion_rng_state));
    player_.position = to_runtime(checkpoint.player.position);
    player_.velocity = to_runtime(checkpoint.player.velocity);
    player_.facing = checked_facing;
    player_.state = checked_player_state;
    player_.combo_stage = checkpoint.player.combo_stage;
    player_.hit_stop_ticks = 0U;
    player_.air_attack_available = checkpoint.player.air_attack_available;
    player_.hp = checkpoint.player.hp;
    player_.max_hp = checkpoint.player.max_hp;
    player_.barrier = checkpoint.player.barrier;
    player_.max_barrier = checkpoint.player.max_barrier;
    player_.damage_reduction = checkpoint.player.damage_reduction;
    player_.damage_reduction_cap = checkpoint.player.damage_reduction_cap;
    player_.armor = checkpoint.player.armor;
    player_.evasion = checkpoint.player.evasion;
    player_.armor_reduction_bp = checkpoint.player.armor_reduction_bp;
    player_.evasion_rate_bp = checkpoint.player.evasion_rate_bp;
    player_.hurt_ticks = checkpoint.player.hurt_ticks;
    player_.invulnerability_ticks = checkpoint.player.invulnerability_ticks;
    player_.status = checked_status;
    attack_.id = checked_attack_id;
    attack_.elapsed_ticks = checkpoint.attack.elapsed_ticks;
    attack_.startup_ticks = checkpoint.attack.startup_ticks;
    attack_.recovery_ticks = checkpoint.attack.recovery_ticks;
    attack_.connected = checkpoint.attack.connected;
    attack_.impact_event_emitted = checkpoint.attack.impact_event_emitted;
    attack_.hit_targets = checked_hit_targets;
    active_skill_ = {};
    active_skill_.cooldowns = checkpoint.player.skill_cooldowns;
    abyss_environment_ = checked_abyss;
    player_damage_history_ = checked_history;

    if (room_monster_field_ != nullptr) {
        room_monster_field_->discard_active_residency();
        for (std::uint16_t index = 0U;
                index < checkpoint.monster_count; ++index) {
            MonsterPersistentState converted{};
            const bool converted_ok =
                to_runtime(checkpoint.monsters[index], converted);
            assert(converted_ok);
            static_cast<void>(converted_ok);
            MonsterPersistentState* state =
                room_monster_field_->persistent_state(
                    checkpoint.monsters[index].ordinal);
            assert(state != nullptr);
            *state = converted;
        }
        room_monster_field_->rebind_active_pool(monsters_);
        const bool synchronized = room_monster_field_->synchronize_active_region(
            make_room_streaming_region(player_.position));
        assert(synchronized);
        static_cast<void>(synchronized);
    } else {
        for (std::uint16_t index = 0U;
                index < checkpoint.monster_count; ++index) {
            MonsterRuntime* runtime = active_monster_by_ordinal(
                checkpoint.monsters[index].ordinal);
            assert(runtime != nullptr);
            MonsterPersistentState converted{};
            const bool converted_ok =
                to_runtime(checkpoint.monsters[index], converted);
            assert(converted_ok);
            static_cast<void>(converted_ok);
            restore_monster_persistent_state(converted, *runtime);
        }
    }
    for (std::uint16_t index = 0U; index < checkpoint.obstacle_count; ++index) {
        const auto& source = checkpoint.obstacles[index];
        RoomObstacleState* obstacle = room_obstacles_->state(source.ordinal);
        assert(obstacle != nullptr);
        obstacle->hp = source.hp;
        obstacle->broken_tick = source.broken_tick;
        obstacle->intact = source.intact;
        static_cast<void>(obstacle->effects.restore_checkpoint(source.effects));
    }
    for (std::size_t index = 0U; index < checkpoint.fire_crates.size();
            ++index) {
        fire_crates_[index].position =
            to_runtime(checkpoint.fire_crates[index].position);
        fire_crates_[index].broken_tick =
            checkpoint.fire_crates[index].broken_tick;
        fire_crates_[index].intact = checkpoint.fire_crates[index].intact;
    }
    projectiles_.clear();
    hazards_.clear();
    input_buffer_.clear();
    input_buffer_.reset_diagnostics();
    defeat_ledger_.clear();
    while (events_.try_pop().has_value()) {
    }
    event_overflow_count_ = 0U;
    projectile_saturation_count_ = 0U;
    projectile_invalid_owner_count_ = 0U;
    hazard_saturation_count_ = 0U;
    hazard_invalid_owner_count_ = 0U;
    death_snapshot_ = checkpoint.has_death_snapshot
        ? std::optional<CombatDeathSnapshot>{checked_death}
        : std::nullopt;
    fault_ = CombatFault::none;
    return true;
}

void CombatWorld::adopt_restored_state(CombatWorld&& source) noexcept {
    if (this == &source) return;
    encounter_config_ = std::move(source.encounter_config_);
    legacy_config_ = std::move(source.legacy_config_);
    legacy_mode_ = source.legacy_mode_;
    player_ = std::move(source.player_);
    monsters_ = std::move(source.monsters_);
    room_monster_field_ = std::move(source.room_monster_field_);
    if (room_monster_field_ != nullptr) {
        room_monster_field_->retarget_moved_active_pool(monsters_);
    }
    room_obstacles_ = std::move(source.room_obstacles_);
    projectiles_ = std::move(source.projectiles_);
    hazards_ = std::move(source.hazards_);
    abyss_environment_ = std::move(source.abyss_environment_);
    attack_ = std::move(source.attack_);
    active_skill_ = std::move(source.active_skill_);
    fire_crates_ = std::move(source.fire_crates_);
    input_buffer_ = std::move(source.input_buffer_);
    defeat_ledger_ = std::move(source.defeat_ledger_);
    while (events_.try_pop().has_value()) {}
    while (auto event = source.events_.try_pop()) {
        const bool published = events_.try_push(std::move(*event));
        assert(published);
        static_cast<void>(published);
    }
    tick_ = source.tick_;
    event_overflow_count_ = source.event_overflow_count_;
    projectile_saturation_count_ = source.projectile_saturation_count_;
    projectile_invalid_owner_count_ = source.projectile_invalid_owner_count_;
    hazard_saturation_count_ = source.hazard_saturation_count_;
    hazard_invalid_owner_count_ = source.hazard_invalid_owner_count_;
    evasion_rng_ = std::move(source.evasion_rng_);
    player_damage_history_ = std::move(source.player_damage_history_);
    death_snapshot_ = std::move(source.death_snapshot_);
    fault_ = source.fault_;
}

void CombatWorld::align_checkpoint_restore_authority_from(
    const CombatWorld& source) noexcept {
    encounter_config_ = source.encounter_config_;
    legacy_config_ = source.legacy_config_;
    legacy_mode_ = source.legacy_mode_;
    player_ = source.player_;
}

}  // namespace arpg::combat
