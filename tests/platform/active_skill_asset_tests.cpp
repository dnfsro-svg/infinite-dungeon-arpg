#include "test_framework.hpp"

#include "active_skill_assets.hpp"
#include "active_skill_view.hpp"
#include "material_animation.hpp"
#include "material_manifest.hpp"
#include "skills/skill_loadout.hpp"

#include <cstddef>

namespace {

namespace platform = arpg::platform;

arpg::test::Failure active_skill_atlases_define_complete_original_frame_grids()
    noexcept {
    for (std::size_t frame = 0U; frame < 36U; ++frame) {
        const auto source = platform::active_skill_atlas_frame(
            arpg::skills::ActiveSkillId::draw_slash, frame);
        ARPG_REQUIRE(source.has_value());
        ARPG_REQUIRE(source->atlas == platform::MaterialAtlasId::skill_draw_slash);
        ARPG_REQUIRE(source->source.x >= 0.0F);
        ARPG_REQUIRE(source->source.y >= 0.0F);
        ARPG_REQUIRE(source->source.x + source->source.width <= 1254.0F);
        ARPG_REQUIRE(source->source.y + source->source.height <= 1254.0F);
        ARPG_REQUIRE(source->foot_anchor.x >= 0.0F);
        ARPG_REQUIRE(source->foot_anchor.x <= source->source.width);
        ARPG_REQUIRE(source->foot_anchor.y >= 0.0F);
        ARPG_REQUIRE(source->foot_anchor.y <= source->source.height);
    }
    for (std::size_t frame = 0U; frame < 24U; ++frame) {
        const auto source = platform::active_skill_atlas_frame(
            arpg::skills::ActiveSkillId::storm_swords, frame);
        ARPG_REQUIRE(source.has_value());
        ARPG_REQUIRE(source->atlas == platform::MaterialAtlasId::skill_storm_swords);
        ARPG_REQUIRE(source->source.x >= 0.0F);
        ARPG_REQUIRE(source->source.y >= 0.0F);
        ARPG_REQUIRE(source->source.x + source->source.width <= 1024.0F);
        ARPG_REQUIRE(source->source.y + source->source.height <= 1536.0F);
    }
    ARPG_REQUIRE(!platform::active_skill_atlas_frame(
        arpg::skills::ActiveSkillId::storm_swords, 24U).has_value());
    return {};
}

arpg::test::Failure active_skill_atlases_are_registered_as_material_pairs()
    noexcept {
    const auto manifest = platform::default_material_manifest();
    bool found_draw{};
    bool found_storm{};
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        if (atlas.id == platform::MaterialAtlasId::skill_draw_slash) {
            found_draw = atlas.width == 1254 && atlas.height == 1254
                && atlas.rgba_bytes == 1254U * 1254U * 4U
                && atlas.ecology == platform::MaterialEcology::skill;
        }
        if (atlas.id == platform::MaterialAtlasId::skill_storm_swords) {
            found_storm = atlas.width == 1024 && atlas.height == 1536
                && atlas.rgba_bytes == 1024U * 1536U * 4U
                && atlas.ecology == platform::MaterialEcology::skill;
        }
    }
    ARPG_REQUIRE(found_draw);
    ARPG_REQUIRE(found_storm);
    return {};
}

arpg::test::Failure active_skill_actor_scale_matches_base_player_geometry()
    noexcept {
    constexpr float kBasePlayerOpaqueHeight = 117.0F;
    constexpr float kDrawSlashActorOpaqueHeight = 139.0F;
    constexpr float kStormSwordsActorOpaqueHeight = 164.0F;
    const float base_height = platform::material_actor_draw_scale(true, 1.0F)
        * kBasePlayerOpaqueHeight;
    const float draw_slash_height = platform::active_skill_material_draw_scale(
        arpg::skills::ActiveSkillId::draw_slash, 1.0F)
        * kDrawSlashActorOpaqueHeight;
    const float storm_swords_height = platform::active_skill_material_draw_scale(
        arpg::skills::ActiveSkillId::storm_swords, 1.0F)
        * kStormSwordsActorOpaqueHeight;
    ARPG_REQUIRE(arpg::test::near(draw_slash_height, base_height, 0.1F));
    ARPG_REQUIRE(arpg::test::near(storm_swords_height, base_height, 0.1F));
    ARPG_REQUIRE(platform::active_skill_material_draw_scale(
        arpg::skills::ActiveSkillId::draw_slash, 0.5F)
        == platform::active_skill_material_draw_scale(
            arpg::skills::ActiveSkillId::draw_slash, 1.0F) * 0.5F);
    ARPG_REQUIRE(platform::active_skill_material_draw_scale(
        arpg::skills::ActiveSkillId::none, 1.0F) == 0.0F);
    constexpr float kProjectionScale = 1.25F;
    const float draw_effect_scale =
        platform::active_skill_material_effect_scale(
            arpg::skills::ActiveSkillId::draw_slash, kProjectionScale);
    const float storm_effect_scale =
        platform::active_skill_material_effect_scale(
            arpg::skills::ActiveSkillId::storm_swords, kProjectionScale);
    ARPG_REQUIRE(arpg::test::near(
        draw_effect_scale, 0.72F * kProjectionScale));
    ARPG_REQUIRE(arpg::test::near(
        storm_effect_scale, 0.70F * kProjectionScale));
    ARPG_REQUIRE(draw_effect_scale
        > platform::active_skill_material_draw_scale(
            arpg::skills::ActiveSkillId::draw_slash, kProjectionScale));
    ARPG_REQUIRE(storm_effect_scale
        > platform::active_skill_material_draw_scale(
            arpg::skills::ActiveSkillId::storm_swords, kProjectionScale));
    ARPG_REQUIRE(platform::active_skill_material_effect_scale(
        arpg::skills::ActiveSkillId::none, kProjectionScale) == 0.0F);
    ARPG_REQUIRE(platform::active_skill_material_effect_scale(
        arpg::skills::ActiveSkillId::draw_slash, 0.0F) == 0.0F);
    return {};
}

arpg::test::Failure active_skill_hud_uses_distinct_registered_material_icons()
    noexcept {
    const auto manifest = platform::default_material_manifest();
    const platform::MaterialSpriteId draw_icon =
        platform::active_skill_icon_sprite(
            arpg::skills::ActiveSkillId::draw_slash);
    const platform::MaterialSpriteId storm_icon =
        platform::active_skill_icon_sprite(
            arpg::skills::ActiveSkillId::storm_swords);
    ARPG_REQUIRE(draw_icon != platform::MaterialSpriteId::missing);
    ARPG_REQUIRE(storm_icon != platform::MaterialSpriteId::missing);
    ARPG_REQUIRE(draw_icon != storm_icon);
    ARPG_REQUIRE(platform::active_skill_icon_sprite(
        arpg::skills::ActiveSkillId::none)
        == platform::MaterialSpriteId::missing);

    const platform::MaterialFrameDefinition* const draw_frame =
        platform::find_material_frame(manifest, draw_icon);
    const platform::MaterialFrameDefinition* const storm_frame =
        platform::find_material_frame(manifest, storm_icon);
    ARPG_REQUIRE(draw_frame != nullptr);
    ARPG_REQUIRE(storm_frame != nullptr);
    ARPG_REQUIRE(draw_frame->atlas
        == platform::MaterialAtlasId::skill_draw_slash);
    ARPG_REQUIRE(draw_frame->source.x == 0.0F);
    ARPG_REQUIRE(draw_frame->source.y == 627.0F);
    ARPG_REQUIRE(draw_frame->source.width == 209.0F);
    ARPG_REQUIRE(draw_frame->source.height == 209.0F);
    ARPG_REQUIRE(storm_frame->atlas
        == platform::MaterialAtlasId::skill_storm_swords);
    ARPG_REQUIRE(storm_frame->source.x == 0.0F);
    ARPG_REQUIRE(storm_frame->source.y == 1280.0F);
    ARPG_REQUIRE(storm_frame->source.width == 256.0F);
    ARPG_REQUIRE(storm_frame->source.height == 256.0F);

    const platform::ActiveSkillHudModel hud =
        platform::make_active_skill_hud_model(
            arpg::skills::default_skill_loadout(), {});
    ARPG_REQUIRE(hud.slots[0U].icon == draw_icon);
    ARPG_REQUIRE(hud.slots[1U].icon == storm_icon);
    for (std::size_t index = 2U; index < hud.slots.size(); ++index) {
        ARPG_REQUIRE(hud.slots[index].icon
            == platform::MaterialSpriteId::missing);
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"active skill atlases expose complete original frame grids",
     &active_skill_atlases_define_complete_original_frame_grids},
    {"active skill atlases are registered as material pairs",
        &active_skill_atlases_are_registered_as_material_pairs},
    {"active skill actor scale matches the base player geometry",
        &active_skill_actor_scale_matches_base_player_geometry},
    {"active skill HUD uses distinct registered material icons",
     &active_skill_hud_uses_distinct_registered_material_icons},
};

}  // namespace

arpg::test::TestSuite active_skill_asset_suite() noexcept {
    return arpg::test::make_suite("active_skill_assets", kCases);
}
