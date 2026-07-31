#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "combat/combat_world.hpp"
#include "combat_test_support.hpp"
#include "dungeon/death_checkpoint.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/health_potion_loot.hpp"
#include "dungeon/material_loot.hpp"
#include "dungeon/room_generation.hpp"
#include "dungeon/room_progress_checkpoint.hpp"
#include "modifiers/effect_set.hpp"
#include "persistence/room_progress_codec.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>
#include <string_view>
#include <utility>

namespace {

using namespace arpg;

constexpr std::uint64_t kRootSeed = 0x5649574952453039ULL;

items::ItemInstance normal_item(const std::uint64_t id) noexcept {
    items::ItemInstance item{};
    item.id = id;
    item.base_id = 1U;
    item.rarity = items::ItemRarity::normal;
    item.item_level = 1U;
    item.required_level = 1U;
    return item;
}

void set_first_defeated_after_live(
    dungeon::checkpoint::RoomProgressCheckpoint& room,
    const std::uint32_t count) noexcept {
    for (std::uint32_t ordinal = 3U; ordinal < 3U + count; ++ordinal) {
        room.defeat_bits[ordinal / 64U] |=
            std::uint64_t{1U} << (ordinal % 64U);
    }
}

bool add_effect(combat::RoomCombatCheckpoint& combat_state) noexcept {
    if (combat_state.monster_count == 0U) return false;
    modifiers::EffectSet effects{};
    modifiers::EffectDefinition definition{};
    definition.id = 17U;
    definition.duration_ticks = 23;
    definition.refresh_rule = modifiers::RefreshRule::add_stack;
    definition.max_stacks = 2U;
    definition.strength = 42;
    definition.has_modifier = true;
    definition.modifier.id = 91U;
    definition.modifier.stat = modifiers::StatId::shield;
    definition.on_apply = {modifiers::EffectCommandKind::set_shield, 42};
    if (effects.apply(definition) != modifiers::ApplyResult::applied) {
        return false;
    }
    effects.capture_checkpoint(combat_state.monsters[0U].effects);
    combat_state.monsters[0U].effects_touched = true;
    return true;
}

bool add_ordinary_ground(
    dungeon::checkpoint::RoomProgressCheckpoint& room) noexcept {
    room.equipment_ground_count = 1U;
    auto& equipment = room.equipment_ground[0U];
    equipment.ordinal = 7U;
    equipment.source = static_cast<std::uint8_t>(
        dungeon::GroundItemSource::monster_drop);
    equipment.reward_ordinal = 0xFFU;
    equipment.position = {7.25F, 8.5F, 0.0F};
    equipment.item = normal_item(0x7001U);

    room.secondary_ground_count = 2U;
    auto& material = room.secondary_ground[0U];
    material.tag = dungeon::checkpoint::SecondaryGroundTag::material;
    material.ordinal = dungeon::checkpoint_material_ordinal(2U);
    material.source = static_cast<std::uint8_t>(
        dungeon::GroundMaterialSource::monster_common);
    material.position = {9.0F, 10.0F, 0.0F};
    material.material = items::MaterialId::reinforcement_stone;

    auto& potion = room.secondary_ground[1U];
    potion.tag = dungeon::checkpoint::SecondaryGroundTag::health_potion;
    potion.ordinal = dungeon::health_potion_claim_ordinal(3U);
    potion.source = 0U;
    potion.position = {11.0F, 12.0F, 0.0F};
    potion.material = items::MaterialId::count;
    return material.ordinal < potion.ordinal;
}

bool make_active_base(
    dungeon::checkpoint::SaveCheckpointSlot& slot,
    const bool active_attack,
    const bool include_effect,
    const bool include_ground) noexcept {
    dungeon::checkpoint::clear_save_checkpoint_slot(slot);
    slot.persistence_revision = 19U;
    slot.state.root_seed = kRootSeed;
    slot.state.commit_generation = 7U;
    slot.state.current_room.index = 41U;
    slot.state.current_room.seed = 43U;
    slot.state.current_room.depth = 2U;
    slot.state.current_room.floor_room_index = 3U;

    auto& room = slot.room_progress;
    room.lifecycle = dungeon::checkpoint::RoomProgressLifecycle::active;
    room.room_index = slot.state.current_room.index;
    room.room_seed = slot.state.current_room.seed;
    room.monster_generator_version = 1U;
    room.monster_blueprint_hash = 0xA11CE1125ULL;
    room.environment_generator_version = 1U;
    room.environment_blueprint_hash = 0xE1170001ULL;
    room.generated_monsters = 1125U;
    room.defeated_monsters = 282U;
    room.required_kills = dungeon::required_kills(room.generated_monsters);
    room.exits_unlocked = true;
    set_first_defeated_after_live(room, room.defeated_monsters);

    std::unique_ptr<combat::CombatWorld> world{
        new (std::nothrow) combat::CombatWorld{}};
    if (world == nullptr) return false;
    if (active_attack) {
        if (!world->queue_action(combat::Action::light)) return false;
        world->tick({});
    }
    if (!world->capture_room_checkpoint(room.combat)) return false;
    const auto valid = [&]() noexcept {
        return dungeon::checkpoint::valid_room_progress_checkpoint_structural(
            room, slot.state);
    };
    if (!valid()) {
        std::cerr << "active base invalid after combat capture\n";
        return false;
    }
    if (include_effect) {
        if (!add_effect(room.combat)) return false;
        if (!valid()) {
            std::cerr << "active base invalid after effect\n";
            return false;
        }
    }
    if (include_ground) {
        if (!add_ordinary_ground(room)) return false;
        if (!valid()) {
            std::cerr << "active base invalid after ground\n";
            return false;
        }
    }
    return true;
}

bool make_canonical_none(
    dungeon::checkpoint::SaveCheckpointSlot& slot) noexcept {
    dungeon::checkpoint::clear_save_checkpoint_slot(slot);
    const auto initial = dungeon::make_initial_run_state(
        kRootSeed, dungeon::DungeonRules{});
    if (initial.fault != dungeon::DungeonFault::none) return false;
    slot.state = initial.state;
    slot.persistence_revision = 11U;
    return dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        slot.room_progress, slot.state);
}

bool configure_abyss_room(
    dungeon::checkpoint::SaveCheckpointSlot& slot,
    const abyss::AbyssLifecycle lifecycle) noexcept {
    std::uint64_t room_seed = 1U;
    while (room_seed < 100000U && !abyss::is_abyss_roll(room_seed)) {
        ++room_seed;
    }
    if (room_seed == 100000U) return false;
    constexpr std::uint64_t kDepth = 40U;
    const auto selection = abyss::select_abyss_rule(room_seed, kDepth);
    if (!selection.has_value()) return false;

    auto& state = slot.state;
    state.current_room.seed = room_seed;
    state.current_room.depth = kDepth;
    state.current_room.entry = dungeon::checkpoint::EntrySide::left;
    state.current_room.ecology = dungeon::checkpoint::DungeonElement::water;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = true;
    state.last_transition = dungeon::checkpoint::TransitionKind::door;
    state.last_direction = dungeon::checkpoint::ExitDirection::right;
    state.abyss.lifecycle = lifecycle;
    state.abyss.danger = selection->danger;
    state.abyss.rule = selection->rule;
    state.abyss.rules_version = selection->rules_version;

    slot.room_progress.room_seed = room_seed;
    slot.room_progress.combat.abyss_environment.rule = selection->rule;
    return true;
}

bool make_active_normal(
    dungeon::checkpoint::SaveCheckpointSlot& slot) noexcept {
    return make_active_base(slot, true, true, true);
}

bool make_started_abyss(
    dungeon::checkpoint::SaveCheckpointSlot& slot) noexcept {
    if (!make_active_base(slot, true, true, true)
            || !configure_abyss_room(slot, abyss::AbyssLifecycle::started)) {
        return false;
    }
    const bool valid =
        dungeon::checkpoint::valid_room_progress_checkpoint_structural(
            slot.room_progress, slot.state);
    if (!valid) std::cerr << "started abyss invalid after configuration\n";
    return valid;
}

bool make_death_pending(
    dungeon::checkpoint::SaveCheckpointSlot& slot) noexcept {
    if (!make_active_base(slot, false, false, false)) return false;
    auto& room = slot.room_progress;
    room.lifecycle = dungeon::checkpoint::RoomProgressLifecycle::death_pending;
    room.generated_monsters = 3U;
    room.defeated_monsters = 0U;
    room.required_kills = dungeon::required_kills(room.generated_monsters);
    room.exits_unlocked = false;
    room.defeat_bits = {};

    std::unique_ptr<combat::CombatWorld> world{
        new (std::nothrow) combat::CombatWorld{}};
    if (world == nullptr) return false;
    test::CombatWorldTestAccess::set_player_resources(*world, 20, 0);
    const combat::PlayerDamageSource source{
        combat::PlayerDamageSourceKind::ground_hazard,
        combat::MonsterId::chaos_hazard,
        static_cast<std::uint16_t>(combat::HazardKind::native)};
    test::CombatWorldTestAccess::apply_damage(*world,
        combat::DamagePacket{{200, 200, 200, 200, 200}},
        combat::DamageDelivery::ground_or_environment, source,
        {3.0F, 4.0F, 0.0F}, combat::FeedbackLevel::heavy);
    if (!world->player_defeated() || !world->death_snapshot().has_value()
            || !world->capture_room_checkpoint(room.combat)) {
        return false;
    }

    slot.state.death_sequence = 4U;
    const std::uint64_t next_death_sequence = 5U;
    const auto target = dungeon::make_death_retreat_target(
        slot.state, next_death_sequence, dungeon::DungeonRules{});
    if (target.fault != dungeon::DungeonFault::none) return false;
    slot.state.death = dungeon::make_death_checkpoint(
        *world->death_snapshot(), slot.state.current_room, target.room);
    return dungeon::checkpoint::valid_room_progress_checkpoint_structural(
        room, slot.state);
}

bool make_cleared_abyss_rewards(
    dungeon::checkpoint::SaveCheckpointSlot& slot) noexcept {
    if (!make_active_base(slot, false, false, false)
            || !configure_abyss_room(slot, abyss::AbyssLifecycle::cleared)) {
        return false;
    }
    auto& state = slot.state;
    auto& room = slot.room_progress;
    const std::uint8_t reward_total = abyss::reward_profile_for(
        state.abyss.danger, 1U).item_count;
    if (reward_total == 0U || reward_total > 3U) return false;
    state.abyss.reward_total = reward_total;
    state.abyss.generated_mask = 0x01U;
    state.abyss.reward_revision = 23U;

    room.generated_monsters = 3U;
    room.defeated_monsters = room.generated_monsters;
    room.required_kills = dungeon::required_kills(room.generated_monsters);
    room.exits_unlocked = true;
    room.full_clear = true;
    room.reward_committed = true;
    room.defeat_bits = {};
    room.defeat_bits[0U] = 0x07U;
    room.combat.monster_count = 0U;
    room.combat.abyss_environment = {};
    room.combat.abyss_environment.expansion_stage = 0xFFU;
    room.equipment_ground_count = 1U;
    auto& ground = room.equipment_ground[0U];
    ground.ordinal = 0U;
    ground.source = static_cast<std::uint8_t>(
        dungeon::GroundItemSource::abyss_chest);
    ground.reward_ordinal = 0U;
    ground.position = {15.0F, 16.0F, 0.0F};
    ground.item = normal_item(0xA000U);
    const bool valid =
        dungeon::checkpoint::valid_room_progress_checkpoint_structural(
            room, state);
    if (!valid) std::cerr << "cleared abyss invalid after rewards\n";
    return valid;
}

bool make_scenario(
    const std::string_view name,
    dungeon::checkpoint::SaveCheckpointSlot& slot) noexcept {
    if (name == "canonical_none") return make_canonical_none(slot);
    if (name == "active_normal") return make_active_normal(slot);
    if (name == "started_abyss") return make_started_abyss(slot);
    if (name == "death_pending") return make_death_pending(slot);
    if (name == "cleared_abyss_rewards") {
        return make_cleared_abyss_rewards(slot);
    }
    return false;
}

}  // namespace

int main(const int argc, char** argv) {
    if (argc != 3 || argv == nullptr || argv[1] == nullptr
            || argv[2] == nullptr) {
        std::cerr << "usage: arpg_checkpoint_v9_wire_baseline "
                     "<scenario> <output>\n";
        return 2;
    }

    std::unique_ptr<arpg::dungeon::checkpoint::SaveCheckpointSlot> slot{
        new (std::nothrow)
            arpg::dungeon::checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            arpg::persistence::kMaximumEncodedCheckpointBytes]};
    if (slot == nullptr || bytes == nullptr) {
        std::cerr << "allocation failure\n";
        return 3;
    }

    const std::string_view scenario{argv[1]};
    if (!make_scenario(scenario, *slot)) {
        std::cerr << "invalid or unknown scenario: " << scenario << '\n';
        return 4;
    }
    std::size_t written{};
    const auto error = arpg::persistence::encode_checkpoint_v9_into(
        *slot, bytes.get(), arpg::persistence::kMaximumEncodedCheckpointBytes,
        written);
    if (error != arpg::persistence::CodecError::none || written == 0U) {
        std::cerr << "encode failed for " << scenario
                  << ": " << static_cast<int>(error) << '\n';
        return 5;
    }

    std::ofstream output{argv[2], std::ios::binary | std::ios::trunc};
    if (!output) {
        std::cerr << "cannot open output for " << scenario << '\n';
        return 6;
    }
    output.write(reinterpret_cast<const char*>(bytes.get()),
        static_cast<std::streamsize>(written));
    output.close();
    if (!output) {
        std::cerr << "write failed for " << scenario << '\n';
        return 7;
    }
    return 0;
}
