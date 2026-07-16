#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_affix_generation.hpp"
#include "combat/monster_catalog.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/encounter_director.hpp"
#include "persistence/checkpoint_codec.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {

using arpg::combat::MonsterAffixId;
using arpg::combat::MonsterAffixSet;
using arpg::combat::MonsterAffixTier;

std::string_view affix_name(MonsterAffixId id) noexcept {
    const auto* const definition = arpg::combat::monster_affix_definition(id);
    return definition == nullptr ? "unknown" : definition->short_name;
}

const char* tier_name(MonsterAffixTier tier) noexcept {
    switch (tier) {
    case MonsterAffixTier::m1: return "M1";
    case MonsterAffixTier::m2: return "M2";
    case MonsterAffixTier::m3: return "M3";
    case MonsterAffixTier::count: return "invalid";
    }
    return "invalid";
}

bool print_plan(const char* name, std::uint64_t room_seed,
    std::uint64_t depth, arpg::dungeon::checkpoint::DungeonElement ecology,
    const arpg::dungeon::DungeonRules& rules) noexcept {
    const auto result = arpg::dungeon::build_encounter_plan(
        room_seed, depth, ecology, rules.encounter);
    if (result.fault != arpg::dungeon::DungeonFault::none) return false;
    std::cout << name << "_seed=" << room_seed << " depth=" << depth
              << " waves=" << static_cast<unsigned>(result.plan.wave_count)
              << " budget=" << static_cast<unsigned>(result.plan.total_budget)
              << '\n';
    for (std::uint8_t wave = 0U; wave < result.plan.wave_count; ++wave) {
        const auto& entries = result.plan.waves[wave];
        for (std::uint8_t index = 0U; index < entries.spawn_count; ++index) {
            const auto& spawn = entries.spawns[index];
            std::cout << "  wave=" << static_cast<unsigned>(wave)
                      << " spawn=" << static_cast<unsigned>(index)
                      << " monster=" << static_cast<unsigned>(spawn.id)
                      << " pos=" << spawn.position.x << ',' << spawn.position.y
                      << " affixes=";
            for (std::uint8_t affix = 0U; affix < spawn.affixes.count; ++affix) {
                if (affix != 0U) std::cout << ',';
                std::cout << affix_name(spawn.affixes.values[affix].id)
                          << '-' << tier_name(spawn.affixes.values[affix].tier);
            }
            std::cout << " score="
                      << arpg::combat::monster_affix_danger_score(spawn.affixes)
                      << '\n';
        }
    }
    return true;
}

MonsterAffixSet high_risk_sample() noexcept {
    MonsterAffixSet result{};
    result.values = {{
        {MonsterAffixId::multishot, MonsterAffixTier::m3},
        {MonsterAffixId::burning_ground, MonsterAffixTier::m3},
        {MonsterAffixId::chain_lightning, MonsterAffixTier::m3},
    }};
    result.count = 3U;
    return result;
}

}  // namespace

int main() {
    const arpg::dungeon::DungeonRules rules{};
    constexpr std::uint64_t kRootSeed = 0x9A0FF10900000001ULL;
    const auto initial = arpg::dungeon::make_initial_run_state(kRootSeed, rules);
    if (initial.fault != arpg::dungeon::DungeonFault::none) return 2;
    const auto encoded = arpg::persistence::encode_checkpoint(initial.state);
    if (!encoded.has_value()) return 3;
    const auto decoded = arpg::persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    if (decoded.error != arpg::persistence::CodecError::none
            || !arpg::dungeon::same_run_state(initial.state, decoded.state)) {
        return 4;
    }

    const auto& room = initial.state.current_room;
    if (!print_plan("shallow", room.seed, room.depth, room.ecology, rules)
            || !print_plan("deep40", room.seed, 40U, room.ecology, rules)) {
        return 5;
    }

    const MonsterAffixSet high_risk = high_risk_sample();
    std::cout << "high_risk=";
    for (std::uint8_t index = 0U; index < high_risk.count; ++index) {
        if (index != 0U) std::cout << ',';
        std::cout << affix_name(high_risk.values[index].id)
                  << '-' << tier_name(high_risk.values[index].tier);
    }
    const std::uint16_t score = arpg::combat::monster_affix_danger_score(high_risk);
    std::cout << " score=" << score << '\n'
              << "rewards score=" << score
              << " drop_bp=" << arpg::dungeon::affix_drop_chance_bp(score)
              << " item_level=" << static_cast<unsigned>(
                    arpg::dungeon::affix_item_level(40U, score))
              << " experience=" << arpg::dungeon::affix_experience(40U, score)
              << '\n'
              << "v4_reload_consistent=1\n";
    return 0;
}
