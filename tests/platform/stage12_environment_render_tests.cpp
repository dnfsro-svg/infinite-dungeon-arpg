#include "test_framework.hpp"

#include "environment_render_plan.hpp"
#include "material_pack.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string read_project_source(const char* relative_path) {
    const std::filesystem::path path =
        std::filesystem::path{ARPG_PROJECT_SOURCE_DIR} / relative_path;
    std::ifstream input{path};
    return {std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

std::string braced_block_after(const std::string& source,
    std::size_t marker) {
    const std::size_t opening = source.find('{', marker);
    if (opening == std::string::npos) return {};
    std::size_t depth{};
    for (std::size_t index = opening; index < source.size(); ++index) {
        if (source[index] == '{') {
            ++depth;
        } else if (source[index] == '}' && --depth == 0U) {
            return source.substr(opening, index - opening + 1U);
        }
    }
    return {};
}

std::size_t occurrence_count(const std::string& source,
    const char* token) noexcept {
    std::size_t count{};
    std::size_t position{};
    while ((position = source.find(token, position)) != std::string::npos) {
        ++count;
        position += std::char_traits<char>::length(token);
    }
    return count;
}

arpg::test::Failure environment_renderer_uses_independent_native_paths() noexcept {
    const std::string renderer = read_project_source(
        "src/platform/raylib/room_renderer.cpp");
    ARPG_REQUIRE(!renderer.empty());
    ARPG_REQUIRE(renderer.find("room_background_scale(width, height)")
        != std::string::npos);
    const std::size_t draw_environment = renderer.find(
        "bool draw_environment_room");
    ARPG_REQUIRE(draw_environment != std::string::npos);
    const std::string draw_environment_block = braced_block_after(
        renderer, draw_environment);
    ARPG_REQUIRE(draw_environment_block.find(
        "draw_frame(plan.atlas, plan.source") != std::string::npos);
    ARPG_REQUIRE(occurrence_count(draw_environment_block, "draw_frame(") == 1U);
    for (const char* legacy_atlas : {"MaterialAtlasId::environment",
             "MaterialAtlasId::fire_environment",
             "MaterialAtlasId::water_environment",
             "MaterialAtlasId::lightning_environment",
             "MaterialAtlasId::chaos_environment"}) {
        ARPG_REQUIRE(draw_environment_block.find(legacy_atlas)
            == std::string::npos);
    }
    for (const char* legacy_source : {
             "{0.0F, 0.0F, 256.0F, 256.0F}",
             "{0.0F, 0.0F, 512.0F, 512.0F}"}) {
        ARPG_REQUIRE(draw_environment_block.find(legacy_source)
            == std::string::npos);
    }
    for (const char* legacy : {"water_room_render_plan(",
             "lightning_room_render_plan(", "chaos_room_render_plan(",
             "select_floor_sprite("}) {
        ARPG_REQUIRE(renderer.find(legacy) == std::string::npos);
    }

    const std::size_t draw_room = renderer.find(
        "void CombatRenderer::draw_room");
    ARPG_REQUIRE(draw_room != std::string::npos);
    const std::string draw_room_block = braced_block_after(renderer, draw_room);
    ARPG_REQUIRE(draw_room_block.find("can_draw_room_environment(current,")
        == std::string::npos);
    ARPG_REQUIRE(draw_room_block.find(
        "&& draw_environment_room(material_pack_, current.ecology)")
        != std::string::npos);
    const std::size_t graybox_gate = draw_room_block.find(
        "if (!draw_material_background)");
    ARPG_REQUIRE(graybox_gate != std::string::npos);
    ARPG_REQUIRE(braced_block_after(draw_room_block, graybox_gate).find(
        "draw_graybox_room") != std::string::npos);
    const std::size_t doors = draw_room_block.find("draw_doors(");
    ARPG_REQUIRE(doors != std::string::npos);
    const std::size_t door_call_end = draw_room_block.find(");", doors);
    ARPG_REQUIRE(door_call_end != std::string::npos);
    const std::string door_call = draw_room_block.substr(doors,
        door_call_end - doors + 2U);
    ARPG_REQUIRE(door_call.find(
        "draw_material_background") == std::string::npos);
    ARPG_REQUIRE(draw_room_block.find(
        "draw_hole(current, material_pack_)")
        != std::string::npos);
    ARPG_REQUIRE(occurrence_count(draw_room_block,
        "draw_fire_room_props(") == 1U);
    ARPG_REQUIRE(occurrence_count(draw_room_block,
        "draw_environment_room_props(") == 1U);
    for (const char* old_loop : {"draw_water_room_props(",
             "draw_lightning_room_props(", "draw_chaos_room_props("}) {
        ARPG_REQUIRE(renderer.find(old_loop) == std::string::npos);
    }
    const std::size_t shared_props = renderer.find(
        "void draw_environment_room_props");
    ARPG_REQUIRE(shared_props != std::string::npos);
    const std::string shared_props_block = braced_block_after(
        renderer, shared_props);
    ARPG_REQUIRE(occurrence_count(shared_props_block, "for (") == 1U);
    ARPG_REQUIRE(shared_props_block.find(
        "environment_prop_layout(") != std::string::npos);
    ARPG_REQUIRE(shared_props_block.find(
        "DrawRectangleLinesEx(bounds, 2.0F, Color{35, 48, 62, 72})")
        != std::string::npos);
    const std::size_t draw_hole = renderer.find("void draw_hole");
    ARPG_REQUIRE(draw_hole != std::string::npos);
    const std::string draw_hole_block = braced_block_after(renderer, draw_hole);
    ARPG_REQUIRE(draw_hole_block.find(
        "if (!material_pack.draw(hole_sprite(snapshot.ecology)")
        != std::string::npos);
    ARPG_REQUIRE(draw_hole_block.find("DrawEllipse(x, y")
        != std::string::npos);

    constexpr std::array<const char*, 8> legacy_plan_sources{{
        "src/platform/raylib/material_animation.hpp",
        "src/platform/raylib/material_animation.cpp",
        "src/platform/raylib/water_room_material_slice.hpp",
        "src/platform/raylib/water_room_material_slice.cpp",
        "src/platform/raylib/lightning_room_material_slice.hpp",
        "src/platform/raylib/lightning_room_material_slice.cpp",
        "src/platform/raylib/chaos_room_material_slice.hpp",
        "src/platform/raylib/chaos_room_material_slice.cpp",
    }};
    for (const char* relative_path : legacy_plan_sources) {
        const std::string source = read_project_source(relative_path);
        ARPG_REQUIRE(!source.empty());
        ARPG_REQUIRE(source.find("select_floor_sprite") == std::string::npos);
        ARPG_REQUIRE(source.find("RoomRenderPlan") == std::string::npos);
        ARPG_REQUIRE(source.find("_room_render_plan") == std::string::npos);
    }
    for (const char* runtime_source : {
             "src/platform/raylib/environment_prop_layout.cpp",
             "src/platform/raylib/water_room_material_slice.cpp",
             "src/platform/raylib/lightning_room_material_slice.cpp",
             "src/platform/raylib/chaos_room_material_slice.cpp",
             "src/platform/raylib/room_renderer.cpp"}) {
        const std::string source = read_project_source(runtime_source);
        ARPG_REQUIRE(!source.empty());
        ARPG_REQUIRE(source.find(".json") == std::string::npos);
    }
    return {};
}

arpg::test::Failure directional_door_atlas_is_drawable_without_gating_background() noexcept {
    arpg::platform::MaterialPackState doors_only{};
    doors_only.set_available(arpg::platform::MaterialAtlasId::element_doors,
        true);
    for (const arpg::platform::MaterialSpriteId sprite : {
             arpg::platform::MaterialSpriteId::environment_door_fire,
             arpg::platform::MaterialSpriteId::environment_door_water,
             arpg::platform::MaterialSpriteId::environment_door_lightning,
             arpg::platform::MaterialSpriteId::environment_door_chaos}) {
        ARPG_REQUIRE(doors_only.can_draw(sprite));
    }

    const std::string renderer = read_project_source(
        "src/platform/raylib/room_renderer.cpp");
    const std::size_t can_draw = renderer.find("bool can_draw_room_environment");
    const std::size_t draw_doors = renderer.find("void draw_doors");
    ARPG_REQUIRE(can_draw == std::string::npos);
    ARPG_REQUIRE(draw_doors != std::string::npos);
    const std::string doors = braced_block_after(renderer, draw_doors);
    ARPG_REQUIRE(doors.find("material_pack.draw(visual.sprite") != std::string::npos);
    ARPG_REQUIRE(doors.find("DrawRectangleRec") == std::string::npos);
    ARPG_REQUIRE(doors.find("visual.arrow") == std::string::npos);
    ARPG_REQUIRE(doors.find("MeasureText(visual.arrow") == std::string::npos);
    for (const char* segment : {"DrawLineEx(arrow.tail, arrow.tip",
             "DrawLineEx(arrow.tip, arrow.head_left",
             "DrawLineEx(arrow.tip, arrow.head_right"}) {
        ARPG_REQUIRE(doors.find(segment) != std::string::npos);
    }
    return {};
}

arpg::test::Failure environment_falls_back_atomically_when_a_required_frame_is_missing() noexcept {
    using arpg::platform::EnvironmentFrameAvailability;

    EnvironmentFrameAvailability available{};
    available.background = true;
    available.door = true;
    available.hole = true;
    available.props = true;
    available.door_required = true;
    available.hole_required = true;
    ARPG_REQUIRE(arpg::platform::should_draw_material_environment(available));

    EnvironmentFrameAvailability missing = available;
    missing.door = false;
    ARPG_REQUIRE(!arpg::platform::should_draw_material_environment(
        missing));
    missing = available;
    missing.hole = false;
    ARPG_REQUIRE(!arpg::platform::should_draw_material_environment(
        missing));
    missing = available;
    missing.background = false;
    ARPG_REQUIRE(!arpg::platform::should_draw_material_environment(
        missing));
    missing = available;
    missing.props = false;
    ARPG_REQUIRE(!arpg::platform::should_draw_material_environment(
        missing));

    available.door = false;
    available.hole = false;
    available.door_required = false;
    available.hole_required = false;
    ARPG_REQUIRE(arpg::platform::should_draw_material_environment(available));
    return {};
}

arpg::test::Failure formal_background_only_path_reuses_the_production_draw() noexcept {
    const std::string host_header = read_project_source(
        "src/platform/raylib/raylib_host.hpp");
    const std::string host = read_project_source(
        "src/platform/raylib/raylib_host.cpp");
    const std::string renderer_header = read_project_source(
        "src/platform/raylib/combat_renderer.hpp");
    const std::string renderer = read_project_source(
        "src/platform/raylib/room_renderer.cpp");
    const std::string material_pack = read_project_source(
        "src/platform/raylib/material_pack.cpp");
    const std::string platform_cmake = read_project_source(
        "tests/platform/CMakeLists.txt");
    const std::string formal = read_project_source(
        "tests/platform/stage12_material_formal_game_validation.cpp");
    const std::string validator = read_project_source(
        "tests/platform/stage12_material_validator.ps1");
    const std::string validator_self_test = read_project_source(
        "tests/platform/stage12_material_validator_self_test.ps1");
    ARPG_REQUIRE(!host_header.empty());
    ARPG_REQUIRE(!host.empty());
    ARPG_REQUIRE(!renderer_header.empty());
    ARPG_REQUIRE(!renderer.empty());
    ARPG_REQUIRE(!material_pack.empty());
    ARPG_REQUIRE(!platform_cmake.empty());
    ARPG_REQUIRE(!formal.empty());
    ARPG_REQUIRE(!validator.empty());
    ARPG_REQUIRE(!validator_self_test.empty());

    const std::size_t stage10_input = host.find(
        "combat::MovementInput stage10_validation_input");
    ARPG_REQUIRE(stage10_input != std::string::npos);
    const std::string stage10_input_block = braced_block_after(
        host, stage10_input);
    ARPG_REQUIRE(!stage10_input_block.empty());
    for (const char* validation_only : {
             "scenario == Stage10ValidationScenario::abyss_hole_descent",
             "session.request_active_skill_slot(1U)",
             "session.request_active_skill_slot(0U)",
             "combat::kStormCenterForward", "combat::kDrawSlashRange"}) {
        ARPG_REQUIRE(stage10_input_block.find(validation_only)
            != std::string::npos);
    }
    ARPG_REQUIRE(occurrence_count(stage10_input_block,
        "request_active_skill_slot") == 2U);
    ARPG_REQUIRE(occurrence_count(host,
        "request_active_skill_slot")
        == occurrence_count(stage10_input_block,
            "request_active_skill_slot"));

    ARPG_REQUIRE(platform_cmake.find(
        "target_link_options(arpg_stage12_material_formal PRIVATE /STACK:2097152)")
        != std::string::npos);
    ARPG_REQUIRE(occurrence_count(platform_cmake, "/STACK:2097152") == 1U);

    const std::size_t material_draw = material_pack.find(
        "void draw_material(");
    ARPG_REQUIRE(material_draw != std::string::npos);
    const std::string material_draw_block = braced_block_after(
        material_pack, material_draw);
    const std::size_t begin_shader = material_draw_block.find(
        "BeginShaderMode(");
    const std::size_t set_texture = material_draw_block.find(
        "SetShaderValueTexture(");
    const std::size_t set_value_1 = material_draw_block.find(
        "SetShaderValue(", set_texture);
    const std::size_t set_value_2 = material_draw_block.find(
        "SetShaderValue(", set_value_1 + 1U);
    const std::size_t set_value_3 = material_draw_block.find(
        "SetShaderValue(", set_value_2 + 1U);
    const std::size_t draw_texture = material_draw_block.find(
        "DrawTexturePro(");
    const std::size_t end_shader = material_draw_block.find("EndShaderMode(");
    ARPG_REQUIRE(begin_shader < set_texture);
    ARPG_REQUIRE(set_texture < set_value_1);
    ARPG_REQUIRE(set_value_1 < set_value_2);
    ARPG_REQUIRE(set_value_2 < set_value_3);
    ARPG_REQUIRE(set_value_3 < draw_texture);
    ARPG_REQUIRE(draw_texture < end_shader);

    ARPG_REQUIRE(host_header.find(
        "bool stage12_material_background_only{};") != std::string::npos);
    ARPG_REQUIRE(host_header.find(
        "bool stage12_material_icons_only{};") != std::string::npos);
    ARPG_REQUIRE(host_header.find(
        "bool stage12_material_showcase_hide_items{};") != std::string::npos);
    for (const char* field : {"hud_ecology", "room_background_ecology",
             "room_background_atlas", "room_background_resident",
             "room_background_drawn", "room_background_source_width",
             "room_background_source_height", "room_background_scale"}) {
        ARPG_REQUIRE(host_header.find(field) != std::string::npos);
    }
    ARPG_REQUIRE(renderer_header.find(
        "draw_room_background_only") != std::string::npos);
    ARPG_REQUIRE(renderer_header.find(
        "draw_ground_loot_icons_only") != std::string::npos);

    const std::size_t background_only = renderer.find(
        "CombatRenderer::draw_room_background_only");
    ARPG_REQUIRE(background_only != std::string::npos);
    const std::string background_only_block = braced_block_after(
        renderer, background_only);
    ARPG_REQUIRE(background_only_block.find(
        "material_pack_.load(material_ecology(ecology))") != std::string::npos);
    ARPG_REQUIRE(background_only_block.find(
        "draw_environment_room(material_pack_, ecology)") != std::string::npos);
    ARPG_REQUIRE(background_only_block.find("draw_graybox_room(ecology)")
        != std::string::npos);
    for (const char* forbidden : {"draw_room(", "draw_actors(", "draw_hud(",
             "draw_abyss(", "draw_environment_hazards(",
             "draw_ground_materials(", "draw_ground_items(",
             "draw_water_room_props(", "draw_lightning_room_props(",
             "draw_chaos_room_props(", "draw_doors(", "draw_hole(",
             "draw_fire_room_props("}) {
        ARPG_REQUIRE(background_only_block.find(forbidden) == std::string::npos);
    }

    const std::size_t icons_only = renderer.find(
        "CombatRenderer::draw_ground_loot_icons_only");
    ARPG_REQUIRE(icons_only != std::string::npos);
    const std::string icons_only_block = braced_block_after(renderer, icons_only);
    for (const char* required : {"build_ground_loot_view(",
             "draw_room_background_only(", "draw_ground_materials(",
             "draw_ground_items("}) {
        ARPG_REQUIRE(icons_only_block.find(required) != std::string::npos);
    }
    for (const char* forbidden : {"draw_room(", "draw_actors(", "draw_hud(",
             "draw_abyss(", "draw_environment_hazards(",
             "draw_environment_room_props(", "draw_fire_room_props(",
             "draw_doors(", "draw_hole("}) {
        ARPG_REQUIRE(icons_only_block.find(forbidden) == std::string::npos);
    }

    const std::size_t host_background_draw = host.find(
        "renderer.draw_room_background_only(");
    ARPG_REQUIRE(host_background_draw != std::string::npos);
    const std::size_t host_gate = host.rfind(
        "if (config.stage12_material_background_only)", host_background_draw);
    ARPG_REQUIRE(host_gate != std::string::npos);
    const std::string host_gate_block = braced_block_after(host, host_gate);
    ARPG_REQUIRE(host_gate_block.find("renderer.draw_room_background_only(")
        != std::string::npos);
    ARPG_REQUIRE(host_gate_block.find("renderer.draw(") == std::string::npos);
    ARPG_REQUIRE(host_gate_block.find("draw_hud") == std::string::npos);
    const std::size_t normal_draw = host.find(
        "return renderer.draw(", host_gate);
    const std::size_t runtime_status = host.find(
        "if (config.stage12_material_runtime_status", host_gate);
    ARPG_REQUIRE(normal_draw != std::string::npos);
    ARPG_REQUIRE(runtime_status != std::string::npos);
    ARPG_REQUIRE(host_background_draw < normal_draw);
    ARPG_REQUIRE(normal_draw < runtime_status);
    const std::size_t host_icons_draw = host.find(
        "renderer.draw_ground_loot_icons_only(", host_background_draw);
    ARPG_REQUIRE(host_icons_draw != std::string::npos);
    ARPG_REQUIRE(host_background_draw < host_icons_draw);
    ARPG_REQUIRE(host_icons_draw < normal_draw);

    for (const char* required : {"items-icons-1280x720.png",
             "items-icons-baseline-1280x720.png",
             "config.stage12_material_icons_only = icons_only;",
             "bool icons_only = false,\n"
             "    std::uint32_t fixed_steps_per_frame = 0U,\n"
             "    bool hide_showcase_items = false)",
             "== platform::Stage11CHudValidationScenario::none\n"
             "        ? fixed_steps_per_frame : 8U;",
             "lightning_runtime.lightning_shooter_draw.frame_index == 2U"}) {
        ARPG_REQUIRE(formal.find(required) != std::string::npos);
    }
    for (const char* required : {"lightning-monsters-isolated-1280x720.png",
             "lightning-background-isolated-1280x720.png",
             "config.stage12_material_showcase_hide_items = hide_showcase_items;"}) {
        ARPG_REQUIRE(formal.find(required) != std::string::npos);
    }
    ARPG_REQUIRE(host.find(
        "(config.stage12_material_showcase\n"
        "                            && config.validation_capture_file.has_value())")
        != std::string::npos);
    ARPG_REQUIRE(host.find(
        "items::ItemRarity::rare, items::ItemRarity::magic,\n"
        "            items::ItemRarity::normal, items::ItemRarity::normal,")
        != std::string::npos);
    ARPG_REQUIRE(validator.find(
        "Measure-ItemCapture $itemIconScreenshot $itemIconBaselineScreenshot 'isolated'")
        != std::string::npos);
    ARPG_REQUIRE(validator.find(
        "Measure-ItemCapture $itemScreenshot $itemBaselineScreenshot 'legacy'")
        != std::string::npos);
    ARPG_REQUIRE(validator.find("$brightChroma -ge 600")
        != std::string::npos);
    ARPG_REQUIRE(validator.find("$authoredOverrides")
        == std::string::npos);
    ARPG_REQUIRE(validator.find("$darkAuthoredRegions")
        == std::string::npos);
    for (const char* strict_threshold : {
             "$minimumChanged = 190; $minimumLargest = 185",
             "$minimumChanged = 180; $minimumLargest = 170",
             "$minimumChanged = 210; $minimumLargest = 210",
             "$minimumChanged = 200; $minimumLargest = 200",
             "$minimumBrightChroma = 100; $minimumAnyChroma = 115",
             "$minimumBrightChroma = 6; $minimumAnyChroma = 16",
             "$minimumBrightChroma = 24; $minimumAnyChroma = 40",
             "$minimumBrightChroma = 80; $minimumAnyChroma = 110"}) {
        ARPG_REQUIRE(validator.find(strict_threshold) != std::string::npos);
    }
    const std::size_t item_measure = validator.find(
        "function Measure-ItemCapture");
    ARPG_REQUIRE(item_measure != std::string::npos);
    const std::string item_measure_block = braced_block_after(
        validator, item_measure);
    const std::size_t changed_mask = item_measure_block.find(
        "if ($difference -ge 36)");
    ARPG_REQUIRE(changed_mask != std::string::npos);
    const std::string changed_mask_block = braced_block_after(
        item_measure_block, changed_mask);
    for (const char* changed_only : {"++$brightChroma", "++$anyChroma",
             "$colors.Add("}) {
        ARPG_REQUIRE(changed_mask_block.find(changed_only)
            != std::string::npos);
        ARPG_REQUIRE(occurrence_count(item_measure_block, changed_only)
            == occurrence_count(changed_mask_block, changed_only));
    }
    ARPG_REQUIRE(item_measure_block.find("$colors.Count -lt")
        == std::string::npos);
    const std::size_t atlas_measure = validator.find(
        "function Measure-ItemAtlasPalette");
    ARPG_REQUIRE(atlas_measure != std::string::npos);
    const std::string atlas_measure_block = braced_block_after(
        validator, atlas_measure);
    for (const char* atlas_contract : {
             "$minimumAlpha = 96", "$minimumPalette = 45",
             "$paletteOverrides = @{ 12 = 45; 18 = 72; 21 = 72; 22 = 64 }",
             "New-Object 'bool[,]'", "HashSet[int]", "-shr 4",
             "if ($component -gt $largest)",
             "if ($largest -eq 0 -or $largestPalette -lt $requiredPalette)"}) {
        ARPG_REQUIRE(atlas_measure_block.find(atlas_contract)
            != std::string::npos);
    }
    ARPG_REQUIRE(validator.find(
        "Measure-ItemAtlasPalette $publishedItemsAtlas")
        != std::string::npos);
    ARPG_REQUIRE(validator_self_test.find("$mutations.Count -ne 80")
        != std::string::npos);
    ARPG_REQUIRE(validator_self_test.find(
        "'partial-material10-degradation' 636 562 8 8")
        != std::string::npos);
    for (const char* mutation : {"aliased-item-icon-evidence",
             "missing-equipment-weapon-roi", "missing-material6-roi",
             "missing-material9-roi", "aliased-lightning-isolated-evidence",
             "lightning-shooter-horizontal-strip",
             "lightning-shooter-grayscale",
             "partial-material6-degradation",
             "partial-material9-degradation",
             "partial-material10-degradation",
             "collapsed-item-atlas-material0-palette",
             "collapsed-item-atlas-material6-palette",
             "collapsed-item-atlas-material9-palette",
             "collapsed-item-atlas-material10-palette"}) {
        ARPG_REQUIRE(validator_self_test.find(mutation) != std::string::npos);
    }

    const std::size_t normal_hud_observe = host.find(
        "renderer.observe_presented_hud_frame(hud_presented_frame,");
    const std::size_t showcase_snapshot = host.rfind(
        "presented_snapshot = current;", normal_hud_observe);
    const std::size_t showcase_transform = host.rfind(
        "apply_stage12_material_showcase(presented_snapshot,",
        normal_hud_observe);
    ARPG_REQUIRE(normal_hud_observe != std::string::npos);
    ARPG_REQUIRE(showcase_snapshot != std::string::npos);
    ARPG_REQUIRE(showcase_transform != std::string::npos);
    ARPG_REQUIRE(showcase_snapshot < showcase_transform);
    ARPG_REQUIRE(showcase_transform < normal_hud_observe);
    ARPG_REQUIRE(host.find(
        "material_status.hud_ecology = "
        "renderer.hud_model().navigation.ecology;") != std::string::npos);

    for (const char* guarded_draw : {"draw_passive_tree_overlay(",
             "inventory.draw(", "pause_menu_renderer.draw(",
             "draw_stage12_ui_material_gallery("}) {
        const std::size_t draw = host.find(guarded_draw, host_gate);
        ARPG_REQUIRE(draw != std::string::npos);
        const std::size_t guard = host.rfind(
            "if (!config.stage12_material_background_only", draw);
        ARPG_REQUIRE(guard != std::string::npos);
        ARPG_REQUIRE(draw - guard < 500U);
    }

    const std::size_t init_background_recovery = host.find(
        "if ((config.stage12_material_background_only");
    const std::size_t init_window = host.find("InitWindow(");
    ARPG_REQUIRE(init_background_recovery != std::string::npos);
    ARPG_REQUIRE(init_window != std::string::npos);
    ARPG_REQUIRE(init_background_recovery < init_window);
    const std::size_t init_recovery_state = host.find(
        "DungeonRuntimeState::recovery_required", init_background_recovery);
    const std::size_t init_recovery_open = host.find(
        '{', init_background_recovery);
    ARPG_REQUIRE(init_recovery_state != std::string::npos);
    ARPG_REQUIRE(init_recovery_open != std::string::npos);
    ARPG_REQUIRE(init_recovery_state < init_recovery_open);
    const std::string init_recovery_block = braced_block_after(
        host, init_background_recovery);
    ARPG_REQUIRE(init_recovery_block.find(
        "HostExitCode::save_initialization_failed") != std::string::npos);

    const std::size_t recovery_ui = host.find(
        "if (runtime.state() == DungeonRuntimeState::recovery_required)");
    ARPG_REQUIRE(recovery_ui != std::string::npos);
    const std::string recovery_ui_block = braced_block_after(host, recovery_ui);
    const std::size_t recovery_background_guard = recovery_ui_block.find(
        "if (config.stage12_material_background_only");
    const std::size_t recovery_draw = recovery_ui_block.find(
        "draw_recovery_screen(");
    ARPG_REQUIRE(recovery_background_guard != std::string::npos);
    ARPG_REQUIRE(recovery_ui_block.find("config.stage12_material_icons_only",
        recovery_background_guard) != std::string::npos);
    ARPG_REQUIRE(recovery_draw != std::string::npos);
    ARPG_REQUIRE(recovery_background_guard < recovery_draw);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"uses independent native background prop and hole paths",
        &environment_renderer_uses_independent_native_paths},
    {"falls back atomically when a required frame is missing",
        &environment_falls_back_atomically_when_a_required_frame_is_missing},
    {"directional door atlas leaves room background independent",
        &directional_door_atlas_is_drawable_without_gating_background},
    {"formal background-only path reuses the production draw",
        &formal_background_only_path_reuses_the_production_draw},
};

}  // namespace

arpg::test::TestSuite stage12_environment_render_suite() noexcept {
    return arpg::test::make_suite("stage12_environment_render", kCases);
}
