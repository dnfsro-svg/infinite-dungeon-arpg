#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_generation.hpp"
#include "raylib_host.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>

namespace {

namespace dungeon = arpg::dungeon;
namespace platform = arpg::platform;

struct SelectedRun final {
    std::uint64_t root{};
    dungeon::ExitDirection direction{dungeon::ExitDirection::none};
};

struct ScenarioSpec final {
    const char* argument{};
    const char* image{};
    const char* summary{};
    platform::Stage11CHudValidationScenario scenario{
        platform::Stage11CHudValidationScenario::none};
    std::uint32_t steps_per_frame{};
    std::uint32_t maximum_frames{};
};

constexpr std::array<ScenarioSpec, 6> kScenarios{{
    {"combat", "combat.png", "combat.txt",
        platform::Stage11CHudValidationScenario::normal_combat, 1U, 180U},
    {"low-health", "low-health.png", "low-health.txt",
        platform::Stage11CHudValidationScenario::low_health_status, 1U, 2400U},
    {"cleared", "cleared.png", "cleared.txt",
        platform::Stage11CHudValidationScenario::cleared_exit, 64U, 4096U},
    {"abyss-warning", "abyss-warning.png", "abyss-warning.txt",
        platform::Stage11CHudValidationScenario::abyss_abandon, 64U, 8192U},
    {"level-up", "level-up.png", "level-up.txt",
        platform::Stage11CHudValidationScenario::level_up_points, 64U, 4096U},
    {"debug", "debug.png", "debug.txt",
        platform::Stage11CHudValidationScenario::debug_overlay, 1U, 180U},
}};

std::filesystem::path g_executable_path{};

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
constexpr std::array<unsigned char, 6> kFnvFixture{{
    0x00U, 0x01U, 0x02U, 0x7FU, 0x80U, 0xFFU,
}};

[[nodiscard]] constexpr std::uint64_t hash_bytes(
    const unsigned char* bytes, std::size_t size) noexcept {
    std::uint64_t hash = kFnvOffset;
    for (std::size_t index{}; index < size; ++index) {
        hash ^= bytes[index];
        hash *= kFnvPrime;
    }
    return hash;
}

static_assert(hash_bytes(kFnvFixture.data(), kFnvFixture.size())
    == 12476124638988131554ULL,
    "Stage11C FNV-1a fixture must remain language-neutral");

[[nodiscard]] std::optional<SelectedRun> find_abyss_run() noexcept {
    constexpr std::array<dungeon::ExitDirection, 4> directions{{
        dungeon::ExitDirection::up,
        dungeon::ExitDirection::down,
        dungeon::ExitDirection::left,
        dungeon::ExitDirection::right,
    }};
    const dungeon::DungeonRules rules{};
    for (std::uint64_t root = 1U; root < 200000U; ++root) {
        const auto initial = dungeon::make_initial_run_state(root, rules);
        if (initial.fault != dungeon::DungeonFault::none) continue;
        const auto doors = dungeon::preview_abyss_doors(
            initial.state.current_room);
        for (std::size_t index = 0U; index < directions.size(); ++index) {
            if (!doors[index]) continue;
            const auto next = dungeon::make_door_transition(
                initial.state, directions[index], rules);
            if (next.fault == dungeon::DungeonFault::none
                    && next.state.current_room.is_abyss) {
                return SelectedRun{root, directions[index]};
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] const ScenarioSpec* scenario_from_argument(
    const std::string& argument) noexcept {
    for (const ScenarioSpec& spec : kScenarios) {
        if (argument == spec.argument) return &spec;
    }
    return nullptr;
}

[[nodiscard]] std::string quote_argument(
    const std::filesystem::path& value) {
    std::string result{"\""};
    for (const char character : value.string()) {
        if (character == '\"') result += '\\';
        result += character;
    }
    result += '\"';
    return result;
}

[[nodiscard]] std::uint64_t hash_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::uint64_t hash = kFnvOffset;
    char byte{};
    while (stream.get(byte)) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= kFnvPrime;
    }
    return hash;
}

[[nodiscard]] std::map<std::string, std::string> read_summary(
    const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::map<std::string, std::string> values{};
    std::string line{};
    while (std::getline(stream, line)) {
        const std::size_t separator = line.find('=');
        if (separator != std::string::npos) {
            values.emplace(line.substr(0U, separator),
                line.substr(separator + 1U));
        }
    }
    return values;
}

[[nodiscard]] bool field_equals(
    const std::map<std::string, std::string>& values,
    const char* key, const char* expected) {
    const auto found = values.find(key);
    return found != values.end() && found->second == expected;
}

[[nodiscard]] bool field_nonzero(
    const std::map<std::string, std::string>& values, const char* key) {
    const auto found = values.find(key);
    return found != values.end() && !found->second.empty()
        && found->second != "0";
}

[[nodiscard]] const std::string& field_or_empty(
    const std::map<std::string, std::string>& values, const char* key) {
    static const std::string empty{};
    const auto found = values.find(key);
    return found == values.end() ? empty : found->second;
}

[[nodiscard]] bool fresh_nonempty_file(const std::filesystem::path& path,
    std::filesystem::file_time_type earliest, std::uintmax_t minimum_size) {
    std::error_code error{};
    const auto size = std::filesystem::file_size(path, error);
    if (error || size <= minimum_size) return false;
    const auto modified = std::filesystem::last_write_time(path, error);
    return !error && modified >= earliest;
}

[[nodiscard]] bool run_host_once(const std::filesystem::path& root,
    const ScenarioSpec& spec, SelectedRun selected) {
    const auto started = std::filesystem::file_time_type::clock::now()
        - std::chrono::seconds(2);
    const std::filesystem::path scenario_root = root / spec.argument;
    std::error_code error{};
    std::filesystem::create_directories(scenario_root / "saves", error);
    if (error) return false;
    platform::RaylibHostConfig config{};
    config.window_width = 1280;
    config.window_height = 720;
    config.window_title = "Stage11C Production HUD Formal Validation";
    config.save_directory = scenario_root / "saves";
    config.settings_directory = scenario_root / "settings";
    config.new_run_seed = selected.root;
    config.stage11c_hud_validation = spec.scenario;
    config.validation_abyss_direction = static_cast<std::uint8_t>(
        selected.direction);
    config.validation_steps_per_frame = spec.steps_per_frame;
    config.validation_exit_after_presented_frames = spec.maximum_frames;
    config.validation_capture_file = root / spec.image;
    config.validation_summary_file = root / spec.summary;
    const auto result = platform::run_raylib_host(config);
    return result == platform::HostExitCode::success
        && fresh_nonempty_file(*config.validation_capture_file, started, 1024U)
        && fresh_nonempty_file(*config.validation_summary_file, started, 64U);
}

[[nodiscard]] bool run_child(const std::filesystem::path& root,
    const ScenarioSpec& spec, SelectedRun selected) {
    const std::filesystem::path command_file = root
        / (std::string{"stage11c-child-"} + spec.argument + ".cmd");
    std::ofstream stream(command_file, std::ios::out | std::ios::trunc);
    stream << "@echo off\r\n" << quote_argument(g_executable_path)
           << " --scenario " << spec.argument << ' '
           << quote_argument(root) << ' ' << selected.root << ' '
           << static_cast<unsigned>(selected.direction) << "\r\n";
    if (!stream) return false;
    stream.close();
    const std::string command = "call " + quote_argument(command_file);
    return std::system(command.c_str()) == 0;
}

[[nodiscard]] bool summary_has_required_fields(
    const std::map<std::string, std::string>& values,
    const ScenarioSpec& spec) {
    constexpr std::array<const char*, 18> required{{
        "scenario", "safe_rect", "player_rect", "objective_rect",
        "navigation_rect", "primary_notice_rect", "secondary_notice_rect",
        "debug_rect", "player_values", "status_tags", "objective",
        "notice_kinds", "notice_texts", "navigation_values", "font_ready", "f1",
        "production_snapshot_hash", "result",
    }};
    for (const char* key : required) {
        if (values.find(key) == values.end()) return false;
    }
    const bool expect_debug = spec.scenario
        == platform::Stage11CHudValidationScenario::debug_overlay;
    return field_equals(values, "result", "pass")
        && field_equals(values, "font_ready", "1")
        && field_equals(values, "f1", expect_debug ? "1" : "0")
        && field_nonzero(values, "production_snapshot_hash")
        && !field_or_empty(values, "objective").empty();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 6 && argv[1] != nullptr
            && std::string{argv[1]} == "--scenario") {
        const ScenarioSpec* const spec = scenario_from_argument(argv[2]);
        if (spec == nullptr) return 2;
        const SelectedRun selected{
            std::stoull(argv[4]),
            static_cast<dungeon::ExitDirection>(std::stoul(argv[5]))};
        return run_host_once(argv[3], *spec, selected) ? 0 : 1;
    }
    if (argc != 2 || argv[1] == nullptr) {
        std::cerr << "usage: arpg_stage11c_hud_formal <evidence-directory>\n";
        return 2;
    }
    g_executable_path = std::filesystem::absolute(argv[0]);
    const std::filesystem::path root = std::filesystem::absolute(argv[1]);
    std::error_code error{};
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    if (error) return 3;
    const auto selected = find_abyss_run();
    if (!selected.has_value()) return 4;

    bool ok = true;
    for (const ScenarioSpec& spec : kScenarios) {
        std::cout << "STAGE11C HUD START " << spec.argument << std::endl;
        const bool child_ok = run_child(root, spec, *selected);
        std::cout << "STAGE11C HUD " << (child_ok ? "PASS " : "FAIL ")
                  << spec.argument << std::endl;
        ok = child_ok && ok;
    }
    std::ofstream report(root / "stage11c-hud-evidence.txt",
        std::ios::out | std::ios::trunc);
    report << "fnv1a_fixture_hash="
           << hash_bytes(kFnvFixture.data(), kFnvFixture.size()) << '\n';
    for (const ScenarioSpec& spec : kScenarios) {
        const auto values = read_summary(root / spec.summary);
        const bool valid = summary_has_required_fields(values, spec);
        ok = valid && ok;
        report << spec.argument << "_image_hash="
               << hash_file(root / spec.image) << '\n'
               << spec.argument << "_snapshot_hash="
               << field_or_empty(values, "production_snapshot_hash") << '\n'
               << spec.argument << "_valid=" << (valid ? 1 : 0) << '\n';
    }
    report << "result=" << (ok ? "pass" : "fail") << '\n';
    return ok && report ? 0 : 1;
}
