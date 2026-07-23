#include "material_asset_validation.hpp"
#include "raylib_host.hpp"
#include "hud_font.hpp"
#include "hud_layout.hpp"
#include "inventory_view_math.hpp"
#include "pause_menu_view.hpp"
#include "dungeon/dungeon_types.hpp"
#include "ui_material.hpp"
#include "ui_text_contrast.hpp"
#include "ui_text_bounds_audit.hpp"

#include <raylib.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <initializer_list>
#include <string>

namespace {

namespace platform = arpg::platform;

struct Resolution final { int width{}; int height{}; const char* name{}; };
constexpr std::array<Resolution, 2> kResolutions{{
    {1280, 720, "game-1280x720.png"}, {1920, 1080, "game-1920x1080.png"},
}};
bool png_has_size(const std::filesystem::path& path, int width, int height) {
    const Image image = LoadImage(path.string().c_str());
    const bool valid = image.data != nullptr && image.width == width
        && image.height == height;
    if (image.data != nullptr) UnloadImage(image);
    return valid;
}

bool copy_materials(const std::filesystem::path& executable) {
    const std::filesystem::path destination = executable.parent_path();
    const std::filesystem::path source{ARPG_PROJECT_SOURCE_DIR};
    const auto manifest = platform::default_material_manifest();
    std::error_code error{};
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        for (const char* relative : {manifest.atlases[index].color_path,
                                    manifest.atlases[index].material_path}) {
            if (relative == nullptr) return false;
            const std::filesystem::path target = destination / relative;
            std::filesystem::create_directories(target.parent_path(), error);
            if (error) return false;
            std::filesystem::copy_file(source / relative, target,
                std::filesystem::copy_options::overwrite_existing, error);
            if (error) return false;
        }
    }
    return true;
}

template <std::size_t Size>
bool every_resource_drawn(const std::array<std::uint64_t, Size>& counts) {
    for (const std::uint64_t count : counts) {
        if (count == 0U) return false;
    }
    return true;
}

template <std::size_t Size>
bool ui_resources_drawn(const platform::Stage12MaterialRuntimeStatus& status,
    const std::array<platform::UiMaterialElement, Size>& resources) {
    for (const platform::UiMaterialElement resource : resources) {
        if (status.ui_material_draws[static_cast<std::size_t>(resource)] == 0U) {
            return false;
        }
    }
    return status.ui_material_resident;
}

std::uint64_t ui_draw_mask(
    const platform::Stage12MaterialRuntimeStatus& status) noexcept {
    std::uint64_t mask{};
    for (std::size_t index{}; index < status.ui_material_draws.size(); ++index) {
        if (status.ui_material_draws[index] != 0U) mask |= (1ULL << index);
    }
    return mask;
}

template <std::size_t Size>
bool no_direct_stretch(
    const platform::Stage12MaterialRuntimeStatus& status,
    const std::array<platform::UiMaterialElement, Size>& resources) {
    for (const platform::UiMaterialElement resource : resources) {
        if (status.ui_direct_stretch_draws[
                static_cast<std::size_t>(resource)] != 0U) {
            return false;
        }
    }
    return true;
}

bool rectangle_inside(Rectangle inner, Rectangle outer) noexcept {
    return inner.width > 0.0F && inner.height > 0.0F
        && inner.x >= outer.x && inner.y >= outer.y
        && inner.x + inner.width <= outer.x + outer.width
        && inner.y + inner.height <= outer.y + outer.height;
}

bool rectangles_separated(Rectangle first, Rectangle second) noexcept {
    return first.x + first.width <= second.x
        || second.x + second.width <= first.x
        || first.y + first.height <= second.y
        || second.y + second.height <= first.y;
}

std::uint64_t ui_text_role_mask(
    std::initializer_list<platform::UiTextAuditRole> roles) noexcept {
    std::uint64_t mask{};
    for (const platform::UiTextAuditRole role : roles) {
        mask |= 1ULL << static_cast<std::size_t>(role);
    }
    return mask;
}

bool real_font_text_bounds_safe(
    const platform::Stage12MaterialRuntimeStatus& status,
    platform::UiTextAuditPage page, std::uint64_t required_roles,
    std::uint32_t minimum_measurements) noexcept {
    const std::size_t index = static_cast<std::size_t>(page);
    return index < status.ui_text_bounds_safe.size()
        && status.bundled_font_ready
        && status.ui_text_bounds_safe[index]
        && status.ui_text_sizes_readable[index]
        && status.ui_text_measured_counts[index] >= minimum_measurements
        && (status.ui_text_observed_roles[index] & required_roles)
            == required_roles;
}

bool hud_text_layout_safe(int width, int height) noexcept {
    const auto layout = platform::make_hud_layout(width, height, false);
    const auto text = platform::make_hud_text_safe_layout(layout);
    return platform::hud_rect_inside(text.objective_title,
               layout.objective_panel)
        && platform::hud_rect_inside(text.objective_hint,
               layout.objective_panel)
        && !platform::hud_rects_overlap(text.objective_title,
               text.objective_hint)
        && platform::hud_rect_inside(text.navigation_title,
               layout.navigation_panel)
        && platform::hud_rect_inside(text.navigation_ecology,
               layout.navigation_panel)
        && !platform::hud_rects_overlap(text.navigation_title,
               text.navigation_ecology);
}

bool inventory_text_layout_safe(int width, int height) noexcept {
    const auto panels = platform::inventory_layout(width, height);
    const auto skill = platform::active_skill_loadout_layout(width, height);
    const auto text = platform::inventory_text_safe_layout(width, height);
    const Rectangle viewport{0.0F, 0.0F, static_cast<float>(width),
        static_cast<float>(height)};
    return rectangle_inside(text.page_title, viewport)
        && rectangles_separated(text.page_title,
            skill.equipment_page_button)
        && rectangles_separated(text.page_title,
            skill.skill_stones_page_button)
        && rectangle_inside(text.equipment_panel_title, panels.equipment)
        && rectangle_inside(text.grid_panel_title, panels.grid)
        && rectangle_inside(text.detail_panel_title, panels.detail);
}

bool skill_text_layout_safe(int width, int height) noexcept {
    const auto skill = platform::active_skill_loadout_layout(width, height);
    const auto text = platform::inventory_text_safe_layout(width, height);
    return rectangle_inside(text.skill_panel_title, skill.panel)
        && rectangles_separated(text.skill_panel_title, skill.main_slots[0])
        && rectangles_separated(text.support_section_title,
            skill.support_slots[0])
        && rectangles_separated(text.inventory_section_title,
            skill.inventory_slots[0]);
}

bool pause_text_layout_safe(int width, int height) noexcept {
    const auto layout = platform::pause_menu_layout(width, height);
    return rectangle_inside(layout.title, layout.panel)
        && rectangles_separated(layout.title, layout.rows[0])
        && rectangle_inside(layout.footer, layout.panel)
        && rectangles_separated(layout.rows[20], layout.footer);
}

bool path_is_within(const std::filesystem::path& child,
    const std::filesystem::path& parent) {
    const auto relative = child.lexically_relative(parent);
    return !relative.empty() && !relative.is_absolute()
        && *relative.begin() != "..";
}

bool allowed_evidence_parent(const std::filesystem::path& candidate) {
    std::error_code error{};
    const auto absolute = std::filesystem::absolute(candidate, error)
        .lexically_normal();
    if (error || absolute.empty() || absolute == absolute.root_path()
        || absolute.filename() != "stage12 material evidence") return false;
    const std::string generic = absolute.generic_string();
    return generic.find("/out/build/") != std::string::npos;
}

bool prepare_evidence_run(const std::filesystem::path& candidate,
    std::filesystem::path& run_root) {
    if (!allowed_evidence_parent(candidate)) return false;
    std::error_code error{};
    const auto parent = std::filesystem::absolute(candidate, error).lexically_normal();
    if (error) return false;
    run_root = parent / "stage12-run";
    if (!path_is_within(run_root, parent)) return false;
    const auto status = std::filesystem::symlink_status(run_root, error);
    if (error && error != std::errc::no_such_file_or_directory) return false;
    if (!error && std::filesystem::is_symlink(status)) return false;
    error.clear();
    std::filesystem::remove_all(run_root, error);
    if (error) return false;
    std::filesystem::create_directories(run_root, error);
    return !error;
}

bool root_safety_self_test(const std::filesystem::path& root) {
    std::error_code error{};
    std::filesystem::create_directories(root, error);
    if (error) return false;
    const auto sentinel = root / "sentinel.txt";
    std::ofstream output(sentinel, std::ios::out | std::ios::trunc);
    output << "must survive";
    output.close();
    std::filesystem::path ignored{};
    const bool rejected_temp_parent = !prepare_evidence_run(root, ignored);
    const bool rejected_source_root = !prepare_evidence_run(
        std::filesystem::path{ARPG_PROJECT_SOURCE_DIR}, ignored);
    const bool rejected_workspace = !prepare_evidence_run(
        std::filesystem::path{ARPG_PROJECT_SOURCE_DIR}.parent_path(), ignored);
    return rejected_temp_parent && rejected_source_root && rejected_workspace
        && std::filesystem::is_regular_file(sentinel, error) && !error;
}

bool capture(const std::filesystem::path& root, const Resolution& resolution,
    const char* image_name = nullptr, bool showcase = false,
    bool request_f12 = false,
    std::optional<arpg::dungeon::DungeonElement> showcase_ecology = std::nullopt,
    platform::Stage12MaterialRuntimeStatus* material_status = nullptr,
    bool hide_showcase_monsters = false,
    const char* baseline_image_name = nullptr,
    platform::Stage12UiShowcase ui_showcase =
        platform::Stage12UiShowcase::none,
    platform::Stage11CHudValidationScenario hud_scenario =
        platform::Stage11CHudValidationScenario::none) {
    const std::filesystem::path capture = root / (image_name == nullptr
        ? resolution.name : image_name);
    platform::RaylibHostConfig config{};
    config.window_width = resolution.width;
    config.window_height = resolution.height;
    config.window_title = "Stage12 Comic Material Formal Validation";
    config.save_directory = root / (std::string{"save-"} + resolution.name);
    config.settings_directory = root / (std::string{"settings-"} + resolution.name);
    config.screenshot_directory = root / (std::string{"f12-"}
        + (image_name == nullptr ? resolution.name : image_name));
    config.new_run_seed = 12012U;
    config.validation_exit_after_presented_frames = hud_scenario
        == platform::Stage11CHudValidationScenario::none ? 4U : 600U;
    config.validation_steps_per_frame = hud_scenario
        == platform::Stage11CHudValidationScenario::none ? 0U : 8U;
    config.validation_capture_file = capture;
    config.stage12_material_showcase = showcase;
    config.stage12_material_showcase_ecology = showcase_ecology;
    config.stage12_material_runtime_status = material_status;
    config.stage12_material_showcase_hide_monsters = hide_showcase_monsters;
    config.stage12_ui_showcase = ui_showcase;
    config.stage11c_hud_validation = hud_scenario;
    if (baseline_image_name != nullptr) {
        config.stage12_material_baseline_capture_file =
            root / baseline_image_name;
    }
    config.validation_request_screenshot = request_f12;
    const auto started = std::filesystem::file_time_type::clock::now()
        - std::chrono::seconds(2);
    const auto result = platform::run_raylib_host(config);
    std::error_code error{};
    return result == platform::HostExitCode::success
        && std::filesystem::is_regular_file(capture, error) && !error
        && std::filesystem::file_size(capture, error) > 1024U && !error
        && std::filesystem::last_write_time(capture, error) >= started && !error
        && png_has_size(capture, resolution.width, resolution.height);
}

bool text_contains(const std::filesystem::path& path, const char* text) {
    std::ifstream input(path);
    std::string contents((std::istreambuf_iterator<char>(input)), {});
    return input && contents.find(text) != std::string::npos;
}

bool run_input_hole_evidence(const std::filesystem::path& root,
    const std::filesystem::path& executable) {
    const std::filesystem::path validator = executable.parent_path()
        / "arpg_stage10_formal_game_validation.exe";
    const std::filesystem::path command_file = root / "run-input-hole.cmd";
    std::ofstream command(command_file, std::ios::out | std::ios::trunc);
    command << "@echo off\r\n\"" << validator.string() << "\"\r\n";
    command.close();
    const std::string invoke = "call \"" + command_file.string() + "\"";
    const std::filesystem::path source_summary = executable.parent_path()
        / "stage10-formal-game-validation" / "formal-path-summary.txt";
    const std::filesystem::path copied_summary = root / "input-hole-summary.txt";
    std::error_code error{};
    const bool ran = command && std::filesystem::is_regular_file(validator)
        && std::system(invoke.c_str()) == 0;
    if (ran) std::filesystem::copy_file(source_summary, copied_summary,
        std::filesystem::copy_options::overwrite_existing, error);
    return ran && !error && text_contains(copied_summary, "depth=2")
        && text_contains(copied_summary, "last_transition=1")
        && text_contains(copied_summary, "resolution_valid=1");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 3 && argv[1] != nullptr && argv[2] != nullptr
        && std::string{argv[1]} == "--root-safety-self-test") {
        return root_safety_self_test(std::filesystem::absolute(argv[2])) ? 0 : 1;
    }
    if (argc != 2 || argv[1] == nullptr) return 2;
    std::filesystem::path root{};
    if (!prepare_evidence_run(std::filesystem::absolute(argv[1]), root)
        || !copy_materials(std::filesystem::absolute(argv[0]))) return 3;
    std::error_code error{};
    bool captures_ok = true;
    for (const Resolution& resolution : kResolutions) {
        captures_ok = capture(root, resolution) && captures_ok;
    }
    const std::filesystem::path executable = std::filesystem::absolute(argv[0]);
    const std::filesystem::path effects = executable.parent_path() / "assets"
        / "stage12" / "effects_ui.png";
    const std::filesystem::path corrupt_effects = effects.string() + ".corrupt";
    std::filesystem::rename(effects, corrupt_effects, error);
    const bool fallback_capture = !error && capture(root, kResolutions[0],
        "fallback-1280x720.png");
    error.clear();
    std::filesystem::rename(corrupt_effects, effects, error);
    const auto material_manifest = platform::default_material_manifest();
    const bool manifest_ok =
        platform::validate_material_manifest(material_manifest).valid;
    const std::size_t material_full_pack_bytes =
        platform::full_pack_bytes(material_manifest);
    const std::size_t material_resident_peak_bytes =
        platform::resident_peak_bytes(material_manifest);
    platform::Stage12MaterialRuntimeStatus item_runtime{};
    const bool item_showcase_ok = capture(root, kResolutions[0],
        "items-materials-1280x720.png", true, false,
        arpg::dungeon::DungeonElement::fire, &item_runtime, true,
        "items-baseline-1280x720.png");
    const std::filesystem::path item_baseline =
        root / "items-baseline-1280x720.png";
    error.clear();
    const bool item_baseline_ok = std::filesystem::is_regular_file(
            item_baseline, error) && !error
        && std::filesystem::file_size(item_baseline, error) > 1024U && !error
        && png_has_size(item_baseline, 1280, 720);
    const bool item_runtime_ok = item_showcase_ok
        && item_runtime.items_ui_resident
        && every_resource_drawn(item_runtime.equipment_slot_draws)
        && every_resource_drawn(item_runtime.rarity_draws)
        && every_resource_drawn(item_runtime.material_draws);
    const bool ui_baseline_ok = capture(root, kResolutions[0],
        "ui-baseline-1280x720.png");
    const bool ui_baseline_1920_ok = capture(root, kResolutions[1],
        "ui-baseline-1920x1080.png");
    platform::Stage12MaterialRuntimeStatus ui_runtime{};
    const bool ui_gallery_ok = capture(root, kResolutions[0],
        "ui-gallery-1280x720.png", false, false, std::nullopt,
        &ui_runtime, false, nullptr,
        platform::Stage12UiShowcase::material_gallery);
    platform::Stage12MaterialRuntimeStatus hud_ui_runtime{};
    const bool hud_ui_ok = capture(root, kResolutions[0],
        "ui-hud-1280x720.png", false, false, std::nullopt,
        &hud_ui_runtime, false, nullptr, platform::Stage12UiShowcase::none,
        platform::Stage11CHudValidationScenario::low_health_status);
    platform::Stage12MaterialRuntimeStatus hud_ui_1920_runtime{};
    const bool hud_ui_1920_ok = capture(root, kResolutions[1],
        "ui-hud-1920x1080.png", false, false, std::nullopt,
        &hud_ui_1920_runtime, false, nullptr,
        platform::Stage12UiShowcase::none,
        platform::Stage11CHudValidationScenario::low_health_status);
    platform::Stage12MaterialRuntimeStatus inventory_ui_runtime{};
    const bool inventory_ui_ok = capture(root, kResolutions[0],
        "ui-inventory-1280x720.png", false, false, std::nullopt,
        &inventory_ui_runtime, false, nullptr,
        platform::Stage12UiShowcase::inventory);
    platform::Stage12MaterialRuntimeStatus inventory_ui_1920_runtime{};
    const bool inventory_ui_1920_ok = capture(root, kResolutions[1],
        "ui-inventory-1920x1080.png", false, false, std::nullopt,
        &inventory_ui_1920_runtime, false, nullptr,
        platform::Stage12UiShowcase::inventory);
    platform::Stage12MaterialRuntimeStatus skill_ui_runtime{};
    const bool skill_ui_ok = capture(root, kResolutions[0],
        "ui-skill-stones-1280x720.png", false, false, std::nullopt,
        &skill_ui_runtime, false, nullptr,
        platform::Stage12UiShowcase::skill_stones);
    platform::Stage12MaterialRuntimeStatus skill_ui_1920_runtime{};
    const bool skill_ui_1920_ok = capture(root, kResolutions[1],
        "ui-skill-stones-1920x1080.png", false, false, std::nullopt,
        &skill_ui_1920_runtime, false, nullptr,
        platform::Stage12UiShowcase::skill_stones);
    platform::Stage12MaterialRuntimeStatus pause_ui_runtime{};
    const bool pause_ui_ok = capture(root, kResolutions[0],
        "ui-pause-1280x720.png", false, false, std::nullopt,
        &pause_ui_runtime, false, nullptr, platform::Stage12UiShowcase::pause);
    platform::Stage12MaterialRuntimeStatus pause_ui_1920_runtime{};
    const bool pause_ui_1920_ok = capture(root, kResolutions[1],
        "ui-pause-1920x1080.png", false, false, std::nullopt,
        &pause_ui_1920_runtime, false, nullptr,
        platform::Stage12UiShowcase::pause);
    const bool ui_runtime_ok = ui_gallery_ok
        && ui_runtime.ui_material_resident
        && every_resource_drawn(ui_runtime.ui_material_draws);
    using Ui = platform::UiMaterialElement;
    const bool hud_ui_runtime_ok = hud_ui_ok && ui_resources_drawn(
        hud_ui_runtime, std::array{Ui::hud_panel, Ui::hud_health_track,
            Ui::hud_health_fill, Ui::hud_resource_track,
            Ui::hud_skill_empty, Ui::hud_skill_ready, Ui::label_plate});
    const bool inventory_ui_runtime_ok = inventory_ui_ok && ui_resources_drawn(
        inventory_ui_runtime, std::array{Ui::inventory_panel_equipment,
            Ui::inventory_panel_grid, Ui::inventory_panel_detail,
            Ui::inventory_tab_idle, Ui::inventory_tab_active,
            Ui::inventory_slot_idle, Ui::inventory_button_idle,
            Ui::inventory_button_disabled, Ui::label_plate});
    const bool skill_ui_runtime_ok = skill_ui_ok && ui_resources_drawn(
        skill_ui_runtime, std::array{Ui::skill_panel, Ui::skill_slot_empty,
            Ui::skill_slot_support, Ui::inventory_tab_idle,
            Ui::inventory_tab_active, Ui::inventory_button_active});
    const bool pause_ui_runtime_ok = pause_ui_ok && ui_resources_drawn(
        pause_ui_runtime, std::array{Ui::pause_panel, Ui::pause_row_idle,
            Ui::pause_row_selected, Ui::pause_footer});
    const bool hud_ui_1920_runtime_ok = hud_ui_1920_ok && ui_resources_drawn(
        hud_ui_1920_runtime, std::array{Ui::hud_panel, Ui::hud_health_track,
            Ui::hud_health_fill, Ui::hud_resource_track,
            Ui::hud_skill_empty, Ui::hud_skill_ready, Ui::label_plate});
    const bool inventory_ui_1920_runtime_ok = inventory_ui_1920_ok
        && ui_resources_drawn(inventory_ui_1920_runtime,
            std::array{Ui::inventory_panel_equipment,
                Ui::inventory_panel_grid, Ui::inventory_panel_detail,
                Ui::inventory_tab_idle, Ui::inventory_tab_active,
                Ui::inventory_slot_idle, Ui::inventory_button_idle,
                Ui::inventory_button_disabled, Ui::label_plate});
    const bool skill_ui_1920_runtime_ok = skill_ui_1920_ok
        && ui_resources_drawn(skill_ui_1920_runtime,
            std::array{Ui::skill_panel, Ui::skill_slot_empty,
                Ui::skill_slot_support, Ui::inventory_tab_idle,
                Ui::inventory_tab_active, Ui::inventory_button_active});
    const bool pause_ui_1920_runtime_ok = pause_ui_1920_ok
        && ui_resources_drawn(pause_ui_1920_runtime,
            std::array{Ui::pause_panel, Ui::pause_row_idle,
                Ui::pause_row_selected, Ui::pause_footer});
    constexpr std::array kHudNonStretchResources{
        Ui::hud_panel, Ui::hud_health_track, Ui::hud_health_fill,
        Ui::hud_barrier_track, Ui::hud_barrier_fill,
        Ui::hud_resource_track, Ui::hud_resource_fill,
        Ui::hud_status_slow, Ui::hud_status_corrosion,
        Ui::hud_status_invulnerable, Ui::hud_objective_panel,
        Ui::hud_navigation_panel, Ui::hud_notice, Ui::hud_notice_abyss,
        Ui::label_plate};
    constexpr std::array kInventoryNonStretchResources{
        Ui::inventory_panel_equipment, Ui::inventory_panel_grid,
        Ui::inventory_panel_detail, Ui::inventory_tab_idle,
        Ui::inventory_tab_active, Ui::inventory_slot_idle,
        Ui::inventory_slot_selected, Ui::inventory_button_idle,
        Ui::inventory_button_active, Ui::inventory_button_disabled,
        Ui::warning_modal, Ui::label_plate};
    constexpr std::array kSkillNonStretchResources{
        Ui::skill_panel, Ui::skill_slot_empty, Ui::skill_slot_ready,
        Ui::skill_slot_selected, Ui::skill_slot_support,
        Ui::inventory_tab_idle, Ui::inventory_tab_active,
        Ui::inventory_button_active, Ui::inventory_button_disabled};
    constexpr std::array kPauseNonStretchResources{
        Ui::pause_panel, Ui::pause_row_idle, Ui::pause_row_selected,
        Ui::pause_footer};
    const bool hud_non_stretch_ok = no_direct_stretch(
        hud_ui_runtime, kHudNonStretchResources);
    const bool inventory_non_stretch_ok = no_direct_stretch(
        inventory_ui_runtime, kInventoryNonStretchResources);
    const bool skill_non_stretch_ok = no_direct_stretch(
        skill_ui_runtime, kSkillNonStretchResources);
    const bool pause_non_stretch_ok = no_direct_stretch(
        pause_ui_runtime, kPauseNonStretchResources);
    const bool hud_non_stretch_1920_ok = no_direct_stretch(
        hud_ui_1920_runtime, kHudNonStretchResources);
    const bool inventory_non_stretch_1920_ok = no_direct_stretch(
        inventory_ui_1920_runtime, kInventoryNonStretchResources);
    const bool skill_non_stretch_1920_ok = no_direct_stretch(
        skill_ui_1920_runtime, kSkillNonStretchResources);
    const bool pause_non_stretch_1920_ok = no_direct_stretch(
        pause_ui_1920_runtime, kPauseNonStretchResources);
    const auto font_runtime_valid = [](const auto& status) noexcept {
        return status.bundled_font_ready
            && status.bundled_font_glyph_count > 0U
            && status.bundled_font_glyph_count
                <= platform::kDeathOverlayCodepointCapacity
            && status.bundled_font_source_base_size
                >= platform::kUiFontMaximumDisplaySize * 2
            && status.bundled_font_atlas_bytes > 0U
            && status.bundled_font_atlas_bytes
                <= platform::kUiFontAtlasByteBudget
            && status.bundled_font_total_atlas_bytes
                <= status.bundled_font_total_atlas_byte_budget;
    };
    const bool bundled_font_runtime_ok =
        font_runtime_valid(hud_ui_runtime)
        && font_runtime_valid(inventory_ui_runtime)
        && font_runtime_valid(skill_ui_runtime)
        && font_runtime_valid(pause_ui_runtime)
        && font_runtime_valid(hud_ui_1920_runtime)
        && font_runtime_valid(inventory_ui_1920_runtime)
        && font_runtime_valid(skill_ui_1920_runtime)
        && font_runtime_valid(pause_ui_1920_runtime);
    const bool hud_text_1280_ok = hud_text_layout_safe(1280, 720);
    const bool inventory_text_1280_ok = inventory_text_layout_safe(1280, 720);
    const bool skill_text_1280_ok = skill_text_layout_safe(1280, 720);
    const bool pause_text_1280_ok = pause_text_layout_safe(1280, 720);
    const bool hud_text_1920_ok = hud_text_layout_safe(1920, 1080);
    const bool inventory_text_1920_ok = inventory_text_layout_safe(1920, 1080);
    const bool skill_text_1920_ok = skill_text_layout_safe(1920, 1080);
    const bool pause_text_1920_ok = pause_text_layout_safe(1920, 1080);
    const std::uint64_t hud_required_roles = ui_text_role_mask({
        platform::UiTextAuditRole::hud_health,
        platform::UiTextAuditRole::hud_barrier,
        platform::UiTextAuditRole::hud_experience,
        platform::UiTextAuditRole::hud_progression,
        platform::UiTextAuditRole::hud_objective,
        platform::UiTextAuditRole::hud_navigation,
        platform::UiTextAuditRole::hud_skill_name,
    });
    const std::uint64_t inventory_required_roles = ui_text_role_mask({
        platform::UiTextAuditRole::inventory_page_title,
        platform::UiTextAuditRole::inventory_equipment_slot,
        platform::UiTextAuditRole::inventory_statistics,
        platform::UiTextAuditRole::inventory_grid_entry,
        platform::UiTextAuditRole::inventory_material_entry,
        platform::UiTextAuditRole::inventory_detail,
        platform::UiTextAuditRole::inventory_status,
    });
    const std::uint64_t skill_required_roles = ui_text_role_mask({
        platform::UiTextAuditRole::skill_page_title,
        platform::UiTextAuditRole::skill_main_slot,
        platform::UiTextAuditRole::skill_description,
        platform::UiTextAuditRole::skill_inventory_entry,
    });
    const std::uint64_t pause_required_roles = ui_text_role_mask({
        platform::UiTextAuditRole::pause_title,
        platform::UiTextAuditRole::pause_row,
        platform::UiTextAuditRole::pause_footer,
    });
    const bool hud_real_text_1280_ok = real_font_text_bounds_safe(
        hud_ui_runtime, platform::UiTextAuditPage::hud,
        hud_required_roles, 12U);
    const bool hud_real_text_1920_ok = real_font_text_bounds_safe(
        hud_ui_1920_runtime, platform::UiTextAuditPage::hud,
        hud_required_roles, 12U);
    const bool inventory_real_text_1280_ok = real_font_text_bounds_safe(
        inventory_ui_runtime, platform::UiTextAuditPage::inventory,
        inventory_required_roles, 20U);
    const bool inventory_real_text_1920_ok = real_font_text_bounds_safe(
        inventory_ui_1920_runtime, platform::UiTextAuditPage::inventory,
        inventory_required_roles, 20U);
    const bool skill_real_text_1280_ok = real_font_text_bounds_safe(
        skill_ui_runtime, platform::UiTextAuditPage::skill,
        skill_required_roles, 10U);
    const bool skill_real_text_1920_ok = real_font_text_bounds_safe(
        skill_ui_1920_runtime, platform::UiTextAuditPage::skill,
        skill_required_roles, 10U);
    const bool pause_real_text_1280_ok = real_font_text_bounds_safe(
        pause_ui_runtime, platform::UiTextAuditPage::pause,
        pause_required_roles, 5U);
    const bool pause_real_text_1920_ok = real_font_text_bounds_safe(
        pause_ui_1920_runtime, platform::UiTextAuditPage::pause,
        pause_required_roles, 5U);
    const auto physical_font_scale_ok = [](const auto& base_status,
            const auto& full_hd_status,
            platform::UiTextAuditPage page) noexcept {
        const std::size_t index = static_cast<std::size_t>(page);
        return index < base_status.ui_text_minimum_display_sizes.size()
            && base_status.ui_text_minimum_display_sizes[index] > 0.0F
            && full_hd_status.ui_text_minimum_display_sizes[index]
                >= base_status.ui_text_minimum_display_sizes[index] * 1.49F;
    };
    const bool ui_text_physical_scale_ok =
        physical_font_scale_ok(hud_ui_runtime, hud_ui_1920_runtime,
            platform::UiTextAuditPage::hud)
        && physical_font_scale_ok(
            inventory_ui_runtime, inventory_ui_1920_runtime,
            platform::UiTextAuditPage::inventory)
        && physical_font_scale_ok(skill_ui_runtime, skill_ui_1920_runtime,
            platform::UiTextAuditPage::skill)
        && physical_font_scale_ok(pause_ui_runtime, pause_ui_1920_runtime,
            platform::UiTextAuditPage::pause);
    const auto text_contrast = platform::ui_text_contrast_style();
    const float primary_contrast = platform::ui_luma_contrast_ratio(
        text_contrast.primary, text_contrast.backing);
    const float secondary_contrast = platform::ui_luma_contrast_ratio(
        text_contrast.secondary, text_contrast.backing);
    const float muted_contrast = platform::ui_luma_contrast_ratio(
        text_contrast.muted, text_contrast.backing);
    const bool solid_text_fill_ok =
        text_contrast.primary.r == 248U
        && text_contrast.primary.g == 246U
        && text_contrast.primary.b == 238U
        && text_contrast.primary.a == 255U
        && text_contrast.interaction.r == 194U
        && text_contrast.interaction.g == 229U
        && text_contrast.interaction.b == 255U
        && text_contrast.interaction.a == 255U
        && text_contrast.warning.r == 255U
        && text_contrast.warning.g == 210U
        && text_contrast.warning.b == 118U
        && text_contrast.warning.a == 255U;
    const bool text_contrast_ok = solid_text_fill_ok
        && text_contrast.primary.a == 255U
        && text_contrast.secondary.a == 255U
        && text_contrast.muted.a == 255U
        && primary_contrast >= 7.0F && secondary_contrast >= 6.0F
        && muted_contrast >= 4.5F && text_contrast.outline_pixels == 0
        && text_contrast.shadow_pixels >= 2
        && text_contrast.backing.a >= 220U;
    const bool ui_readability_contract_ok = bundled_font_runtime_ok
        && text_contrast_ok
        && hud_non_stretch_ok && inventory_non_stretch_ok
        && skill_non_stretch_ok && pause_non_stretch_ok
        && hud_non_stretch_1920_ok && inventory_non_stretch_1920_ok
        && skill_non_stretch_1920_ok && pause_non_stretch_1920_ok
        && hud_text_1280_ok && inventory_text_1280_ok
        && skill_text_1280_ok && pause_text_1280_ok
        && hud_text_1920_ok && inventory_text_1920_ok
        && skill_text_1920_ok && pause_text_1920_ok;
    const bool real_font_text_bounds_ok = hud_real_text_1280_ok
        && hud_real_text_1920_ok
        && inventory_real_text_1280_ok && inventory_real_text_1920_ok
        && skill_real_text_1280_ok && skill_real_text_1920_ok
        && pause_real_text_1280_ok && pause_real_text_1920_ok
        && ui_text_physical_scale_ok;
    const bool ui_readability_contract_with_real_bounds_ok =
        ui_readability_contract_ok && real_font_text_bounds_ok;
    const bool showcase_ok = capture(root, kResolutions[0],
        "monsters-1280x720.png", true, true);
    platform::Stage12MaterialRuntimeStatus water_runtime{};
    const bool water_showcase_ok = capture(root, kResolutions[0],
        "water-monsters-1280x720.png", true, false,
        arpg::dungeon::DungeonElement::water, &water_runtime);
    const bool water_runtime_ok = water_showcase_ok
        && water_runtime.shader_pipeline_ready
        && water_runtime.water_ecology_ready
        && water_runtime.water_environment_resident
        && water_runtime.water_bulwark_resident
        && water_runtime.water_support_resident;
    platform::Stage12MaterialRuntimeStatus lightning_runtime{};
    const bool lightning_showcase_ok = capture(root, kResolutions[0],
        "lightning-monsters-1280x720.png", true, false,
        arpg::dungeon::DungeonElement::lightning, &lightning_runtime);
    const bool lightning_background_ok = capture(root, kResolutions[0],
        "lightning-background-1280x720.png", true, false,
        arpg::dungeon::DungeonElement::lightning, nullptr, true);
    const bool lightning_runtime_ok = lightning_showcase_ok
        && lightning_runtime.shader_pipeline_ready
        && lightning_runtime.lightning_ecology_ready
        && lightning_runtime.lightning_environment_resident
        && lightning_runtime.lightning_shooter_resident
        && lightning_runtime.lightning_dasher_resident
        && lightning_runtime.lightning_shooter_draw.presenter_visible
        && lightning_runtime.lightning_shooter_draw.use_material_frame
        && lightning_runtime.lightning_shooter_draw.atlas
            == platform::MaterialAtlasId::lightning_shooter
        && lightning_runtime.lightning_shooter_draw.frame_index < 12U
        && lightning_runtime.lightning_shooter_draw.drawn
        && lightning_runtime.lightning_dasher_draw.presenter_visible
        && lightning_runtime.lightning_dasher_draw.use_material_frame
        && lightning_runtime.lightning_dasher_draw.atlas
            == platform::MaterialAtlasId::lightning_dasher
        && lightning_runtime.lightning_dasher_draw.frame_index < 12U
        && lightning_runtime.lightning_dasher_draw.drawn;
    platform::Stage12MaterialRuntimeStatus chaos_runtime{};
    const bool chaos_showcase_ok = capture(root, kResolutions[0],
        "chaos-monsters-1280x720.png", true, false,
        arpg::dungeon::DungeonElement::chaos, &chaos_runtime);
    const bool chaos_background_ok = capture(root, kResolutions[0],
        "chaos-background-1280x720.png", true, false,
        arpg::dungeon::DungeonElement::chaos, nullptr, true);
    const bool chaos_runtime_ok = chaos_showcase_ok
        && chaos_runtime.shader_pipeline_ready
        && chaos_runtime.chaos_ecology_ready
        && chaos_runtime.chaos_environment_resident
        && chaos_runtime.chaos_chaser_resident
        && chaos_runtime.chaos_hazard_resident
        && chaos_runtime.chaos_chaser_draw.presenter_visible
        && chaos_runtime.chaos_chaser_draw.use_material_frame
        && chaos_runtime.chaos_chaser_draw.atlas
            == platform::MaterialAtlasId::chaos_chaser
        && chaos_runtime.chaos_chaser_draw.frame_index < 12U
        && chaos_runtime.chaos_chaser_draw.drawn
        && chaos_runtime.chaos_hazard_draw.presenter_visible
        && chaos_runtime.chaos_hazard_draw.use_material_frame
        && chaos_runtime.chaos_hazard_draw.atlas
            == platform::MaterialAtlasId::chaos_hazard
        && chaos_runtime.chaos_hazard_draw.frame_index < 12U
        && chaos_runtime.chaos_hazard_draw.drawn;
    const std::filesystem::path f12_capture = root / "f12-monsters-1280x720.png"
        / "stage8-equipment-loot.png";
    std::error_code f12_error{};
    const bool f12_ok = std::filesystem::is_regular_file(f12_capture, f12_error)
        && !f12_error && png_has_size(f12_capture, 1280, 720);
    const bool input_hole_ok = run_input_hole_evidence(root, executable);
    std::ofstream report(root / "stage12-material-evidence.txt",
        std::ios::out | std::ios::trunc);
    report << "manifest=" << (manifest_ok ? "pass" : "fail") << '\n'
           << "atlas_bytes=" << material_full_pack_bytes << '\n'
           << "full_pack_bytes=" << material_full_pack_bytes << '\n'
           << "resident_peak_bytes=" << material_resident_peak_bytes << '\n'
           << "fallback=" << (fallback_capture && !error ? "pass" : "fail") << '\n'
           << "input_hole_regression=" << (input_hole_ok ? "pass" : "fail") << '\n'
           << "monsters=" << (showcase_ok ? "fire_bomber,fire_charger,water_bulwark,water_support,lightning_shooter,lightning_dasher,chaos_chaser,chaos_hazard" : "") << '\n'
           << "monster_screenshot=monsters-1280x720.png\n"
           << "item_screenshot=items-materials-1280x720.png\n"
           << "item_baseline_screenshot=items-baseline-1280x720.png\n"
           << "items_ui_pair=" << (item_runtime.items_ui_resident
                ? "resident" : "missing") << '\n'
           << "item_runtime_draws=" << (item_runtime_ok ? "pass" : "fail") << '\n'
           << "ui_material_pair=" << (ui_runtime.ui_material_resident
                ? "resident" : "missing") << '\n'
           << "ui_runtime_draws=" << (ui_runtime_ok ? "pass" : "fail") << '\n'
           << "hud_ui_runtime_draws=" << (hud_ui_runtime_ok ? "pass" : "fail") << '\n'
           << "hud_ui_draw_mask=" << ui_draw_mask(hud_ui_runtime) << '\n'
           << "inventory_ui_runtime_draws=" << (inventory_ui_runtime_ok ? "pass" : "fail") << '\n'
           << "inventory_ui_draw_mask=" << ui_draw_mask(inventory_ui_runtime) << '\n'
           << "skill_ui_runtime_draws=" << (skill_ui_runtime_ok ? "pass" : "fail") << '\n'
           << "skill_ui_draw_mask=" << ui_draw_mask(skill_ui_runtime) << '\n'
           << "pause_ui_runtime_draws=" << (pause_ui_runtime_ok ? "pass" : "fail") << '\n'
           << "pause_ui_draw_mask=" << ui_draw_mask(pause_ui_runtime) << '\n'
           << "hud_ui_runtime_draws_1920=" << (hud_ui_1920_runtime_ok ? "pass" : "fail") << '\n'
           << "hud_ui_draw_mask_1920=" << ui_draw_mask(hud_ui_1920_runtime) << '\n'
           << "inventory_ui_runtime_draws_1920=" << (inventory_ui_1920_runtime_ok ? "pass" : "fail") << '\n'
           << "inventory_ui_draw_mask_1920=" << ui_draw_mask(inventory_ui_1920_runtime) << '\n'
           << "skill_ui_runtime_draws_1920=" << (skill_ui_1920_runtime_ok ? "pass" : "fail") << '\n'
           << "skill_ui_draw_mask_1920=" << ui_draw_mask(skill_ui_1920_runtime) << '\n'
           << "pause_ui_runtime_draws_1920=" << (pause_ui_1920_runtime_ok ? "pass" : "fail") << '\n'
           << "pause_ui_draw_mask_1920=" << ui_draw_mask(pause_ui_1920_runtime) << '\n'
           << "bundled_font_runtime=" << (bundled_font_runtime_ok ? "pass" : "fail") << '\n'
           << "bundled_font_glyph_count=" << hud_ui_runtime.bundled_font_glyph_count << '\n'
           << "bundled_font_subset_capacity=" << platform::kDeathOverlayCodepointCapacity << '\n'
           << "bundled_font_source_base_size=" << hud_ui_runtime.bundled_font_source_base_size << '\n'
           << "bundled_font_max_display_size=" << platform::kUiFontMaximumDisplaySize << '\n'
           << "bundled_font_atlas_bytes=" << hud_ui_runtime.bundled_font_atlas_bytes << '\n'
           << "bundled_font_atlas_byte_budget=" << platform::kUiFontAtlasByteBudget << '\n'
           << "bundled_font_total_atlas_bytes=" << hud_ui_runtime.bundled_font_total_atlas_bytes << '\n'
           << "bundled_font_total_atlas_byte_budget=" << hud_ui_runtime.bundled_font_total_atlas_byte_budget << '\n'
           << "ui_text_contrast=" << (text_contrast_ok ? "pass" : "fail") << '\n'
           << "ui_text_solid_fill=" << (solid_text_fill_ok ? "pass" : "fail") << '\n'
           << "ui_text_primary_contrast=" << primary_contrast << '\n'
           << "ui_text_secondary_contrast=" << secondary_contrast << '\n'
           << "ui_text_muted_contrast=" << muted_contrast << '\n'
           << "ui_text_outline_pixels=" << text_contrast.outline_pixels << '\n'
           << "ui_text_shadow_pixels=" << text_contrast.shadow_pixels << '\n'
           << "ui_text_backing_alpha=" << static_cast<unsigned>(text_contrast.backing.a) << '\n'
           << "ui_text_physical_scale="
           << (ui_text_physical_scale_ok ? "pass" : "fail") << '\n'
           << "inventory_min_font_1280="
           << inventory_ui_runtime.ui_text_minimum_display_sizes[1] << '\n'
           << "inventory_min_font_1920="
           << inventory_ui_1920_runtime.ui_text_minimum_display_sizes[1] << '\n'
           << "skill_min_font_1280="
           << skill_ui_runtime.ui_text_minimum_display_sizes[2] << '\n'
           << "skill_min_font_1920="
           << skill_ui_1920_runtime.ui_text_minimum_display_sizes[2] << '\n'
           << "pause_min_font_1280="
           << pause_ui_runtime.ui_text_minimum_display_sizes[3] << '\n'
           << "pause_min_font_1920="
           << pause_ui_1920_runtime.ui_text_minimum_display_sizes[3] << '\n'
           << "hud_decorative_stretch=" << (hud_non_stretch_ok ? "pass" : "fail") << '\n'
           << "inventory_decorative_stretch=" << (inventory_non_stretch_ok ? "pass" : "fail") << '\n'
           << "skill_decorative_stretch=" << (skill_non_stretch_ok ? "pass" : "fail") << '\n'
           << "pause_decorative_stretch=" << (pause_non_stretch_ok ? "pass" : "fail") << '\n'
           << "hud_decorative_stretch_1920=" << (hud_non_stretch_1920_ok ? "pass" : "fail") << '\n'
           << "inventory_decorative_stretch_1920=" << (inventory_non_stretch_1920_ok ? "pass" : "fail") << '\n'
           << "skill_decorative_stretch_1920=" << (skill_non_stretch_1920_ok ? "pass" : "fail") << '\n'
           << "pause_decorative_stretch_1920=" << (pause_non_stretch_1920_ok ? "pass" : "fail") << '\n'
           << "hud_text_layout=" << (hud_text_1280_ok ? "pass" : "fail") << '\n'
           << "inventory_text_layout=" << (inventory_text_1280_ok ? "pass" : "fail") << '\n'
           << "skill_text_layout=" << (skill_text_1280_ok ? "pass" : "fail") << '\n'
           << "pause_text_layout=" << (pause_text_1280_ok ? "pass" : "fail") << '\n'
           << "hud_text_layout_1920=" << (hud_text_1920_ok ? "pass" : "fail") << '\n'
           << "inventory_text_layout_1920=" << (inventory_text_1920_ok ? "pass" : "fail") << '\n'
           << "skill_text_layout_1920=" << (skill_text_1920_ok ? "pass" : "fail") << '\n'
           << "pause_text_layout_1920=" << (pause_text_1920_ok ? "pass" : "fail") << '\n'
           << "hud_real_font_bounds=" << (hud_real_text_1280_ok ? "pass" : "fail") << '\n'
           << "inventory_real_font_bounds=" << (inventory_real_text_1280_ok ? "pass" : "fail") << '\n'
           << "skill_real_font_bounds=" << (skill_real_text_1280_ok ? "pass" : "fail") << '\n'
           << "pause_real_font_bounds=" << (pause_real_text_1280_ok ? "pass" : "fail") << '\n'
           << "hud_real_font_bounds_1920=" << (hud_real_text_1920_ok ? "pass" : "fail") << '\n'
           << "inventory_real_font_bounds_1920=" << (inventory_real_text_1920_ok ? "pass" : "fail") << '\n'
           << "skill_real_font_bounds_1920=" << (skill_real_text_1920_ok ? "pass" : "fail") << '\n'
           << "pause_real_font_bounds_1920=" << (pause_real_text_1920_ok ? "pass" : "fail") << '\n'
           << "hud_observed_text_roles="
           << hud_ui_runtime.ui_text_observed_roles[0] << '\n'
           << "hud_required_text_roles=" << hud_required_roles << '\n'
           << "hud_observed_text_roles_1920="
           << hud_ui_1920_runtime.ui_text_observed_roles[0] << '\n'
           << "hud_real_font_measurements=" << hud_ui_runtime.ui_text_measured_counts[0] << '\n'
           << "inventory_real_font_measurements=" << inventory_ui_runtime.ui_text_measured_counts[1] << '\n'
           << "skill_real_font_measurements=" << skill_ui_runtime.ui_text_measured_counts[2] << '\n'
           << "pause_real_font_measurements=" << pause_ui_runtime.ui_text_measured_counts[3] << '\n'
           << "hud_failed_bounds_roles=" << hud_ui_runtime.ui_text_failed_bounds_roles[0] << '\n'
           << "inventory_failed_bounds_roles=" << inventory_ui_runtime.ui_text_failed_bounds_roles[1] << '\n'
           << "skill_failed_bounds_roles=" << skill_ui_runtime.ui_text_failed_bounds_roles[2] << '\n'
           << "pause_failed_bounds_roles=" << pause_ui_runtime.ui_text_failed_bounds_roles[3] << '\n'
           << "hud_failed_bounds_roles_1920=" << hud_ui_1920_runtime.ui_text_failed_bounds_roles[0] << '\n'
           << "inventory_failed_bounds_roles_1920=" << inventory_ui_1920_runtime.ui_text_failed_bounds_roles[1] << '\n'
           << "skill_failed_bounds_roles_1920=" << skill_ui_1920_runtime.ui_text_failed_bounds_roles[2] << '\n'
           << "pause_failed_bounds_roles_1920=" << pause_ui_1920_runtime.ui_text_failed_bounds_roles[3] << '\n'
           << "hud_failed_size_roles=" << hud_ui_runtime.ui_text_failed_size_roles[0] << '\n'
           << "inventory_failed_size_roles=" << inventory_ui_runtime.ui_text_failed_size_roles[1] << '\n'
           << "skill_failed_size_roles=" << skill_ui_runtime.ui_text_failed_size_roles[2] << '\n'
           << "pause_failed_size_roles=" << pause_ui_runtime.ui_text_failed_size_roles[3] << '\n'
           << "ui_baseline_screenshot=ui-baseline-1280x720.png\n"
           << "ui_baseline_screenshot_1920=ui-baseline-1920x1080.png\n"
           << "hud_ui_screenshot=ui-hud-1280x720.png\n"
           << "hud_ui_screenshot_1920=ui-hud-1920x1080.png\n"
           << "ui_gallery_screenshot=ui-gallery-1280x720.png\n"
           << "inventory_ui_screenshot=ui-inventory-1280x720.png\n"
           << "inventory_ui_screenshot_1920=ui-inventory-1920x1080.png\n"
           << "skill_ui_screenshot=ui-skill-stones-1280x720.png\n"
           << "skill_ui_screenshot_1920=ui-skill-stones-1920x1080.png\n"
           << "pause_ui_screenshot=ui-pause-1280x720.png\n"
           << "pause_ui_screenshot_1920=ui-pause-1920x1080.png\n"
           << "water_monster_screenshot=water-monsters-1280x720.png\n"
           << "lightning_monster_screenshot=lightning-monsters-1280x720.png\n"
           << "lightning_background_screenshot=lightning-background-1280x720.png\n"
           << "chaos_monster_screenshot=chaos-monsters-1280x720.png\n"
           << "chaos_background_screenshot=chaos-background-1280x720.png\n"
           << "shader_pipeline=" << (water_runtime.shader_pipeline_ready
                    && lightning_runtime.shader_pipeline_ready
                    && chaos_runtime.shader_pipeline_ready
                ? "pass" : "fail") << '\n'
           << "water_ecology_residency=" << (water_runtime.water_ecology_ready
                ? "pass" : "fail") << '\n'
           << "water_environment_pair=" << (water_runtime.water_environment_resident
                ? "resident" : "missing") << '\n'
           << "water_bulwark_pair=" << (water_runtime.water_bulwark_resident
                ? "resident" : "missing") << '\n'
           << "water_support_pair=" << (water_runtime.water_support_resident
                ? "resident" : "missing") << '\n'
           << "lightning_ecology_residency=" << (lightning_runtime.lightning_ecology_ready
                ? "pass" : "fail") << '\n'
           << "lightning_environment_pair=" << (lightning_runtime.lightning_environment_resident
                ? "resident" : "missing") << '\n'
           << "lightning_shooter_pair=" << (lightning_runtime.lightning_shooter_resident
                ? "resident" : "missing") << '\n'
           << "lightning_dasher_pair=" << (lightning_runtime.lightning_dasher_resident
                ? "resident" : "missing") << '\n'
           << "lightning_shooter_presenter=" << (lightning_runtime.lightning_shooter_draw.presenter_visible ? "pass" : "fail") << '\n'
           << "lightning_shooter_use_material_frame=" << (lightning_runtime.lightning_shooter_draw.use_material_frame ? "pass" : "fail") << '\n'
           << "lightning_shooter_atlas=" << (lightning_runtime.lightning_shooter_draw.atlas == platform::MaterialAtlasId::lightning_shooter ? "lightning_shooter" : "wrong") << '\n'
           << "lightning_shooter_frame=" << lightning_runtime.lightning_shooter_draw.frame_index << '\n'
           << "lightning_shooter_drawn=" << (lightning_runtime.lightning_shooter_draw.drawn ? "pass" : "fail") << '\n'
           << "lightning_dasher_presenter=" << (lightning_runtime.lightning_dasher_draw.presenter_visible ? "pass" : "fail") << '\n'
           << "lightning_dasher_use_material_frame=" << (lightning_runtime.lightning_dasher_draw.use_material_frame ? "pass" : "fail") << '\n'
           << "lightning_dasher_atlas=" << (lightning_runtime.lightning_dasher_draw.atlas == platform::MaterialAtlasId::lightning_dasher ? "lightning_dasher" : "wrong") << '\n'
           << "lightning_dasher_frame=" << lightning_runtime.lightning_dasher_draw.frame_index << '\n'
           << "lightning_dasher_drawn=" << (lightning_runtime.lightning_dasher_draw.drawn ? "pass" : "fail") << '\n'
           << "chaos_ecology_residency=" << (chaos_runtime.chaos_ecology_ready ? "pass" : "fail") << '\n'
           << "chaos_environment_pair=" << (chaos_runtime.chaos_environment_resident ? "resident" : "missing") << '\n'
           << "chaos_chaser_pair=" << (chaos_runtime.chaos_chaser_resident ? "resident" : "missing") << '\n'
           << "chaos_hazard_pair=" << (chaos_runtime.chaos_hazard_resident ? "resident" : "missing") << '\n'
           << "chaos_chaser_presenter=" << (chaos_runtime.chaos_chaser_draw.presenter_visible ? "pass" : "fail") << '\n'
           << "chaos_chaser_use_material_frame=" << (chaos_runtime.chaos_chaser_draw.use_material_frame ? "pass" : "fail") << '\n'
           << "chaos_chaser_atlas=" << (chaos_runtime.chaos_chaser_draw.atlas == platform::MaterialAtlasId::chaos_chaser ? "chaos_chaser" : "wrong") << '\n'
           << "chaos_chaser_frame=" << chaos_runtime.chaos_chaser_draw.frame_index << '\n'
           << "chaos_chaser_drawn=" << (chaos_runtime.chaos_chaser_draw.drawn ? "pass" : "fail") << '\n'
           << "chaos_hazard_presenter=" << (chaos_runtime.chaos_hazard_draw.presenter_visible ? "pass" : "fail") << '\n'
           << "chaos_hazard_use_material_frame=" << (chaos_runtime.chaos_hazard_draw.use_material_frame ? "pass" : "fail") << '\n'
           << "chaos_hazard_atlas=" << (chaos_runtime.chaos_hazard_draw.atlas == platform::MaterialAtlasId::chaos_hazard ? "chaos_hazard" : "wrong") << '\n'
           << "chaos_hazard_frame=" << chaos_runtime.chaos_hazard_draw.frame_index << '\n'
           << "chaos_hazard_drawn=" << (chaos_runtime.chaos_hazard_draw.drawn ? "pass" : "fail") << '\n'
           << "f12_screenshot=f12-monsters-1280x720.png/stage8-equipment-loot.png\n"
           << "screenshot_isolation=" << (f12_ok ? "pass" : "fail") << '\n'
           << "screenshot_decode=" << (captures_ok && showcase_ok && item_baseline_ok && item_showcase_ok && ui_baseline_ok && ui_baseline_1920_ok && ui_gallery_ok && hud_ui_ok && hud_ui_1920_ok && inventory_ui_ok && inventory_ui_1920_ok && skill_ui_ok && skill_ui_1920_ok && pause_ui_ok && pause_ui_1920_ok && water_showcase_ok && lightning_showcase_ok && lightning_background_ok && chaos_showcase_ok && chaos_background_ok && f12_ok ? "pass" : "fail") << '\n'
           << "result=" << (captures_ok && fallback_capture && !error && manifest_ok && showcase_ok && item_baseline_ok && item_runtime_ok && ui_runtime_ok && hud_ui_runtime_ok && inventory_ui_runtime_ok && skill_ui_runtime_ok && pause_ui_runtime_ok && hud_ui_1920_runtime_ok && inventory_ui_1920_runtime_ok && skill_ui_1920_runtime_ok && pause_ui_1920_runtime_ok && ui_readability_contract_with_real_bounds_ok && ui_baseline_ok && ui_baseline_1920_ok && water_runtime_ok && lightning_runtime_ok && lightning_background_ok && chaos_runtime_ok && chaos_background_ok && f12_ok && input_hole_ok ? "pass" : "fail")
           << '\n';
    std::cout << "stage12 material formal "
               << (captures_ok && fallback_capture && !error && manifest_ok && showcase_ok && item_baseline_ok && item_runtime_ok && ui_runtime_ok && hud_ui_runtime_ok && inventory_ui_runtime_ok && skill_ui_runtime_ok && pause_ui_runtime_ok && hud_ui_1920_runtime_ok && inventory_ui_1920_runtime_ok && skill_ui_1920_runtime_ok && pause_ui_1920_runtime_ok && ui_readability_contract_with_real_bounds_ok && ui_baseline_ok && ui_baseline_1920_ok && water_runtime_ok && lightning_runtime_ok && lightning_background_ok && chaos_runtime_ok && chaos_background_ok && f12_ok && input_hole_ok ? "PASS" : "FAIL")
              << std::endl;
    return report && captures_ok && fallback_capture && !error && manifest_ok
        && showcase_ok && item_baseline_ok && item_runtime_ok && ui_runtime_ok
        && hud_ui_runtime_ok && inventory_ui_runtime_ok
        && skill_ui_runtime_ok && pause_ui_runtime_ok
        && hud_ui_1920_runtime_ok && inventory_ui_1920_runtime_ok
        && skill_ui_1920_runtime_ok && pause_ui_1920_runtime_ok
        && ui_readability_contract_with_real_bounds_ok
        && ui_baseline_ok && ui_baseline_1920_ok
        && water_runtime_ok && lightning_runtime_ok
        && lightning_background_ok && chaos_runtime_ok && chaos_background_ok
        && f12_ok && input_hole_ok ? 0 : 1;
}
