#include "test_framework.hpp"

#include "material_animation.hpp"

#include <array>

namespace {

using arpg::combat::AttackId;
using arpg::combat::MonsterAiPhase;
using arpg::combat::MonsterId;
using arpg::combat::PlayerState;
using arpg::platform::MaterialSpriteId;

arpg::test::Failure material_animation_selects_launcher_active() noexcept {
    ARPG_REQUIRE(arpg::platform::select_player_sprite(PlayerState::attack_active,
        AttackId::launcher) == MaterialSpriteId::player_launcher);
    return {};
}

arpg::test::Failure material_animation_covers_all_player_states_and_attacks() noexcept {
    constexpr std::array<PlayerState, 8> kStates{{
        PlayerState::idle, PlayerState::move, PlayerState::attack_startup,
        PlayerState::attack_active, PlayerState::attack_recovery,
        PlayerState::jump_rise, PlayerState::jump_fall, PlayerState::landing,
    }};
    constexpr std::array<AttackId, 5> kAttacks{{
        AttackId::j1, AttackId::j2, AttackId::j3, AttackId::launcher,
        AttackId::air_j,
    }};
    for (const PlayerState state : kStates) {
        for (const AttackId attack : kAttacks) {
            ARPG_REQUIRE(arpg::platform::select_player_sprite(state, attack)
                != MaterialSpriteId::missing);
        }
    }
    ARPG_REQUIRE(arpg::platform::select_player_sprite(
        PlayerState::attack_active, AttackId::none) == MaterialSpriteId::player_idle);
    return {};
}

arpg::test::Failure material_animation_covers_monsters_and_ai_phases() noexcept {
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
        for (const MonsterAiPhase phase : kPhases) {
            ARPG_REQUIRE(arpg::platform::select_monster_sprite(monster, phase)
                != MaterialSpriteId::missing);
        }
    }
    ARPG_REQUIRE(arpg::platform::select_monster_sprite(
        MonsterId::count, MonsterAiPhase::idle) == MaterialSpriteId::missing);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"selects launcher active", &material_animation_selects_launcher_active},
    {"covers player states and attacks",
        &material_animation_covers_all_player_states_and_attacks},
    {"covers monsters and ai phases",
        &material_animation_covers_monsters_and_ai_phases},
};

}  // namespace

arpg::test::TestSuite material_animation_suite() noexcept {
    return arpg::test::make_suite("material_animation", kCases);
}
