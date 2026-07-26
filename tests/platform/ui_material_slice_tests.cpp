#include "test_framework.hpp"

#include "material_manifest.hpp"
#include "ui_material.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

namespace platform = arpg::platform;

arpg::test::Failure every_ui_element_has_a_unique_material_frame() noexcept {
    const platform::MaterialManifestDefinition manifest =
        platform::default_material_manifest();
    std::array<platform::MaterialSpriteId,
        static_cast<std::size_t>(platform::UiMaterialElement::count)> sprites{};
    for (std::size_t index{}; index < sprites.size(); ++index) {
        const auto element = static_cast<platform::UiMaterialElement>(index);
        const auto sprite = platform::ui_material_sprite(element);
        ARPG_REQUIRE(sprite != platform::MaterialSpriteId::missing);
        for (std::size_t previous{}; previous < index; ++previous) {
            ARPG_REQUIRE(sprite != sprites[previous]);
        }
        sprites[index] = sprite;
        const platform::MaterialFrameDefinition* frame = nullptr;
        for (std::size_t candidate{}; candidate < manifest.frame_count;
             ++candidate) {
            if (manifest.frames[candidate].id == sprite) {
                frame = &manifest.frames[candidate];
                break;
            }
        }
        ARPG_REQUIRE(frame != nullptr);
        ARPG_REQUIRE(frame->atlas == platform::MaterialAtlasId::ui_material);
        ARPG_REQUIRE(frame->material_class == platform::MaterialClass::ui);
        ARPG_REQUIRE(frame->source.width == 128.0F);
        ARPG_REQUIRE(frame->source.height == 128.0F);
        ARPG_REQUIRE(frame->foot_anchor.x > 0.0F);
        ARPG_REQUIRE(frame->foot_anchor.y > 0.0F);
        ARPG_REQUIRE(frame->foot_anchor.x < frame->source.width);
        ARPG_REQUIRE(frame->foot_anchor.y < frame->source.height);
        ARPG_REQUIRE(frame->perceptual_hash != 0U);
        for (std::size_t previous{}; previous < index; ++previous) {
            const auto* previous_frame = [&]() noexcept {
                for (std::size_t candidate{}; candidate < manifest.frame_count;
                     ++candidate) {
                    if (manifest.frames[candidate].id == sprites[previous]) {
                        return &manifest.frames[candidate];
                    }
                }
                return static_cast<const platform::MaterialFrameDefinition*>(nullptr);
            }();
            ARPG_REQUIRE(previous_frame != nullptr);
            ARPG_REQUIRE(previous_frame->source.x != frame->source.x
                || previous_frame->source.y != frame->source.y);
            ARPG_REQUIRE(previous_frame->perceptual_hash
                != frame->perceptual_hash);
        }
    }
    return {};
}

arpg::test::Failure semantic_states_use_distinct_authored_resources() noexcept {
    using E = platform::UiMaterialElement;
    constexpr std::array<std::array<E, 2U>, 12U> pairs{{
        {{E::hud_health_track, E::hud_health_fill}},
        {{E::hud_barrier_track, E::hud_barrier_fill}},
        {{E::hud_resource_track, E::hud_resource_fill}},
        {{E::hud_notice, E::hud_notice_abyss}},
        {{E::hud_skill_empty, E::hud_skill_ready}},
        {{E::hud_skill_ready, E::hud_skill_cooldown}},
        {{E::inventory_tab_idle, E::inventory_tab_active}},
        {{E::inventory_slot_idle, E::inventory_slot_selected}},
        {{E::inventory_button_idle, E::inventory_button_active}},
        {{E::inventory_button_active, E::inventory_button_disabled}},
        {{E::skill_slot_empty, E::skill_slot_ready}},
        {{E::pause_row_idle, E::pause_row_selected}},
    }};
    for (const auto& pair : pairs) {
        ARPG_REQUIRE(platform::ui_material_sprite(pair[0])
            != platform::ui_material_sprite(pair[1]));
    }
    ARPG_REQUIRE(platform::ui_material_sprite(E::hud_status_slow)
        != platform::ui_material_sprite(E::hud_status_corrosion));
    ARPG_REQUIRE(platform::ui_material_sprite(E::hud_status_corrosion)
        != platform::ui_material_sprite(E::hud_status_invulnerable));
    ARPG_REQUIRE(platform::ui_material_decoded_bytes() == 8U * 1024U * 1024U);
    return {};
}

arpg::test::Failure label_plate_is_used_by_runtime_hud() noexcept {
    const std::filesystem::path source = std::filesystem::path{
        ARPG_PROJECT_SOURCE_DIR} / "src/platform/raylib/hud_renderer.cpp";
    std::ifstream input(source);
    const std::string contents((std::istreambuf_iterator<char>(input)), {});
    ARPG_REQUIRE(input.good() || input.eof());
    ARPG_REQUIRE(contents.find("UiMaterialElement::label_plate")
        != std::string::npos);
    ARPG_REQUIRE(contents.find("draw_region_fit") != std::string::npos);
    const std::filesystem::path inventory_source = std::filesystem::path{
        ARPG_PROJECT_SOURCE_DIR} / "src/platform/raylib/inventory_renderer.cpp";
    std::ifstream inventory_input(inventory_source);
    const std::string inventory_contents(
        (std::istreambuf_iterator<char>(inventory_input)), {});
    ARPG_REQUIRE(inventory_contents.find("UiMaterialElement::label_plate")
        != std::string::npos);
    ARPG_REQUIRE(inventory_contents.find("draw_region_fit")
        != std::string::npos);
    return {};
}

arpg::test::Failure death_overlay_uses_independent_material_fallbacks() noexcept {
    const std::filesystem::path renderer_source = std::filesystem::path{
        ARPG_PROJECT_SOURCE_DIR}
        / "src/platform/raylib/death_overlay_renderer.cpp";
    std::ifstream renderer_input(renderer_source);
    const std::string renderer(
        (std::istreambuf_iterator<char>(renderer_input)), {});
    ARPG_REQUIRE(renderer_input.good() || renderer_input.eof());

    const std::size_t plan = renderer.find("death_overlay_material_plan(");
    const std::size_t hidden_guard = renderer.find("if (!plan.visible) return;");
    const std::size_t panel = renderer.find("material_pack.draw_nine_slice(");
    const std::size_t title = renderer.find("material_pack.draw_region_fit(");
    ARPG_REQUIRE(hidden_guard != std::string::npos);
    ARPG_REQUIRE(plan != std::string::npos);
    ARPG_REQUIRE(panel != std::string::npos);
    ARPG_REQUIRE(title != std::string::npos);
    ARPG_REQUIRE(plan < hidden_guard);
    ARPG_REQUIRE(hidden_guard < panel);
    ARPG_REQUIRE(renderer.find("plan.panel_border_pixels", panel)
        != std::string::npos);
    ARPG_REQUIRE(renderer.find("plan.title_plate", title)
        != std::string::npos);
    ARPG_REQUIRE(renderer.find("if (!material_pack.draw_nine_slice(")
        != std::string::npos);
    ARPG_REQUIRE(renderer.find("if (!material_pack.draw_region_fit(")
        != std::string::npos);

    const std::filesystem::path combat_source = std::filesystem::path{
        ARPG_PROJECT_SOURCE_DIR} / "src/platform/raylib/combat_renderer.cpp";
    std::ifstream combat_input(combat_source);
    const std::string combat(
        (std::istreambuf_iterator<char>(combat_input)), {});
    ARPG_REQUIRE(combat_input.good() || combat_input.eof());
    ARPG_REQUIRE(combat.find("death_overlay_.draw(current, material_pack_)")
        != std::string::npos);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"every UI element has a unique material frame",
        &every_ui_element_has_a_unique_material_frame},
    {"semantic UI states use authored resources",
        &semantic_states_use_distinct_authored_resources},
    {"label plate is used by runtime HUD", &label_plate_is_used_by_runtime_hud},
    {"death overlay material fallbacks are independent",
        &death_overlay_uses_independent_material_fallbacks},
};

}  // namespace

arpg::test::TestSuite ui_material_slice_suite() noexcept {
    return arpg::test::make_suite("ui_material_slice", kCases);
}
