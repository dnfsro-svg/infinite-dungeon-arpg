#include "test_framework.hpp"

#include "combat_audio.hpp"
#include "combat_view_math.hpp"
#include "dungeon_view_math.hpp"

#include <array>
#include <cstring>

namespace {

using arpg::combat::HazardSnapshot;
using arpg::combat::MonsterAiPhase;
using arpg::combat::MonsterId;
using arpg::combat::MonsterSnapshot;
using arpg::combat::PlayerSnapshot;
using arpg::combat::ProjectileSnapshot;
using arpg::combat::Vec3;
using arpg::dungeon::DungeonElement;
using arpg::platform::HazardVisualMode;
using arpg::platform::MonsterWarningMode;
using arpg::platform::MonsterVisual;

bool same_color(arpg::platform::Rgba8 lhs, arpg::platform::Rgba8 rhs) noexcept {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

arpg::test::Failure all_monster_roles_have_unique_labels_and_ecology_accent() noexcept {
    constexpr std::array<MonsterId, 8> kIds{{
        MonsterId::fire_bomber,
        MonsterId::fire_charger,
        MonsterId::water_bulwark,
        MonsterId::water_support,
        MonsterId::lightning_shooter,
        MonsterId::lightning_dasher,
        MonsterId::chaos_chaser,
        MonsterId::chaos_hazard,
    }};
    std::array<const char*, kIds.size()> labels{};
    for (std::size_t index = 0; index < kIds.size(); ++index) {
        const MonsterVisual visual = arpg::platform::monster_visual(
            kIds[index], MonsterAiPhase::move, DungeonElement::lightning);
        ARPG_REQUIRE(visual.role_label != nullptr);
        ARPG_REQUIRE(same_color(visual.accent,
            arpg::platform::monster_ecology_color(DungeonElement::lightning)));
        labels[index] = visual.role_label;
        for (std::size_t prior = 0; prior < index; ++prior) {
            ARPG_REQUIRE(std::strcmp(labels[index], labels[prior]) != 0);
        }
    }
    return {};
}

arpg::test::Failure priority_phases_expose_warning_visuals() noexcept {
    constexpr std::array<MonsterId, 4> kPriorityIds{{
        MonsterId::fire_bomber,
        MonsterId::fire_charger,
        MonsterId::lightning_dasher,
        MonsterId::chaos_hazard,
    }};
    for (const MonsterId id : kPriorityIds) {
        ARPG_REQUIRE(arpg::platform::monster_visual(
            id, MonsterAiPhase::telegraph, DungeonElement::fire).warning_mode
            == MonsterWarningMode::telegraph);
        ARPG_REQUIRE(arpg::platform::monster_visual(
            id, MonsterAiPhase::active, DungeonElement::fire).warning_mode
            == MonsterWarningMode::active);
    }
    ARPG_REQUIRE(arpg::platform::monster_visual(
        MonsterId::water_bulwark, MonsterAiPhase::move,
        DungeonElement::water).warning_mode == MonsterWarningMode::none);
    return {};
}

arpg::test::Failure inactive_monsters_are_hidden_and_player_hp_is_clamped() noexcept {
    MonsterSnapshot monster{};
    ARPG_REQUIRE(!arpg::platform::monster_visible(monster));
    monster.active = true;
    ARPG_REQUIRE(arpg::platform::monster_visible(monster));
    monster.ai_phase = MonsterAiPhase::defeated;
    ARPG_REQUIRE(!arpg::platform::monster_visible(monster));

    PlayerSnapshot player{};
    player.max_hp = 100;
    player.hp = -10;
    ARPG_REQUIRE(arpg::test::near(arpg::platform::player_hp_ratio(player), 0.0, 1.0e-4));
    player.hp = 250;
    ARPG_REQUIRE(arpg::test::near(arpg::platform::player_hp_ratio(player), 1.0, 1.0e-4));
    player.max_hp = 0;
    ARPG_REQUIRE(arpg::test::near(arpg::platform::player_hp_ratio(player), 0.0, 1.0e-4));
    return {};
}

arpg::test::Failure hazard_modes_and_projected_effects_are_explicit() noexcept {
    HazardSnapshot hazard{};
    ARPG_REQUIRE(arpg::platform::hazard_visual_mode(hazard)
        == HazardVisualMode::hidden);
    hazard.active = true;
    hazard.center = Vec3{1.5F, -0.5F, 0.0F};
    hazard.telegraph_ticks = 12;
    ARPG_REQUIRE(arpg::platform::hazard_visual_mode(hazard)
        == HazardVisualMode::telegraph);
    hazard.telegraph_ticks = 0;
    hazard.active_ticks = 18;
    ARPG_REQUIRE(arpg::platform::hazard_visual_mode(hazard)
        == HazardVisualMode::active);

    ProjectileSnapshot projectile{};
    projectile.position = Vec3{2.0F, 1.0F, 0.5F};
    const auto projected_projectile = arpg::platform::project_projectile_position(
        projectile, 1280.0F, 720.0F);
    const auto direct = arpg::platform::project_combat_position(
        projectile.position, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(projected_projectile.x, direct.x, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(projected_projectile.y, direct.y, 1.0e-4));
    const auto projected_hazard = arpg::platform::project_hazard_center(
        hazard, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(projected_hazard.x,
        arpg::platform::project_combat_position(hazard.center, 1280.0F, 720.0F).x,
        1.0e-4));
    return {};
}

arpg::test::Failure monster_attack_audio_emits_one_low_layer() noexcept {
    arpg::combat::CombatEvent event{};
    event.kind = arpg::combat::CombatEventKind::player_hit;
    ARPG_REQUIRE(arpg::platform::route_audio_cues(event)
        == arpg::platform::audio_cue_mask(arpg::platform::AudioCue::low));
    event.kind = arpg::combat::CombatEventKind::player_hurt_started;
    ARPG_REQUIRE(arpg::platform::route_audio_cues(event) == 0);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"unique monster labels and ecology accent",
     &all_monster_roles_have_unique_labels_and_ecology_accent},
    {"priority monster warnings", &priority_phases_expose_warning_visuals},
    {"inactive slots and player HP clamp",
     &inactive_monsters_are_hidden_and_player_hp_is_clamped},
    {"hazard modes and effect projection",
     &hazard_modes_and_projected_effects_are_explicit},
    {"monster attack audio aggregation", &monster_attack_audio_emits_one_low_layer},
};

}  // namespace

arpg::test::TestSuite monster_view_suite() noexcept {
    return arpg::test::make_suite("monster_view", kCases);
}
