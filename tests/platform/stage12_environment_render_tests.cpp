#include "test_framework.hpp"

#include "environment_render_plan.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string read_project_source(const char* relative_path) {
    const std::filesystem::path path =
        std::filesystem::path{ARPG_PROJECT_SOURCE_DIR} / relative_path;
    std::ifstream input{path, std::ios::binary};
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

arpg::test::Failure environment_renderer_uses_only_the_atomic_native_plan() noexcept {
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
        != std::string::npos);
    ARPG_REQUIRE(draw_room_block.find(
        "&& draw_environment_room(material_pack_, current.ecology)")
        != std::string::npos);
    const std::size_t graybox_gate = draw_room_block.find(
        "if (!draw_material_environment)");
    ARPG_REQUIRE(graybox_gate != std::string::npos);
    ARPG_REQUIRE(braced_block_after(draw_room_block, graybox_gate).find(
        "draw_graybox_room") != std::string::npos);
    const std::size_t doors = draw_room_block.find("draw_doors(");
    ARPG_REQUIRE(doors != std::string::npos);
    ARPG_REQUIRE(draw_room_block.substr(doors, 180U).find(
        "draw_material_environment") != std::string::npos);
    ARPG_REQUIRE(draw_room_block.find(
        "draw_hole(current, material_pack_, draw_material_environment)")
        != std::string::npos);
    const std::size_t gate = renderer.find(
        "if (draw_material_environment) {", draw_room);
    ARPG_REQUIRE(gate != std::string::npos);
    const std::string guarded_props = braced_block_after(renderer, gate);
    for (const char* draw_props : {"draw_fire_room_props(",
             "draw_water_room_props(", "draw_lightning_room_props(",
             "draw_chaos_room_props("}) {
        ARPG_REQUIRE(occurrence_count(draw_room_block, draw_props) == 1U);
        ARPG_REQUIRE(occurrence_count(guarded_props, draw_props) == 1U);
    }

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
    const std::string platform_cmake = read_project_source(
        "tests/platform/CMakeLists.txt");
    ARPG_REQUIRE(!host_header.empty());
    ARPG_REQUIRE(!host.empty());
    ARPG_REQUIRE(!renderer_header.empty());
    ARPG_REQUIRE(!renderer.empty());
    ARPG_REQUIRE(!platform_cmake.empty());

    ARPG_REQUIRE(platform_cmake.find(
        "target_link_options(arpg_stage12_material_formal PRIVATE /STACK:2097152)")
        != std::string::npos);
    ARPG_REQUIRE(occurrence_count(platform_cmake, "/STACK:2097152") == 1U);

    ARPG_REQUIRE(host_header.find(
        "bool stage12_material_background_only{};") != std::string::npos);
    for (const char* field : {"hud_ecology", "room_background_ecology",
             "room_background_atlas", "room_background_resident",
             "room_background_drawn", "room_background_source_width",
             "room_background_source_height", "room_background_scale"}) {
        ARPG_REQUIRE(host_header.find(field) != std::string::npos);
    }
    ARPG_REQUIRE(renderer_header.find(
        "draw_room_background_only") != std::string::npos);

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
        "ground_loot_view = renderer.draw(", host_gate);
    const std::size_t normal_else = host.find("} else {", host_gate);
    const std::size_t runtime_status = host.find(
        "if (config.stage12_material_runtime_status", host_gate);
    ARPG_REQUIRE(normal_draw != std::string::npos);
    ARPG_REQUIRE(normal_else != std::string::npos);
    ARPG_REQUIRE(runtime_status != std::string::npos);
    ARPG_REQUIRE(normal_else < normal_draw);
    ARPG_REQUIRE(normal_draw < runtime_status);

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
        "if (config.stage12_material_background_only");
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
        "if (config.stage12_material_background_only)");
    const std::size_t recovery_draw = recovery_ui_block.find(
        "draw_recovery_screen(");
    ARPG_REQUIRE(recovery_background_guard != std::string::npos);
    ARPG_REQUIRE(recovery_draw != std::string::npos);
    ARPG_REQUIRE(recovery_background_guard < recovery_draw);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"uses only the atomic native background plan",
        &environment_renderer_uses_only_the_atomic_native_plan},
    {"falls back atomically when a required frame is missing",
        &environment_falls_back_atomically_when_a_required_frame_is_missing},
    {"formal background-only path reuses the production draw",
        &formal_background_only_path_reuses_the_production_draw},
};

}  // namespace

arpg::test::TestSuite stage12_environment_render_suite() noexcept {
    return arpg::test::make_suite("stage12_environment_render", kCases);
}
