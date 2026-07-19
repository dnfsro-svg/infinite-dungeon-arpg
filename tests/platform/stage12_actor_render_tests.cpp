#include "test_framework.hpp"

#include "material_animation.hpp"

#include <array>

namespace {

using arpg::combat::MonsterAiPhase;
using arpg::combat::MonsterId;
using arpg::platform::MaterialSpriteId;

arpg::test::Failure stage12_monster_phases_use_distinct_material_frames() noexcept {
    constexpr std::array<MonsterId, 8> kMonsters{{
        MonsterId::fire_bomber, MonsterId::fire_charger,
        MonsterId::water_bulwark, MonsterId::water_support,
        MonsterId::lightning_shooter, MonsterId::lightning_dasher,
        MonsterId::chaos_chaser, MonsterId::chaos_hazard,
    }};
    constexpr std::array<MonsterAiPhase, 7> kPhases{{
        MonsterAiPhase::idle, MonsterAiPhase::move, MonsterAiPhase::telegraph,
        MonsterAiPhase::active, MonsterAiPhase::recovery,
        MonsterAiPhase::cooldown, MonsterAiPhase::defeated,
    }};
    for (const MonsterId monster : kMonsters) {
        const MaterialSpriteId idle = arpg::platform::select_monster_sprite(
            monster, MonsterAiPhase::idle);
        ARPG_REQUIRE(idle != MaterialSpriteId::missing);
        for (const MonsterAiPhase phase : kPhases) {
            const MaterialSpriteId selected = arpg::platform::select_monster_sprite(
                monster, phase);
            ARPG_REQUIRE(selected != MaterialSpriteId::missing);
            if (phase != MonsterAiPhase::idle) ARPG_REQUIRE(selected != idle);
        }
    }
    return {};
}

arpg::test::Failure stage12_player_attack_actions_have_distinct_material_frames() noexcept {
    using arpg::combat::AttackId;
    using arpg::combat::PlayerState;
    const MaterialSpriteId j1 = arpg::platform::select_player_sprite(
        PlayerState::attack_active, AttackId::j1);
    ARPG_REQUIRE(j1 != MaterialSpriteId::missing);
    ARPG_REQUIRE(j1 != arpg::platform::select_player_sprite(
        PlayerState::attack_active, AttackId::j2));
    ARPG_REQUIRE(j1 != arpg::platform::select_player_sprite(
        PlayerState::attack_active, AttackId::j3));
    ARPG_REQUIRE(j1 != arpg::platform::select_player_sprite(
        PlayerState::attack_active, AttackId::launcher));
    ARPG_REQUIRE(j1 != arpg::platform::select_player_sprite(
        PlayerState::attack_active, AttackId::air_j));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"maps every monster phase to a distinct material frame",
        &stage12_monster_phases_use_distinct_material_frames},
    {"maps player attack actions to distinct material frames",
        &stage12_player_attack_actions_have_distinct_material_frames},
};

}  // namespace

arpg::test::TestSuite stage12_actor_render_suite() noexcept {
    return arpg::test::make_suite("stage12_actor_render", kCases);
}
