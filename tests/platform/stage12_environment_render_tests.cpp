#include "test_framework.hpp"

#include "environment_render_plan.hpp"
#include "combat_view_math.hpp"
#include "combat_renderer.hpp"
#include "environment_prop_layout.hpp"
#include "material_pack.hpp"
#include "raylib_host.hpp"
#include "render_layout.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

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

bool every_call_contains(const std::string& source,
    const char* callee, const char* required,
    std::size_t& call_count) noexcept {
    const std::string token = std::string{callee} + "(";
    std::size_t position{};
    while ((position = source.find(token, position)) != std::string::npos) {
        const std::size_t opening = position + token.size() - 1U;
        std::size_t depth{};
        std::size_t closing = std::string::npos;
        for (std::size_t index = opening; index < source.size(); ++index) {
            if (source[index] == '(') {
                ++depth;
            } else if (source[index] == ')' && --depth == 0U) {
                closing = index;
                break;
            }
        }
        if (closing == std::string::npos
            || source.substr(opening, closing - opening + 1U).find(required)
                == std::string::npos) {
            return false;
        }
        ++call_count;
        position = closing + 1U;
    }
    return true;
}

arpg::test::Failure material_showcase_layout_tracks_room_bounds() noexcept {
    const arpg::combat::Vec3 item =
        arpg::platform::stage12_material_showcase_position(-5.0F, -3.2F);
    const arpg::platform::ScreenProjection item_projection =
        arpg::platform::project_combat_position(item, 1280.0F, 720.0F);
    const float item_center_y = item_projection.ground_y
        - 13.0F * item_projection.scale;
    ARPG_REQUIRE(std::fabs(item_projection.x - 453.0F) < 1.0F);
    ARPG_REQUIRE(std::fabs(item_center_y - 339.0F) < 1.0F);

    const arpg::combat::Vec3 material =
        arpg::platform::stage12_material_showcase_position(-4.5F, 2.0F);
    const arpg::platform::ScreenProjection material_projection =
        arpg::platform::project_combat_position(material, 1280.0F, 720.0F);
    const float material_center_y = material_projection.ground_y
        - 6.0F * material_projection.scale;
    ARPG_REQUIRE(std::fabs(material_projection.x - 440.0F) < 1.0F);
    ARPG_REQUIRE(std::fabs(material_center_y - 514.0F) < 1.0F);

    const arpg::combat::Vec3 skill_center =
        arpg::platform::stage12_material_showcase_position(0.5F, 0.0F);
    const arpg::platform::ScreenProjection skill_projection =
        arpg::platform::project_combat_position(
            skill_center, 1280.0F, 720.0F);
    ARPG_REQUIRE(std::fabs(skill_projection.x - 661.0F) < 1.0F);
    ARPG_REQUIRE(std::fabs(skill_projection.ground_y - 454.0F) < 1.0F);
    return {};
}

std::string cpp_code_only(std::string source) {
    std::size_t write{};
    for (std::size_t read = 0U; read < source.size(); ++read) {
        if (source[read] == '\\' && read + 1U < source.size()) {
            if (source[read + 1U] == '\n') {
                ++read;
                continue;
            }
            if (source[read + 1U] == '\r' && read + 2U < source.size()
                    && source[read + 2U] == '\n') {
                read += 2U;
                continue;
            }
        }
        source[write++] = source[read];
    }
    source.resize(write);

    enum class State { code, line_comment, block_comment, string, character,
        raw_string };
    State state = State::code;
    bool escaped = false;
    std::string raw_closer{};
    const auto blank = [&source](std::size_t index) noexcept {
        if (source[index] != '\r' && source[index] != '\n') source[index] = ' ';
    };
    for (std::size_t index = 0U; index < source.size(); ++index) {
        const char current = source[index];
        const char next = index + 1U < source.size() ? source[index + 1U] : '\0';
        if (state == State::code) {
            if (current == '/' && next == '/') {
                blank(index);
                blank(++index);
                state = State::line_comment;
            } else if (current == '/' && next == '*') {
                blank(index);
                blank(++index);
                state = State::block_comment;
            } else if (current == 'R' && next == '"') {
                const std::size_t delimiter_end = source.find('(', index + 2U);
                if (delimiter_end != std::string::npos
                        && delimiter_end - index <= 18U) {
                    raw_closer = ")" + source.substr(
                        index + 2U, delimiter_end - index - 2U) + '"';
                    for (; index <= delimiter_end; ++index) blank(index);
                    --index;
                    state = State::raw_string;
                }
            } else if (current == '"' || current == '\'') {
                state = current == '"' ? State::string : State::character;
                escaped = false;
                blank(index);
            }
        } else if (state == State::line_comment) {
            blank(index);
            if (current == '\n' || (current == '\r' && next != '\n')) {
                state = State::code;
            }
        } else if (state == State::block_comment) {
            blank(index);
            if (current == '*' && next == '/') {
                blank(++index);
                state = State::code;
            }
        } else if (state == State::raw_string) {
            if (source.compare(index, raw_closer.size(), raw_closer) == 0) {
                for (std::size_t offset = 0U; offset < raw_closer.size(); ++offset) {
                    blank(index + offset);
                }
                index += raw_closer.size() - 1U;
                state = State::code;
            } else {
                blank(index);
            }
        } else {
            const bool closes = !escaped
                && ((state == State::string && current == '"')
                    || (state == State::character && current == '\''));
            if (!escaped && current == '\\') escaped = true;
            else escaped = false;
            blank(index);
            if (closes) state = State::code;
        }
    }

    struct ConditionalFrame {
        bool parent_active{};
        bool guaranteed_match{};
    };
    std::vector<ConditionalFrame> conditionals{};
    bool active = true;
    const auto blank_range = [&source](std::size_t begin,
                                 std::size_t end) noexcept {
        for (std::size_t index = begin; index < end; ++index) {
            if (source[index] != '\r' && source[index] != '\n') {
                source[index] = ' ';
            }
        }
    };
    const auto trim = [](std::string value) {
        const std::size_t begin = value.find_first_not_of(" \t\r");
        if (begin == std::string::npos) return std::string{};
        const std::size_t end = value.find_last_not_of(" \t\r");
        return value.substr(begin, end - begin + 1U);
    };
    enum class ConditionKnowledge { always_false, unknown, always_true };
    const auto classify_condition = [&trim](std::string condition) {
        condition = trim(condition);
        while (condition.size() >= 2U && condition.front() == '('
                && condition.back() == ')') {
            condition = trim(condition.substr(1U, condition.size() - 2U));
        }
        if (condition == "0" || condition == "false") {
            return ConditionKnowledge::always_false;
        }
        if (condition == "1" || condition == "true") {
            return ConditionKnowledge::always_true;
        }
        return ConditionKnowledge::unknown;
    };
    for (std::size_t line_begin = 0U; line_begin < source.size();) {
        const std::size_t newline = source.find('\n', line_begin);
        const std::size_t line_end = newline == std::string::npos
            ? source.size() : newline;
        const std::string line = source.substr(line_begin, line_end - line_begin);
        const std::size_t first = line.find_first_not_of(" \t\r");
        const bool directive = first != std::string::npos && line[first] == '#';
        if (directive) {
            const std::string text = trim(line.substr(first + 1U));
            const std::size_t keyword_end = text.find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
            const std::string keyword = text.substr(0U, keyword_end);
            const std::string argument = keyword_end == std::string::npos
                ? std::string{} : trim(text.substr(keyword_end));
            if (keyword == "if") {
                const bool parent_active = active;
                const ConditionKnowledge condition = classify_condition(argument);
                conditionals.push_back({parent_active,
                    condition == ConditionKnowledge::always_true});
                active = parent_active
                    && condition != ConditionKnowledge::always_false;
            } else if (keyword == "ifdef" || keyword == "ifndef") {
                conditionals.push_back({active, false});
            } else if (keyword == "elif" && !conditionals.empty()) {
                ConditionalFrame& frame = conditionals.back();
                const ConditionKnowledge condition = classify_condition(argument);
                active = frame.parent_active && !frame.guaranteed_match
                    && condition != ConditionKnowledge::always_false;
                frame.guaranteed_match = frame.guaranteed_match
                    || condition == ConditionKnowledge::always_true;
            } else if (keyword == "else" && !conditionals.empty()) {
                ConditionalFrame& frame = conditionals.back();
                active = frame.parent_active && !frame.guaranteed_match;
                frame.guaranteed_match = true;
            } else if (keyword == "endif" && !conditionals.empty()) {
                active = conditionals.back().parent_active;
                conditionals.pop_back();
            }
            blank_range(line_begin, line_end);
        } else if (!active) {
            blank_range(line_begin, line_end);
        }
        line_begin = newline == std::string::npos ? source.size() : newline + 1U;
    }
    return source;
}

arpg::test::Failure cpp_code_only_rejects_non_code_decoys() noexcept {
    const std::string fixture =
        "int real_code_marker = 1;\n"
        "// ordinary_comment_decoy\n"
        "/* block_comment_decoy */\n"
        "const char* label = \"string_literal_decoy\";\n"
        "const char* raw = R\"tag(raw_string_decoy)tag\";\n"
        "// continued_lf_comment \\\n"
        "continued_lf_decoy\n"
        "// continued_crlf_comment \\\r\n"
        "continued_crlf_decoy\r\n"
        "/\\\n"
        "/ split_line_comment_lf_decoy\n"
        "int real_after_split_line_lf_marker = 8;\n"
        "/\\\r\n"
        "/ split_line_comment_crlf_decoy\r\n"
        "int real_after_split_line_crlf_marker = 9;\r\n"
        "/\\\n"
        "* split_block_comment_lf_decoy */\n"
        "int real_after_split_block_lf_marker = 10;\n"
        "/\\\r\n"
        "* split_block_comment_crlf_decoy */\r\n"
        "int real_after_split_block_crlf_marker = 11;\r\n"
        "#if 0\n"
        "inactive_zero_decoy\n"
        "#if 1\n"
        "nested_inactive_decoy\n"
        "#endif\n"
        "#else\n"
        "int real_zero_else_marker = 2;\n"
        "#endif\n"
        "#if false\n"
        "inactive_false_decoy\n"
        "#if 0\n"
        "nested_false_decoy\n"
        "#else\n"
        "nested_else_still_inactive_decoy\n"
        "#endif\n"
        "#else\n"
        "int real_false_else_marker = 3;\n"
        "#endif\n"
        "#if true\n"
        "int real_true_marker = 4;\n"
        "#else\n"
        "inactive_true_else_decoy\n"
        "#endif\n"
        "#\\\n"
        "if 0\n"
        "lf_spliced_directive_decoy\n"
        "#\\\n"
        "endif\n"
        "int real_after_lf_splice_marker = 5;\n"
        "#\\\r\n"
        "if 0\r\n"
        "crlf_spliced_directive_decoy\r\n"
        "#\\\r\n"
        "endif\r\n"
        "int real_after_crlf_splice_marker = 6;\r\n"
        "#if(0)\n"
        "compact_if_decoy\n"
        "#endif\n"
        "int real_after_compact_if_marker = 7;\n";
    const std::string code = cpp_code_only(fixture);
    for (const char* real : {"real_code_marker", "real_zero_else_marker",
             "real_false_else_marker", "real_true_marker",
             "real_after_lf_splice_marker",
             "real_after_crlf_splice_marker",
             "real_after_compact_if_marker",
             "real_after_split_line_lf_marker",
             "real_after_split_line_crlf_marker",
             "real_after_split_block_lf_marker",
             "real_after_split_block_crlf_marker",
             "const char* label"}) {
        ARPG_REQUIRE(code.find(real) != std::string::npos);
    }
    for (const char* decoy : {"ordinary_comment_decoy",
             "block_comment_decoy", "string_literal_decoy",
             "raw_string_decoy", "continued_lf_decoy",
             "continued_crlf_decoy", "inactive_zero_decoy",
             "nested_inactive_decoy", "inactive_false_decoy",
             "nested_false_decoy", "nested_else_still_inactive_decoy",
             "inactive_true_else_decoy", "lf_spliced_directive_decoy",
             "crlf_spliced_directive_decoy", "compact_if_decoy",
             "split_line_comment_lf_decoy",
             "split_line_comment_crlf_decoy",
             "split_block_comment_lf_decoy",
             "split_block_comment_crlf_decoy"}) {
        ARPG_REQUIRE(code.find(decoy) == std::string::npos);
    }
    return {};
}

arpg::test::Failure environment_renderer_uses_independent_native_paths() noexcept {
    const std::string renderer = read_project_source(
        "src/platform/raylib/room_renderer.cpp");
    ARPG_REQUIRE(!renderer.empty());
    const std::size_t draw_environment = renderer.find(
        "bool draw_environment_room");
    ARPG_REQUIRE(draw_environment != std::string::npos);
    const std::string draw_environment_block = braced_block_after(
        renderer, draw_environment);
    ARPG_REQUIRE(draw_environment_block.find(
        "room_background_world_tile_plan(ecology, camera)")
        != std::string::npos);
    ARPG_REQUIRE(draw_environment_block.find(
        "project_room_background_world_tile") != std::string::npos);
    ARPG_REQUIRE(draw_environment_block.find("draw_frame_quad(")
        != std::string::npos);
    ARPG_REQUIRE(draw_environment_block.find("plan.source")
        == std::string::npos);
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
        "&& draw_environment_room(material_pack_, current.ecology, camera")
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
        "draw_hole(current, material_pack_, camera)")
        != std::string::npos);
    ARPG_REQUIRE(occurrence_count(draw_room_block,
        "draw_fire_room_props(") == 0U);
    ARPG_REQUIRE(occurrence_count(draw_room_block,
        "draw_environment_room_props(") == 1U);
    ARPG_REQUIRE(draw_room_block.find("material_pack_, current, camera")
        != std::string::npos);
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
        "project_environment_prop(") != std::string::npos);
    ARPG_REQUIRE(shared_props_block.find(
        "environment_prop_draw_style(") != std::string::npos);
    ARPG_REQUIRE(shared_props_block.find("draw_transformed(")
        != std::string::npos);
    ARPG_REQUIRE(shared_props_block.find("draw_break_marker")
        != std::string::npos);
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
        == std::string::npos);
    ARPG_REQUIRE(draw_hole_block.find("project_hole_geometry(")
        != std::string::npos);

    const std::string render_code = cpp_code_only(renderer);
    const std::string material_code = cpp_code_only(read_project_source(
        "src/platform/raylib/material_pack.cpp"));
    for (const char* forbidden : {"RenderTexture", "LoadRenderTexture"}) {
        ARPG_REQUIRE(render_code.find(forbidden) == std::string::npos);
        ARPG_REQUIRE(material_code.find(forbidden) == std::string::npos);
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

arpg::test::Failure world_renderers_share_one_immutable_camera() noexcept {
    constexpr std::array<const char*, 7U> kWorldSources{{
        "src/platform/raylib/combat_renderer.cpp",
        "src/platform/raylib/actor_renderer.cpp",
        "src/platform/raylib/active_skill_renderer.cpp",
        "src/platform/raylib/debug_renderer.cpp",
        "src/platform/raylib/ground_loot_view.cpp",
        "src/platform/raylib/material_loot_view.cpp",
        "src/platform/raylib/room_renderer.cpp",
    }};
    constexpr std::array<const char*, 4U> kProjectionFunctions{{
        "project_combat_position",
        "project_projectile_position",
        "project_hazard_center",
        "project_render_world",
    }};
    std::size_t projection_call_count{};
    for (const char* path : kWorldSources) {
        const std::string code = cpp_code_only(read_project_source(path));
        ARPG_REQUIRE(!code.empty());
        for (const char* projection : kProjectionFunctions) {
            ARPG_REQUIRE(every_call_contains(
                code, projection, "camera", projection_call_count));
        }
    }
    ARPG_REQUIRE(projection_call_count >= 20U);

    const std::string renderer = cpp_code_only(read_project_source(
        "src/platform/raylib/combat_renderer.cpp"));
    const std::size_t draw_marker = renderer.find("CombatRenderer::draw(");
    ARPG_REQUIRE(draw_marker != std::string::npos);
    const std::size_t draw_opening = renderer.find('{', draw_marker);
    ARPG_REQUIRE(draw_opening != std::string::npos);
    const std::string draw_signature = renderer.substr(
        draw_marker, draw_opening - draw_marker);
    ARPG_REQUIRE(draw_signature.find("const CombatCameraView& camera")
        != std::string::npos);
    ARPG_REQUIRE(draw_signature.find(
        "const dungeon::DungeonRenderSnapshot& world")
        != std::string::npos);
    const std::string draw = braced_block_after(renderer, draw_marker);
    ARPG_REQUIRE(occurrence_count(draw, "make_combat_camera_view(") == 0U);
    for (const char* consumer : {
             "make_combat_render_plan", "draw_room", "draw_actors", "draw_world"}) {
        std::size_t consumer_count{};
        ARPG_REQUIRE(every_call_contains(
            draw, consumer, "camera", consumer_count));
        ARPG_REQUIRE(consumer_count == 1U);
    }
    for (const char* world_consumer : {
             "make_combat_render_plan", "draw_room", "draw_actors"}) {
        std::size_t consumer_count{};
        ARPG_REQUIRE(every_call_contains(
            draw, world_consumer, "world", consumer_count));
        ARPG_REQUIRE(consumer_count == 1U);
    }
    ARPG_REQUIRE(draw.find("draw_room(current_hud") == std::string::npos);
    ARPG_REQUIRE(draw.find("draw_actors(previous, current_hud")
        == std::string::npos);

    const std::string props = cpp_code_only(read_project_source(
        "src/platform/raylib/environment_prop_layout.cpp"));
    ARPG_REQUIRE(props.find("normalized") == std::string::npos);
    ARPG_REQUIRE(props.find("record.anchor") != std::string::npos);
    ARPG_REQUIRE(props.find("environment_obstacles[index]")
        != std::string::npos);
    return {};
}

arpg::test::Failure environment_props_keep_world_anchors_when_camera_moves()
    noexcept {
    arpg::dungeon::VisibleEnvironmentSet visible{};
    visible.count = 1U;
    visible.records[0].ordinal = 17U;
    visible.records[0].prop = arpg::combat::RoomPropKind::torch;
    visible.records[0].anchor = {7.0F, 2.0F, 0.0F};
    visible.records[0].scale_bp = 10000U;
    arpg::dungeon::DungeonRenderSnapshot world{};
    world.ecology = arpg::dungeon::DungeonElement::fire;
    world.environment = visible;
    const arpg::platform::EnvironmentPropLayout layout =
        arpg::platform::environment_prop_layout(world);
    ARPG_REQUIRE(layout.count == 1U);
    ARPG_REQUIRE(layout.props[0].ordinal == 17U);
    ARPG_REQUIRE(layout.props[0].world_foot_position.x == 7.0F);
    ARPG_REQUIRE(layout.props[0].world_foot_position.y == 2.0F);

    const arpg::platform::CombatCameraView first_camera =
        arpg::platform::make_combat_camera_view(
            {0.0F, 0.0F, 0.0F}, 1920.0F, 1080.0F);
    const arpg::platform::CombatCameraView moved_camera =
        arpg::platform::make_combat_camera_view(
            {10.0F, 0.0F, 0.0F}, 1920.0F, 1080.0F);
    const arpg::platform::ProjectedEnvironmentProp first =
        arpg::platform::project_environment_prop(
            layout.props[0], first_camera, 1920.0F, 1080.0F);
    const arpg::platform::ProjectedEnvironmentProp moved =
        arpg::platform::project_environment_prop(
            layout.props[0], moved_camera, 1920.0F, 1080.0F);
    ARPG_REQUIRE(first.foot_position.x != moved.foot_position.x);
    ARPG_REQUIRE(layout.props[0].world_foot_position.x == 7.0F);
    ARPG_REQUIRE(layout.props[0].world_foot_position.y == 2.0F);

    world.environment.count = 2U;
    world.environment.records[1].ordinal = 23U;
    world.environment.records[1].prop = arpg::combat::RoomPropKind::crate;
    world.environment.records[1].anchor = {8.0F, 2.0F, 0.0F};
    world.environment.records[1].scale_bp = 10000U;
    world.environment.records[1].obstacle.kind =
        arpg::combat::RoomObstacleKind::breakable;
    world.environment.records[1].obstacle.max_hp = 10U;
    world.environment_obstacles[1].present = true;
    world.environment_obstacles[1].ordinal = 23U;
    world.environment_obstacles[1].kind =
        arpg::combat::RoomObstacleKind::breakable;
    world.environment_obstacles[1].max_hp = 10U;
    world.environment_obstacles[1].intact = false;
    const auto broken = arpg::platform::environment_prop_layout(world);
    ARPG_REQUIRE(broken.count == 2U);
    ARPG_REQUIRE(broken.props[1U].visual_state
        == arpg::platform::EnvironmentPropVisualState::broken_obstacle);
    world.environment_obstacles[1].hp = 10U;
    world.environment_obstacles[1].intact = true;
    const auto intact = arpg::platform::environment_prop_layout(world);
    ARPG_REQUIRE(intact.count == 2U);
    ARPG_REQUIRE(intact.props[1U].visual_state
        == arpg::platform::EnvironmentPropVisualState::intact_obstacle);
    return {};
}

arpg::test::Failure obstacle_layout_preserves_world_aabb_and_broken_identity()
    noexcept {
    arpg::dungeon::DungeonRenderSnapshot world{};
    world.ecology = arpg::dungeon::DungeonElement::fire;
    world.environment.count = 2U;
    auto& solid = world.environment.records[0U];
    solid.ordinal = 7U;
    solid.home_cell = 42U;
    solid.prop = arpg::combat::RoomPropKind::brazier;
    solid.anchor = {-7.0F, 3.0F, 0.0F};
    solid.scale_bp = 10000U;
    solid.quarter_turns = 1U;
    solid.obstacle.kind = arpg::combat::RoomObstacleKind::solid;
    solid.obstacle.bounds = {{-8.0F, 2.0F, 0.0F}, {-6.0F, 4.0F, 2.0F}};
    auto& breakable = world.environment.records[1U];
    breakable.ordinal = 11U;
    breakable.home_cell = 43U;
    breakable.prop = arpg::combat::RoomPropKind::crate;
    breakable.anchor = {7.0F, 3.0F, 0.0F};
    breakable.scale_bp = 10000U;
    breakable.quarter_turns = 3U;
    breakable.obstacle.kind = arpg::combat::RoomObstacleKind::breakable;
    breakable.obstacle.max_hp = 25U;
    breakable.obstacle.bounds = {{6.0F, 2.0F, 0.0F}, {8.0F, 4.0F, 2.0F}};
    world.environment_obstacles[0U] = {7U,
        arpg::combat::RoomObstacleKind::solid, 0U, 0U, 0U, true, true};
    world.environment_obstacles[1U] = {11U,
        arpg::combat::RoomObstacleKind::breakable, 25U, 25U, 0U, true, true};

    const auto intact = arpg::platform::environment_prop_layout(world);
    ARPG_REQUIRE(intact.status
        == arpg::platform::EnvironmentPropLayoutStatus::ok);
    ARPG_REQUIRE(intact.count == 2U);
    ARPG_REQUIRE(std::memcmp(&intact.props[0U].world_obstacle_bounds,
        &solid.obstacle.bounds, sizeof(arpg::combat::Aabb)) == 0);
    ARPG_REQUIRE(std::memcmp(&intact.props[1U].world_obstacle_bounds,
        &breakable.obstacle.bounds, sizeof(arpg::combat::Aabb)) == 0);
    ARPG_REQUIRE(intact.props[0U].visual_state
        == arpg::platform::EnvironmentPropVisualState::intact_obstacle);
    ARPG_REQUIRE(intact.props[1U].visual_state
        == arpg::platform::EnvironmentPropVisualState::intact_obstacle);
    for (std::uint8_t quarter_turns = 0U; quarter_turns < 4U;
            ++quarter_turns) {
        auto billboard = intact.props[0U];
        billboard.quarter_turns = quarter_turns;
        ARPG_REQUIRE(arpg::platform::environment_prop_draw_style(
            billboard).rotation_degrees == 0.0F);
    }

    world.environment_obstacles[1U].hp = 0U;
    world.environment_obstacles[1U].broken_tick = 91U;
    world.environment_obstacles[1U].intact = false;
    const auto broken = arpg::platform::environment_prop_layout(world);
    ARPG_REQUIRE(broken.status
        == arpg::platform::EnvironmentPropLayoutStatus::ok);
    ARPG_REQUIRE(broken.count == intact.count);
    ARPG_REQUIRE(broken.props[1U].ordinal == intact.props[1U].ordinal);
    ARPG_REQUIRE(broken.props[1U].obstacle_kind
        == intact.props[1U].obstacle_kind);
    ARPG_REQUIRE(std::memcmp(&broken.props[1U].world_obstacle_bounds,
        &intact.props[1U].world_obstacle_bounds,
        sizeof(arpg::combat::Aabb)) == 0);
    ARPG_REQUIRE(broken.props[1U].world_foot_position.x
        == intact.props[1U].world_foot_position.x);
    ARPG_REQUIRE(broken.props[1U].world_foot_position.y
        == intact.props[1U].world_foot_position.y);
    ARPG_REQUIRE(broken.props[1U].visual_state
        == arpg::platform::EnvironmentPropVisualState::broken_obstacle);
    const auto broken_style = arpg::platform::environment_prop_draw_style(
        broken.props[1U]);
    ARPG_REQUIRE(broken_style.rotation_degrees == 0.0F);
    ARPG_REQUIRE(broken_style.draw_break_marker);
    ARPG_REQUIRE(broken_style.tint.r != 255U
        || broken_style.tint.g != 255U
        || broken_style.tint.b != 255U);
    return {};
}

arpg::test::Failure invalid_visible_capacity_and_obstacle_state_fail_empty()
    noexcept {
    arpg::dungeon::VisibleEnvironmentSet overflow{};
    overflow.count = static_cast<std::uint16_t>(overflow.records.size() + 1U);
    const auto visible_failure = arpg::platform::environment_prop_layout(
        arpg::dungeon::DungeonElement::fire, overflow);
    ARPG_REQUIRE(visible_failure.status
        == arpg::platform::EnvironmentPropLayoutStatus::capacity_fault);
    ARPG_REQUIRE(visible_failure.count == 0U);

    arpg::dungeon::DungeonRenderSnapshot world{};
    world.ecology = arpg::dungeon::DungeonElement::fire;
    world.environment.count = static_cast<std::uint16_t>(
        world.environment.records.size() + 1U);
    const auto world_capacity_failure =
        arpg::platform::environment_prop_layout(world);
    ARPG_REQUIRE(world_capacity_failure.status
        == arpg::platform::EnvironmentPropLayoutStatus::capacity_fault);
    ARPG_REQUIRE(world_capacity_failure.count == 0U);

    world.environment.count = 1U;
    world.environment.records[0U].ordinal = 5U;
    world.environment.records[0U].prop = arpg::combat::RoomPropKind::crate;
    world.environment.records[0U].scale_bp = 10000U;
    world.environment.records[0U].obstacle.kind =
        arpg::combat::RoomObstacleKind::breakable;
    world.environment.records[0U].obstacle.max_hp = 10U;
    const auto missing_runtime = arpg::platform::environment_prop_layout(world);
    ARPG_REQUIRE(missing_runtime.status
        == arpg::platform::EnvironmentPropLayoutStatus::invalid_obstacle_state);
    ARPG_REQUIRE(missing_runtime.count == 0U);
    return {};
}

arpg::test::Failure early_unlock_omits_full_clear_only_door_decoration()
    noexcept {
    const auto early = arpg::platform::door_render_decision(
        arpg::platform::DoorVisualMode::open,
        arpg::dungeon::ExitDirection::right, false);
    const auto cleared = arpg::platform::door_render_decision(
        arpg::platform::DoorVisualMode::open,
        arpg::dungeon::ExitDirection::right, true);
    ARPG_REQUIRE(early.sprite
        == arpg::platform::MaterialSpriteId::environment_door_chaos);
    ARPG_REQUIRE(cleared.sprite == early.sprite);
    ARPG_REQUIRE(!early.draw_full_clear_decoration);
    ARPG_REQUIRE(cleared.draw_full_clear_decoration);
    ARPG_REQUIRE(early.body_tint.r != 255U || early.body_tint.g != 0U
        || early.body_tint.b != 0U);
    return {};
}

arpg::test::Failure hole_fallback_geometry_scales_with_camera_projection()
    noexcept {
    const arpg::combat::Vec3 hole{0.0F, 3.5F, 0.0F};
    const arpg::platform::CombatCameraView near_camera{
        {0.0F, -20.0F, 0.0F}, 24.0F, 11.0F};
    const arpg::platform::CombatCameraView far_camera{
        {0.0F, 20.0F, 0.0F}, 24.0F, 11.0F};
    const auto near_geometry = arpg::platform::project_hole_geometry(
        hole, near_camera, 1920.0F, 1080.0F);
    const auto far_geometry = arpg::platform::project_hole_geometry(
        hole, far_camera, 1920.0F, 1080.0F);
    const auto near_projection = arpg::platform::project_render_world(
        hole.x, hole.y, hole.z, near_camera, 1920.0F, 1080.0F);
    const auto far_projection = arpg::platform::project_render_world(
        hole.x, hole.y, hole.z, far_camera, 1920.0F, 1080.0F);
    ARPG_REQUIRE(arpg::test::near(
        near_geometry.radius_x, 74.0F * near_projection.scale));
    ARPG_REQUIRE(arpg::test::near(
        near_geometry.radius_y, 25.0F * near_projection.scale));
    ARPG_REQUIRE(arpg::test::near(
        near_geometry.outline_thickness, 2.0F * near_projection.scale));
    ARPG_REQUIRE(arpg::test::near(
        far_geometry.radius_x, 74.0F * far_projection.scale));
    ARPG_REQUIRE(near_geometry.radius_x > far_geometry.radius_x);
    ARPG_REQUIRE(near_geometry.center.x == near_projection.x);
    ARPG_REQUIRE(near_geometry.center.y == near_projection.ground_y);
    ARPG_REQUIRE(arpg::platform::player_in_hole_range(
        {3.0F, 3.5F, 0.0F}, hole,
        arpg::platform::kHoleInteractionRadius));
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
    const arpg::test::Failure scanner_failure =
        cpp_code_only_rejects_non_code_decoys();
    if (scanner_failure.expression != nullptr) return scanner_failure;
    const std::string host_header = read_project_source(
        "src/platform/raylib/raylib_host.hpp");
    const std::string host = read_project_source(
        "src/platform/raylib/raylib_host.cpp");
    const std::string stage17_runtime = read_project_source(
        "src/platform/raylib/host_validation_stage17_runtime.cpp");
    const std::string stage10_11 = read_project_source(
        "src/platform/raylib/host_validation_stage10_11.cpp");
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
    ARPG_REQUIRE(!stage17_runtime.empty());
    ARPG_REQUIRE(!stage10_11.empty());
    ARPG_REQUIRE(!renderer_header.empty());
    ARPG_REQUIRE(!renderer.empty());
    ARPG_REQUIRE(!material_pack.empty());
    ARPG_REQUIRE(!platform_cmake.empty());
    ARPG_REQUIRE(!formal.empty());
    ARPG_REQUIRE(!validator.empty());
    ARPG_REQUIRE(!validator_self_test.empty());

    const std::string host_code = cpp_code_only(host);
    const std::string stage17_runtime_code = cpp_code_only(stage17_runtime);
    for (const char* stage12_host_only : {
             "void apply_stage12_material_showcase(",
             "const bool stage12_item_baseline_frame =",
             "config.stage12_material_background_only",
             "config.stage12_material_icons_only",
             "config.stage12_material_baseline_capture_file"}) {
        ARPG_REQUIRE(host_code.find(stage12_host_only) != std::string::npos);
        ARPG_REQUIRE(stage17_runtime_code.find(stage12_host_only)
            == std::string::npos);
    }

    const std::size_t stage10_input = stage10_11.find(
        "combat::MovementInput stage10_validation_input");
    ARPG_REQUIRE(stage10_input != std::string::npos);
    const std::string stage10_input_block = braced_block_after(
        stage10_11, stage10_input);
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
    ARPG_REQUIRE(occurrence_count(stage10_11,
        "request_active_skill_slot")
        == occurrence_count(stage10_input_block,
            "request_active_skill_slot"));

    ARPG_REQUIRE(platform_cmake.find(
        "target_link_options(arpg_stage12_material_formal PRIVATE /STACK:2097152)")
        != std::string::npos);
    ARPG_REQUIRE(occurrence_count(platform_cmake, "/STACK:2097152") == 1U);

    const std::size_t material_composite = material_pack.find(
        "void begin_material_composite(");
    ARPG_REQUIRE(material_composite != std::string::npos);
    const std::string material_composite_block = braced_block_after(
        material_pack, material_composite);
    const std::size_t begin_shader = material_composite_block.find(
        "BeginShaderMode(");
    const std::size_t set_texture = material_composite_block.find(
        "SetShaderValueTexture(");
    const std::size_t set_value_1 = material_composite_block.find(
        "SetShaderValue(", set_texture);
    const std::size_t set_value_2 = material_composite_block.find(
        "SetShaderValue(", set_value_1 + 1U);
    const std::size_t set_value_3 = material_composite_block.find(
        "SetShaderValue(", set_value_2 + 1U);
    ARPG_REQUIRE(begin_shader < set_texture);
    ARPG_REQUIRE(set_texture < set_value_1);
    ARPG_REQUIRE(set_value_1 < set_value_2);
    ARPG_REQUIRE(set_value_2 < set_value_3);

    const std::size_t material_draw = material_pack.find(
        "void draw_material(");
    ARPG_REQUIRE(material_draw != std::string::npos);
    const std::string material_draw_block = braced_block_after(
        material_pack, material_draw);
    const std::size_t begin_composite = material_draw_block.find(
        "begin_material_composite(");
    const std::size_t draw_texture = material_draw_block.find(
        "DrawTexturePro(");
    const std::size_t end_shader = material_draw_block.find("EndShaderMode(");
    ARPG_REQUIRE(begin_composite < draw_texture);
    ARPG_REQUIRE(draw_texture < end_shader);

    const std::size_t material_quad = material_pack.find(
        "void draw_material_quad(");
    ARPG_REQUIRE(material_quad != std::string::npos);
    const std::string material_quad_block = braced_block_after(
        material_pack, material_quad);
    const std::size_t quad_begin_composite = material_quad_block.find(
        "begin_material_composite(");
    const std::size_t quad_texture = material_quad_block.find("rlSetTexture(");
    const std::size_t quad_begin = material_quad_block.find("rlBegin(");
    const std::size_t quad_end = material_quad_block.find("rlEnd(");
    const std::size_t quad_end_shader = material_quad_block.find(
        "EndShaderMode(");
    ARPG_REQUIRE(quad_begin_composite < quad_texture);
    ARPG_REQUIRE(quad_texture < quad_begin);
    ARPG_REQUIRE(quad_begin < quad_end);
    ARPG_REQUIRE(quad_end < quad_end_shader);

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
        "material_pack_.synchronize_residency(") != std::string::npos);
    ARPG_REQUIRE(background_only_block.find(
        "room_background_residency_request(ecology)") != std::string::npos);
    ARPG_REQUIRE(background_only_block.find("material_pack_.load(")
        == std::string::npos);
    ARPG_REQUIRE(background_only_block.find(
        "draw_environment_room(material_pack_, ecology, camera)")
        != std::string::npos);
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
    ARPG_REQUIRE(icons_only_block.find("make_combat_camera_view(")
        == std::string::npos);
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
    const std::size_t host_icons_gate = host.rfind(
        "if (config.stage12_material_icons_only)", host_icons_draw);
    ARPG_REQUIRE(host_icons_gate != std::string::npos);
    const std::string host_icons_block = braced_block_after(
        host, host_icons_gate);
    ARPG_REQUIRE(host_icons_block.find(
        "presented_snapshot, frame_camera") != std::string::npos);

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
    const std::size_t bounded_world_publish = host.rfind(
        "session->write_render_snapshot(world_query, *render_world)",
        normal_hud_observe);
    const std::size_t showcase_world_transform = host.rfind(
        "apply_stage12_material_showcase_world(", normal_hud_observe);
    const std::size_t same_world_argument = host.find(
        "frame_seconds, pause_blocks_gameplay, render_world);",
        normal_hud_observe);
    const std::size_t begin_drawing = host.find(
        "BeginDrawing();", normal_hud_observe);
    ARPG_REQUIRE(normal_hud_observe != std::string::npos);
    ARPG_REQUIRE(showcase_snapshot != std::string::npos);
    ARPG_REQUIRE(showcase_transform != std::string::npos);
    ARPG_REQUIRE(showcase_snapshot < showcase_transform);
    ARPG_REQUIRE(showcase_transform < normal_hud_observe);
    ARPG_REQUIRE(bounded_world_publish != std::string::npos);
    ARPG_REQUIRE(showcase_world_transform != std::string::npos);
    ARPG_REQUIRE(same_world_argument != std::string::npos);
    ARPG_REQUIRE(begin_drawing != std::string::npos);
    ARPG_REQUIRE(bounded_world_publish < showcase_world_transform);
    ARPG_REQUIRE(showcase_world_transform < normal_hud_observe);
    ARPG_REQUIRE(normal_hud_observe < same_world_argument);
    ARPG_REQUIRE(same_world_argument < begin_drawing);
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
    const std::size_t init_window = host.find(
        "window.initialize(config, committed_settings)");
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
    {"material showcase layout tracks room bounds",
        &material_showcase_layout_tracks_room_bounds},
    {"uses independent native background prop and hole paths",
        &environment_renderer_uses_independent_native_paths},
    {"world renderers share one immutable camera",
        &world_renderers_share_one_immutable_camera},
    {"environment props keep immutable world anchors",
        &environment_props_keep_world_anchors_when_camera_moves},
    {"obstacle layout preserves AABB and broken identity",
        &obstacle_layout_preserves_world_aabb_and_broken_identity},
    {"invalid visible capacity and obstacle state fail empty",
        &invalid_visible_capacity_and_obstacle_state_fail_empty},
    {"early unlock omits full clear door decoration",
        &early_unlock_omits_full_clear_only_door_decoration},
    {"hole fallback geometry follows camera scale",
        &hole_fallback_geometry_scales_with_camera_projection},
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
