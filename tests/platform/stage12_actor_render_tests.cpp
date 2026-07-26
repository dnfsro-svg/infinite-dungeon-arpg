#include "test_framework.hpp"

#include "material_animation.hpp"
#include "material_asset_validation.hpp"
#include "material_manifest.hpp"
#include "material_residency.hpp"
#include "monster_material_presenter.hpp"

#include <raylib.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

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
    constexpr float kPlayerTrimmedHeight = 172.0F;
    constexpr float kPlayerTargetHeight = 82.0F;
    constexpr float kMonsterTrimmedHeight = 176.0F;
    constexpr float kMonsterTargetHeight = 78.0F;
    const float player_scale = arpg::platform::material_actor_draw_scale(true, 1.0F);
    const float monster_scale = arpg::platform::material_actor_draw_scale(false, 1.0F);
    ARPG_REQUIRE(player_scale * kPlayerTrimmedHeight >= kPlayerTargetHeight - 0.1F);
    ARPG_REQUIRE(player_scale * kPlayerTrimmedHeight <= kPlayerTargetHeight + 0.1F);
    ARPG_REQUIRE(monster_scale * kMonsterTrimmedHeight >= kMonsterTargetHeight - 0.1F);
    ARPG_REQUIRE(monster_scale * kMonsterTrimmedHeight <= kMonsterTargetHeight + 0.1F);
    ARPG_REQUIRE(arpg::platform::material_actor_draw_scale(true, 0.5F)
        == player_scale * 0.5F);
    const float bomber_scale = arpg::platform::monster_material_draw_scale(
        arpg::platform::MaterialAtlasId::fire_bomber, 1.0F);
    const float charger_scale = arpg::platform::monster_material_draw_scale(
        arpg::platform::MaterialAtlasId::fire_charger, 1.0F);
    ARPG_REQUIRE(arpg::test::near(
        bomber_scale * kMonsterTrimmedHeight, kMonsterTargetHeight, 0.1F));
    ARPG_REQUIRE(arpg::test::near(charger_scale, bomber_scale));
    for (const auto atlas : {arpg::platform::MaterialAtlasId::water_bulwark,
             arpg::platform::MaterialAtlasId::water_support,
             arpg::platform::MaterialAtlasId::lightning_shooter,
             arpg::platform::MaterialAtlasId::lightning_dasher,
             arpg::platform::MaterialAtlasId::chaos_chaser,
             arpg::platform::MaterialAtlasId::chaos_hazard}) {
        ARPG_REQUIRE(arpg::test::near(
            arpg::platform::monster_material_draw_scale(atlas, 1.0F), 0.92F));
    }
    return {};
}

std::uint64_t sprite_pixel_hash(const Color* pixels, int image_width,
    const arpg::platform::MaterialFrameDefinition& frame) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    const int left = static_cast<int>(frame.source.x);
    const int top = static_cast<int>(frame.source.y);
    const int right = left + static_cast<int>(frame.source.width);
    const int bottom = top + static_cast<int>(frame.source.height);
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const Color pixel = pixels[y * image_width + x];
            hash ^= pixel.r; hash *= 1099511628211ULL;
            hash ^= pixel.g; hash *= 1099511628211ULL;
            hash ^= pixel.b; hash *= 1099511628211ULL;
            hash ^= pixel.a; hash *= 1099511628211ULL;
        }
    }
    return hash;
}

bool frame_has_opaque_pixel(const Color* pixels, int image_width,
    const arpg::platform::MaterialFrameDefinition& frame) noexcept {
    const int left = static_cast<int>(frame.source.x);
    const int top = static_cast<int>(frame.source.y);
    const int right = left + static_cast<int>(frame.source.width);
    const int bottom = top + static_cast<int>(frame.source.height);
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            if (pixels[y * image_width + x].a > 0U) return true;
        }
    }
    return false;
}

arpg::test::Failure stage12_all_actor_manifest_frames_have_exported_content() noexcept {
    const Image image = LoadImage(ARPG_PROJECT_SOURCE_DIR "/assets/stage12/actors.png");
    ARPG_REQUIRE(image.data != nullptr);
    Color* const pixels = LoadImageColors(image);
    ARPG_REQUIRE(pixels != nullptr);
    const arpg::platform::MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index = 0U; index < manifest.frame_count; ++index) {
        const arpg::platform::MaterialFrameDefinition& frame = manifest.frames[index];
        if (frame.atlas != arpg::platform::MaterialAtlasId::actors) continue;
        ARPG_REQUIRE(frame_has_opaque_pixel(pixels, image.width, frame));
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    return {};
}

arpg::test::Failure stage12_monster_phase_frames_have_unique_exported_pixels() noexcept {
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
    const Image image = LoadImage(ARPG_PROJECT_SOURCE_DIR "/assets/stage12/actors.png");
    ARPG_REQUIRE(image.data != nullptr);
    Color* const pixels = LoadImageColors(image);
    ARPG_REQUIRE(pixels != nullptr);
    for (const MonsterId monster : kMonsters) {
        std::array<std::uint64_t, kPhases.size()> hashes{};
        for (std::size_t index = 0U; index < kPhases.size(); ++index) {
            const arpg::platform::MaterialFrameDefinition* frame = find_manifest_frame(
                arpg::platform::select_monster_sprite(monster, kPhases[index]));
            ARPG_REQUIRE(frame != nullptr);
            hashes[index] = sprite_pixel_hash(pixels, image.width, *frame);
            for (std::size_t prior = 0U; prior < index; ++prior) {
                ARPG_REQUIRE(hashes[index] != hashes[prior]);
            }
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
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

arpg::test::Failure active_monsters_produce_phase_aware_material_draw_plans() noexcept {
    arpg::platform::MonsterMaterialPresenter presenter{};
    arpg::combat::MonsterSnapshot monster{};
    monster.active = true;
    monster.generation = 41U;
    monster.id = MonsterId::chaos_chaser;
    monster.ai_phase = MonsterAiPhase::active;
    arpg::dungeon::DungeonSnapshot fire_snapshot{};
    fire_snapshot.has_active_room = true;
    fire_snapshot.ecology = arpg::dungeon::DungeonElement::fire;
    fire_snapshot.combat.emplace();
    fire_snapshot.combat->monster_count = 1U;
    fire_snapshot.combat->monsters[0].active = false;
    fire_snapshot.combat->monsters[0].id = MonsterId::fire_bomber;
    const std::size_t sparse_slot = fire_snapshot.combat->monsters.size() - 1U;
    fire_snapshot.combat->monsters[sparse_slot] = monster;
    const auto residency = arpg::platform::make_material_residency_request(
        fire_snapshot);
    ARPG_REQUIRE(residency.contains(arpg::platform::MaterialAtlasId::fire_environment));
    ARPG_REQUIRE(residency.contains(arpg::platform::MaterialAtlasId::chaos_chaser));
    const auto chaos = presenter.collect_draw_plan(
        sparse_slot, monster, 100U, false);
    ARPG_REQUIRE(chaos.visible);
    ARPG_REQUIRE(chaos.use_material_frame);
    ARPG_REQUIRE(chaos.animation_state
        == arpg::platform::MonsterAnimationState::active);
    ARPG_REQUIRE(chaos.frame.has_value());
    ARPG_REQUIRE(arpg::platform::select_monster_render_path(
        chaos.use_material_frame, true, false)
        == arpg::platform::MonsterRenderPath::material);
    ARPG_REQUIRE(arpg::platform::select_monster_render_path(
        chaos.use_material_frame, false, false)
        == arpg::platform::MonsterRenderPath::silhouette);

    monster.generation = 42U;
    monster.id = MonsterId::fire_bomber;
    monster.ai_phase = MonsterAiPhase::telegraph;
    const auto telegraph = presenter.collect_draw_plan(0U, monster, 200U, false);
    ARPG_REQUIRE(telegraph.use_material_frame);
    ARPG_REQUIRE(telegraph.frame_index == 0U);
    monster.ai_phase = MonsterAiPhase::active;
    const auto active = presenter.collect_draw_plan(0U, monster, 206U, false);
    ARPG_REQUIRE(active.use_material_frame);
    ARPG_REQUIRE(active.animation_state
        == arpg::platform::MonsterAnimationState::active);
    ARPG_REQUIRE(active.frame_index == 0U);
    return {};
}

arpg::test::Failure combat_text_uses_crisp_shared_font_path() noexcept {
    const std::filesystem::path actor_source = std::filesystem::path{
        ARPG_PROJECT_SOURCE_DIR} / "src/platform/raylib/actor_renderer.cpp";
    std::ifstream actor_input(actor_source);
    const std::string actor((std::istreambuf_iterator<char>(actor_input)), {});
    ARPG_REQUIRE(actor_input.good() || actor_input.eof());
    const std::size_t begin = actor.find("void draw_effects(");
    const std::size_t end = actor.find("void draw_hazards(", begin);
    ARPG_REQUIRE(begin != std::string::npos);
    ARPG_REQUIRE(end != std::string::npos);
    const std::string effects = actor.substr(begin, end - begin);
    ARPG_REQUIRE(effects.find("Font hud_font") != std::string::npos);
    ARPG_REQUIRE(effects.find("bool hud_font_ready") != std::string::npos);
    ARPG_REQUIRE(effects.find("MeasureTextEx(") != std::string::npos);
    ARPG_REQUIRE(effects.find("draw_crisp_ui_text(") != std::string::npos);
    ARPG_REQUIRE(effects.find("GetFontDefault()") != std::string::npos);
    ARPG_REQUIRE(effects.find("kCombatTextShadowPixels = 1")
        != std::string::npos);
    ARPG_REQUIRE(effects.find("DrawText(") == std::string::npos);
    ARPG_REQUIRE(effects.find("MeasureText(") == std::string::npos);
    ARPG_REQUIRE(effects.find("DrawTextGradient") == std::string::npos);

    const std::filesystem::path crisp_header = std::filesystem::path{
        ARPG_PROJECT_SOURCE_DIR} / "src/platform/raylib/ui_text_renderer.hpp";
    std::ifstream crisp_input(crisp_header);
    const std::string crisp((std::istreambuf_iterator<char>(crisp_input)), {});
    ARPG_REQUIRE(crisp_input.good() || crisp_input.eof());
    ARPG_REQUIRE(crisp.find("int shadow_pixels = 2") != std::string::npos);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"maps every monster phase to a distinct material frame",
        &stage12_monster_phases_use_distinct_material_frames},
    {"maps player attack actions to distinct material frames",
        &stage12_player_attack_actions_have_distinct_material_frames},
    {"scales material actors to the existing geometry envelope",
        &stage12_actor_material_scale_matches_existing_geometry},
    {"exports distinct pixels for every monster phase frame",
        &stage12_monster_phase_frames_have_unique_exported_pixels},
    {"exports content for every actor manifest frame",
        &stage12_all_actor_manifest_frames_have_exported_content},
    {"active monsters produce phase-aware material draw plans",
        &active_monsters_produce_phase_aware_material_draw_plans},
    {"combat text uses crisp shared HUD font",
        &combat_text_uses_crisp_shared_font_path},
};

}  // namespace

arpg::test::TestSuite stage12_actor_render_suite() noexcept {
    return arpg::test::make_suite("stage12_actor_render", kCases);
}
