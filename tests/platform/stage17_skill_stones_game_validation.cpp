#include "persistence/checkpoint_codec.hpp"
#include "persistence/room_progress_codec.hpp"
#include "active_skill_renderer.hpp"
#include "raylib_host.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <thread>

namespace {

namespace platform = arpg::platform;
namespace persistence = arpg::persistence;
namespace combat = arpg::combat;
namespace skills = arpg::skills;

std::filesystem::path g_executable{};

struct Resolution final {
    int width{};
    int height{};
};

constexpr std::array<Resolution, 3U> kPoe2Resolutions{{
    {800, 450},
    {1280, 720},
    {1920, 1080},
}};

[[nodiscard]] std::string resolution_name(const Resolution resolution) {
    return std::to_string(resolution.width) + 'x'
        + std::to_string(resolution.height);
}

[[nodiscard]] bool legacy_resolution(const Resolution resolution) noexcept {
    return resolution.width == 1280 && resolution.height == 720;
}

[[nodiscard]] std::filesystem::path resolution_run(
    const std::filesystem::path& run, const Resolution resolution) {
    return run / resolution_name(resolution);
}

[[nodiscard]] bool path_is_within(const std::filesystem::path& child,
    const std::filesystem::path& parent) noexcept {
    auto child_part = child.begin();
    auto parent_part = parent.begin();
    for (; parent_part != parent.end(); ++parent_part, ++child_part) {
        if (child_part == child.end() || *child_part != *parent_part) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool prepare_evidence(const std::filesystem::path& argument,
    std::filesystem::path& run) noexcept {
    try {
        std::error_code error{};
        std::filesystem::create_directories(argument, error);
        if (error) return false;
        const std::filesystem::path root =
            std::filesystem::weakly_canonical(argument, error);
        if (error || root.empty()) return false;
        run = (root / "stage17-run").lexically_normal();
        if (run.filename() != "stage17-run" || run.parent_path() != root
                || !path_is_within(run, root)) {
            return false;
        }
        if (std::filesystem::exists(run, error)) {
            const std::filesystem::path existing =
                std::filesystem::weakly_canonical(run, error);
            if (error || existing != run || !path_is_within(existing, root)) {
                return false;
            }
            std::filesystem::remove_all(existing, error);
            if (error) return false;
        }
        std::filesystem::create_directories(run, error);
        return !error;
    } catch (...) {
        return false;
    }
}

[[nodiscard]] std::string quote(const std::filesystem::path& value) {
    std::string result{"\""};
    for (const char character : value.string()) {
        if (character == '"') result += '\\';
        result += character;
    }
    result += '"';
    return result;
}

[[nodiscard]] bool regular_nonempty(
    const std::filesystem::path& path) noexcept {
    std::error_code error{};
    return std::filesystem::is_regular_file(path, error) && !error
        && std::filesystem::file_size(path, error) > 0U && !error;
}

[[nodiscard]] bool png_has_dimensions(const std::filesystem::path& path,
    const Resolution resolution) noexcept {
    if (!regular_nonempty(path)) return false;
    const Image image = LoadImage(path.string().c_str());
    const bool valid = IsImageValid(image)
        && image.width == resolution.width && image.height == resolution.height;
    UnloadImage(image);
    return valid;
}

[[nodiscard]] std::string capture_name(const char* stem,
    const Resolution resolution) {
    return std::string{stem} + resolution_name(resolution) + ".png";
}

[[nodiscard]] std::array<std::string, 3U> production_legacy_images(
    const Resolution resolution) {
    return {{
        capture_name("01-new-default-", resolution),
        capture_name("02-draw-slash-windup-", resolution),
        capture_name("03-draw-slash-hit-", resolution),
    }};
}

[[nodiscard]] std::array<std::string, 4U> production_skill_images(
    const Resolution resolution) {
    return {{
        capture_name("poe2-inventory-skill-empty-", resolution),
        capture_name("poe2-inventory-skill-removed-", resolution),
        capture_name("poe2-inventory-skill-selected-", resolution),
        capture_name("poe2-inventory-skill-swapped-", resolution),
    }};
}

[[nodiscard]] bool production_images_valid(
    const std::filesystem::path& run, const Resolution resolution) noexcept {
    for (const std::string& name : production_skill_images(resolution)) {
        if (!png_has_dimensions(run / name, resolution)) return false;
    }
    for (const std::string& name : production_legacy_images(resolution)) {
        const bool exists = std::filesystem::exists(run / name);
        if (legacy_resolution(resolution)) {
            if (!exists || !png_has_dimensions(run / name, resolution)) {
                return false;
            }
        } else if (exists) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::map<std::string, std::string> read_fields(
    const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::map<std::string, std::string> fields{};
    std::string line{};
    while (std::getline(stream, line)) {
        const std::size_t separator = line.find('=');
        if (separator != std::string::npos) {
            fields.emplace(line.substr(0U, separator),
                line.substr(separator + 1U));
        }
    }
    return fields;
}

[[nodiscard]] bool field_is(
    const std::map<std::string, std::string>& fields,
    const char* key, const char* expected) noexcept {
    const auto found = fields.find(key);
    return found != fields.end() && found->second == expected;
}

[[nodiscard]] bool capture_fields_are(
    const std::map<std::string, std::string>& fields, const char* prefix,
    const bool captured, const bool skipped, const std::string& file) noexcept {
    const std::string base{prefix};
    return field_is(fields, (base + "_captured").c_str(),
               captured ? "1" : "0")
        && field_is(fields, (base + "_skipped").c_str(),
            skipped ? "1" : "0")
        && field_is(fields, (base + "_capture_file").c_str(), file.c_str());
}

[[nodiscard]] bool field_is_positive(
    const std::map<std::string, std::string>& fields,
    const char* key) noexcept {
    const auto found = fields.find(key);
    if (found == fields.end()) return false;
    try {
        return std::stoull(found->second) > 0U;
    } catch (...) {
        return false;
    }
}

[[nodiscard]] bool run_child(const std::filesystem::path& run,
    const char* scenario, std::uint64_t seed,
    const Resolution resolution) {
    std::error_code error{};
    const std::filesystem::path child_run = resolution_run(run, resolution);
    std::filesystem::create_directories(child_run, error);
    if (error) return false;
    const std::filesystem::path command = child_run /
        (std::string{"stage17-"} + scenario + ".cmd");
    std::ofstream stream(command, std::ios::out | std::ios::trunc);
    stream << "@echo off\r\n" << quote(g_executable)
           << " --scenario " << scenario << ' ' << quote(run)
           << ' ' << seed << ' ' << resolution.width << ' '
           << resolution.height << "\r\n";
    if (!stream) return false;
    stream.close();
    return std::system(("call " + quote(command)).c_str()) == 0;
}

[[nodiscard]] bool run_host_child(const std::filesystem::path& run,
    const std::string& scenario, std::uint64_t seed,
    const Resolution resolution) noexcept {
    platform::RaylibHostConfig config{};
    config.window_width = resolution.width;
    config.window_height = resolution.height;
    config.window_title = "Stage 17 Skill Stones Production Validation";
    const bool storm = scenario == "storm";
    const std::filesystem::path child_run = resolution_run(run, resolution);
    config.save_directory = child_run / (storm ? "storm-save" : "save");
    config.settings_directory = child_run /
        (storm ? "storm-settings" : "settings");
    config.screenshot_directory = run;
    config.new_run_seed = seed;
    config.validation_steps_per_frame = 1U;
    config.validation_exit_after_presented_frames =
        scenario == "restart" ? 300U : 1200U;
    if (scenario == "production") {
        config.stage17_skill_stones_validation =
            platform::Stage17SkillStonesValidationScenario::production_sequence;
        config.validation_summary_file = child_run /
            ("production-summary-" + resolution_name(resolution) + ".txt");
    } else if (scenario == "storm") {
        config.stage17_skill_stones_validation =
            platform::Stage17SkillStonesValidationScenario::storm_sequence;
        config.validation_summary_file = child_run / "storm-summary.txt";
    } else if (scenario == "restart") {
        config.stage17_skill_stones_validation =
            platform::Stage17SkillStonesValidationScenario::restarted_loadout;
        config.validation_summary_file = child_run / "restart-summary.txt";
    } else {
        return false;
    }
    return platform::run_raylib_host(config) == platform::HostExitCode::success
        && regular_nonempty(*config.validation_summary_file);
}

[[nodiscard]] bool production_summary_valid(
    const std::map<std::string, std::string>& fields,
    const Resolution resolution) noexcept {
    const bool legacy = legacy_resolution(resolution);
    const auto legacy_images = production_legacy_images(resolution);
    const auto skill_images = production_skill_images(resolution);
    return field_is(fields, "scenario", "production_sequence")
        && field_is(fields, "result", "pass")
        && field_is(fields, "window", resolution_name(resolution).c_str())
        && field_is(fields, "initial_slots",
            "draw_slash,storm_swords,none,none,none")
        && field_is(fields, "final_slots",
            "none,draw_slash,none,none,storm_swords")
        && field_is(fields, "owned_active_bits", "3")
        && field_is(fields, "support_none_count", "25")
        && field_is(fields, "empty_slots_none", "1")
        && field_is(fields, "draw_accepted", "1")
        && field_is(fields, "draw_frame_peak", "35")
        && field_is(fields, "draw_hit_count", "2")
        && field_is(fields, "storm_accepted", "0")
        && field_is(fields, "storm_strike_hit_count", "0")
        && field_is(fields, "storm_finisher_hit_count", "0")
        && field_is(fields, "public_input_path", "1")
        && field_is(fields, "production_transactions", "1")
        && field_is(fields, "production_inventory_closed", "1")
        && field_is(fields, "production_cooldown_wait_started", "1")
        && field_is_positive(fields, "production_cooldown_start_ticks")
        && field_is_positive(fields, "production_cooldown_wait_ticks")
        && field_is(fields,
            "production_cooldowns_zero_before_shutdown", "1")
        && field_is(fields, "clean_shutdown_exact_ready", "1")
        && capture_fields_are(fields, "initial", legacy, !legacy,
            legacy ? legacy_images[0] : "N/A")
        && capture_fields_are(fields, "draw_windup", legacy, !legacy,
            legacy ? legacy_images[1] : "N/A")
        && capture_fields_are(fields, "draw_hit", legacy, !legacy,
            legacy ? legacy_images[2] : "N/A")
        && capture_fields_are(fields, "skill_empty", true, false,
            skill_images[0])
        && capture_fields_are(fields, "skill_removed", true, false,
            skill_images[1])
        && capture_fields_are(fields, "skill_selected", true, false,
            skill_images[2])
        && capture_fields_are(fields, "skill_swapped", true, false,
            skill_images[3]);
}

[[nodiscard]] bool storm_summary_valid(
    const std::map<std::string, std::string>& fields) noexcept {
    return field_is(fields, "scenario", "storm_sequence")
        && field_is(fields, "result", "pass")
        && field_is(fields, "window", "1280x720")
        && field_is(fields, "initial_slots",
            "draw_slash,storm_swords,none,none,none")
        && field_is(fields, "draw_accepted", "1")
        && field_is(fields, "draw_hit_count", "2")
        && field_is(fields, "storm_prelude_draw_hit_count", "2")
        && field_is(fields, "storm_accepted", "1")
        && field_is(fields, "storm_strike_hit_count", "3")
        && field_is(fields, "storm_finisher_hit_count", "0")
        && field_is(fields, "storm_strike_count", "12")
        && field_is(fields, "storm_sword_peak", "24")
        && field_is(fields, "storm_invulnerable_seen", "1")
        && field_is(fields, "storm_finisher_phase_seen", "1")
        && field_is(fields, "storm_aerial_captured", "1")
        && field_is(fields, "active_skill_atlases_ready", "1")
        && field_is_positive(fields, "storm_renderer_samples")
        && field_is(fields, "storm_material_frame_drawn", "1")
        && field_is(fields, "storm_base_player_drawn", "0")
        && field_is(fields, "storm_procedural_main_visual_peak", "0")
        && field_is(fields, "storm_renderer_status_valid", "1")
        && field_is(fields, "renderer_status_failure_latched", "0")
        && field_is(fields, "storm_center_locked", "1")
        && field_is(fields, "storm_isolation_invalidated", "0")
        && field_is(fields, "storm_player_moved", "1")
        && field_is(fields, "public_input_path", "1")
        && field_is(fields, "production_transactions", "0")
        && field_is(fields, "clean_shutdown_exact_ready", "1");
}

[[nodiscard]] bool restart_summary_valid(
    const std::map<std::string, std::string>& fields) noexcept {
    return field_is(fields, "scenario", "restarted_loadout")
        && field_is(fields, "result", "pass")
        && field_is(fields, "window", "1280x720")
        && field_is(fields, "initial_slots",
            "none,draw_slash,none,none,storm_swords")
        && field_is(fields, "restart_persisted", "1")
        && field_is(fields, "restart_cooldowns_zero", "1")
        && field_is(fields, "clean_shutdown_exact_ready", "1");
}

struct PureTimelineEvidence final {
    std::array<bool, 36> draw_frames{};
    std::array<bool, 24> storm_frames{};

    [[nodiscard]] bool draw_complete() const noexcept {
        return std::all_of(draw_frames.begin(), draw_frames.end(),
            [](bool seen) { return seen; });
    }

    [[nodiscard]] bool storm_complete() const noexcept {
        return std::all_of(storm_frames.begin(), storm_frames.end(),
            [](bool seen) { return seen; });
    }

    [[nodiscard]] bool valid() const noexcept {
        return draw_complete() && storm_complete()
            && draw_frames.front() && draw_frames.back()
            && storm_frames.front() && storm_frames.back();
    }
};

[[nodiscard]] PureTimelineEvidence pure_timeline_evidence() noexcept {
    PureTimelineEvidence evidence{};
    for (std::uint16_t tick{}; tick < 90U; ++tick) {
        const std::size_t frame = platform::active_skill_visual_frame_index(
            skills::ActiveSkillId::draw_slash, tick);
        if (frame < evidence.draw_frames.size()) evidence.draw_frames[frame] = true;
    }
    for (std::uint16_t tick{}; tick < 360U; ++tick) {
        const std::size_t frame = platform::active_skill_visual_frame_index(
            skills::ActiveSkillId::storm_swords, tick);
        if (frame < evidence.storm_frames.size()) evidence.storm_frames[frame] = true;
    }

    return evidence;
}

[[nodiscard]] bool write_final_state(const std::filesystem::path& run,
    const PureTimelineEvidence& timeline,
    const std::map<std::string, std::string>& production,
    const std::map<std::string, std::string>& storm) {
    static_assert(persistence::kLatestCheckpointFormatVersion == 10U);
    if (!timeline.valid()) return false;
    std::ofstream stream(run / "stage17-skill-stones-state.txt",
        std::ios::out | std::ios::trunc | std::ios::binary);
    stream << "schema=stage17-active-skill-rework-evidence-v3\n"
           << "result=PASS\n"
           << "renderer=raylib-6.0-opengl\n"
           << "window=1280x720\n"
           << "poe2_resolutions=800x450,1280x720,1920x1080\n"
           << "fixture_path=production-raylib-host\n"
           << "showcase_capture_count=12\n"
           << "legacy_capture_count=7\n"
           << "save_version=" << persistence::kLatestCheckpointFormatVersion << "\n"
           << "initial_slots=draw_slash,storm_swords,none,none,none\n"
           << "final_slots=none,draw_slash,none,none,storm_swords\n"
           << "restarted_slots=none,draw_slash,none,none,storm_swords\n"
           << "owned_active_bits=3\n"
           << "support_none_count=25\n"
           << "digit_1_cast=accepted\n"
           << "digit_2_cast=accepted\n"
           << "digit_3_5_effect=none\n"
           << "draw_windup_captured=true\n"
           << "draw_frame_peak=35\n"
           << "draw_slash_frame_union="
           << (timeline.draw_complete() ? "0..35" : "incomplete") << "\n"
           << "draw_slash_frame_zero="
           << (timeline.draw_frames.front() ? "true" : "false") << "\n"
           << "draw_slash_frame_last="
           << (timeline.draw_frames.back() ? "true" : "false") << "\n"
           << "draw_slash_hit_count=2\n"
           << "storm_strike_hit_count=3\n"
           << "storm_finisher_hit_count=0\n"
           << "storm_strike_count=12\n"
           << "storm_sword_peak=24\n"
           << "storm_swords_frame_union="
           << (timeline.storm_complete() ? "0..23" : "incomplete") << "\n"
           << "storm_swords_frame_zero="
           << (timeline.storm_frames.front() ? "true" : "false") << "\n"
           << "storm_swords_frame_last="
           << (timeline.storm_frames.back() ? "true" : "false") << "\n"
           << "storm_invulnerable_seen=true\n"
           << "storm_finisher_phase_seen=true\n"
           << "storm_aerial_captured=true\n"
           << "active_skill_atlases_ready=true\n"
           << "renderer_status_source="
              "800x450/production-summary-800x450.txt,"
              "1280x720/production-summary-1280x720.txt,"
              "1920x1080/production-summary-1920x1080.txt,"
              "1280x720/storm-summary.txt\n"
           << "draw_renderer_samples="
           << production.at("draw_renderer_samples") << "\n"
           << "draw_material_frame_drawn="
           << production.at("draw_material_frame_drawn") << "\n"
           << "draw_base_player_drawn="
           << production.at("draw_base_player_drawn") << "\n"
           << "draw_procedural_main_visual_peak="
           << production.at("draw_procedural_main_visual_peak") << "\n"
           << "draw_renderer_status_valid="
           << production.at("draw_renderer_status_valid") << "\n"
           << "storm_renderer_samples="
           << storm.at("storm_renderer_samples") << "\n"
           << "storm_material_frame_drawn="
           << storm.at("storm_material_frame_drawn") << "\n"
           << "storm_base_player_drawn="
           << storm.at("storm_base_player_drawn") << "\n"
           << "storm_procedural_main_visual_peak="
           << storm.at("storm_procedural_main_visual_peak") << "\n"
           << "storm_renderer_status_valid="
           << storm.at("storm_renderer_status_valid") << "\n"
           << "renderer_status_failure_latched="
           << production.at("renderer_status_failure_latched") << "\n"
           << "storm_center_locked=true\n"
           << "loadout_transactions=remove1,equip5,swap2_5\n"
           << "restart_persisted=true\n"
           << "cooldown_persisted=false\n"
           << "production_inventory_closed=true\n"
           << "production_cooldown_start_ticks="
           << production.at("production_cooldown_start_ticks") << "\n"
           << "production_cooldown_wait_ticks="
           << production.at("production_cooldown_wait_ticks") << "\n"
           << "cooldown_naturally_elapsed=true\n"
           << "clean_shutdown_exact_ready=true\n"
           << "public_input_path=true\n"
           << "production_transactions=true\n";
    return static_cast<bool>(stream);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 7 && argv[1] != nullptr
            && std::string{argv[1]} == "--scenario") {
        try {
            return run_host_child(std::filesystem::absolute(argv[3]), argv[2],
                std::stoull(argv[4]), {std::stoi(argv[5]),
                    std::stoi(argv[6])}) ? 0 : 1;
        } catch (...) {
            return 1;
        }
    }
    if (argc != 2 || argv[1] == nullptr) {
        std::cerr << "usage: arpg_stage17_skill_stones_game_validation "
                     "<evidence-root>\n";
        return 2;
    }
    g_executable = std::filesystem::absolute(argv[0]);
    std::filesystem::path run{};
    if (!prepare_evidence(std::filesystem::absolute(argv[1]), run)) return 3;
    {
        std::ofstream marker(run / "run.marker",
            std::ios::out | std::ios::trunc);
        marker << "stage17-run\n";
        if (!marker) return 4;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    constexpr std::uint64_t kProductionSeed = 753U;
    constexpr std::uint64_t kStormSeed = 4989U;
    std::array<std::map<std::string, std::string>,
        kPoe2Resolutions.size()> production_summaries{};
    for (std::size_t index = 0U; index < kPoe2Resolutions.size(); ++index) {
        const Resolution resolution = kPoe2Resolutions[index];
        if (!run_child(run, "production", kProductionSeed, resolution)) {
            return 5;
        }
        if (!production_images_valid(run, resolution)) return 6;
        const std::filesystem::path summary = resolution_run(run, resolution) /
            ("production-summary-" + resolution_name(resolution) + ".txt");
        production_summaries[index] = read_fields(summary);
        if (!production_summary_valid(production_summaries[index], resolution)) {
            std::cerr << "stage17 production summary rejected for "
                      << resolution_name(resolution) << '\n';
            return 7;
        }
    }
    constexpr Resolution kLegacyResolution{1280, 720};
    if (!run_child(run, "storm", kStormSeed, kLegacyResolution)) return 8;
    for (const char* name : {"04-storm-ground-array-1280x720.png",
             "05-storm-aerial-array-1280x720.png",
             "06-storm-finisher-1280x720.png"}) {
        if (!png_has_dimensions(run / name, kLegacyResolution)) return 9;
    }
    const auto storm = read_fields(resolution_run(run, kLegacyResolution)
        / "storm-summary.txt");
    if (!storm_summary_valid(storm)) {
        std::cerr << "stage17 storm summary rejected\n";
        return 10;
    }
    if (!run_child(run, "restart", kProductionSeed, kLegacyResolution)) {
        return 11;
    }
    if (!png_has_dimensions(run / "07-restarted-loadout-1280x720.png",
            kLegacyResolution)) {
        return 12;
    }
    const auto restart = read_fields(resolution_run(run, kLegacyResolution)
        / "restart-summary.txt");
    if (!restart_summary_valid(restart)) {
        std::cerr << "stage17 restart summary rejected\n";
        return 13;
    }
    const PureTimelineEvidence timeline = pure_timeline_evidence();
    if (!timeline.valid()) return 14;
    if (!write_final_state(run, timeline, production_summaries[1], storm)) {
        return 15;
    }
    std::cout << "stage17 raylib skill stones scenario=PASS\n";
    return 0;
}
