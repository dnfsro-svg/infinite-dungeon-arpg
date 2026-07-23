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

constexpr arpg::test::TestCase kCases[] = {
    {"uses only the atomic native background plan",
        &environment_renderer_uses_only_the_atomic_native_plan},
    {"falls back atomically when a required frame is missing",
        &environment_falls_back_atomically_when_a_required_frame_is_missing},
};

}  // namespace

arpg::test::TestSuite stage12_environment_render_suite() noexcept {
    return arpg::test::make_suite("stage12_environment_render", kCases);
}
