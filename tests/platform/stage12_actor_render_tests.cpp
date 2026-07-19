#include "test_framework.hpp"

#include "material_animation.hpp"
#include "material_asset_validation.hpp"
#include "material_manifest.hpp"

#include <array>

namespace {

using arpg::combat::MonsterAiPhase;
using arpg::combat::MonsterId;
using arpg::platform::MaterialSpriteId;

const arpg::platform::MaterialFrameDefinition* find_manifest_frame(
    MaterialSpriteId id) noexcept {
    const arpg::platform::MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index = 0U; index < manifest.frame_count; ++index) {
        if (manifest.frames[index].id == id) return &manifest.frames[index];
    }
    return nullptr;
}

const arpg::platform::MaterialAtlasDefinition* find_manifest_atlas(
    arpg::platform::MaterialAtlasId id) noexcept {
    const arpg::platform::MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index = 0U; index < manifest.atlas_count; ++index) {
        if (manifest.atlases[index].id == id) return &manifest.atlases[index];
    }
    return nullptr;
}

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
        std::array<MaterialSpriteId, kPhases.size()> frames{};
        for (std::size_t phase_index = 0U; phase_index < kPhases.size(); ++phase_index) {
            const MonsterAiPhase phase = kPhases[phase_index];
            const MaterialSpriteId selected = arpg::platform::select_monster_sprite(
                monster, phase);
            ARPG_REQUIRE(selected != MaterialSpriteId::missing);
            const arpg::platform::MaterialFrameDefinition* frame =
                find_manifest_frame(selected);
            ARPG_REQUIRE(frame != nullptr);
            const arpg::platform::MaterialAtlasDefinition* atlas =
                find_manifest_atlas(frame->atlas);
            ARPG_REQUIRE(atlas != nullptr);
            ARPG_REQUIRE(arpg::platform::validate_material_frame(*atlas, *frame).valid);
            for (std::size_t prior = 0U; prior < phase_index; ++prior) {
                ARPG_REQUIRE(selected != frames[prior]);
            }
            frames[phase_index] = selected;
        }
    }
    return {};
}

arpg::test::Failure stage12_actor_material_scale_matches_existing_geometry() noexcept {
    ARPG_REQUIRE(arpg::platform::material_actor_draw_scale(true, 1.0F) == 0.36F);
    ARPG_REQUIRE(arpg::platform::material_actor_draw_scale(false, 1.0F) == 0.34F);
    ARPG_REQUIRE(arpg::platform::material_actor_draw_scale(true, 0.5F) == 0.18F);
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
    {"scales material actors to the existing geometry envelope",
        &stage12_actor_material_scale_matches_existing_geometry},
};

}  // namespace

arpg::test::TestSuite stage12_actor_render_suite() noexcept {
    return arpg::test::make_suite("stage12_actor_render", kCases);
}
