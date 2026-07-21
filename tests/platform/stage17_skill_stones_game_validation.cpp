#include "persistence/checkpoint_codec.hpp"
#include "raylib_host.hpp"

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

std::filesystem::path g_executable{};

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

[[nodiscard]] bool run_child(const std::filesystem::path& run,
    const char* scenario, std::uint64_t seed) {
    const std::filesystem::path command = run /
        (std::string{"stage17-"} + scenario + ".cmd");
    std::ofstream stream(command, std::ios::out | std::ios::trunc);
    stream << "@echo off\r\n" << quote(g_executable)
           << " --scenario " << scenario << ' ' << quote(run)
           << ' ' << seed << "\r\n";
    if (!stream) return false;
    stream.close();
    return std::system(("call " + quote(command)).c_str()) == 0;
}

[[nodiscard]] bool run_host_child(const std::filesystem::path& run,
    const std::string& scenario, std::uint64_t seed) noexcept {
    platform::RaylibHostConfig config{};
    config.window_width = 1280;
    config.window_height = 720;
    config.window_title = "Stage 17 Skill Stones Production Validation";
    config.save_directory = run / "save";
    config.settings_directory = run / "settings";
    config.screenshot_directory = run;
    config.new_run_seed = seed;
    config.validation_steps_per_frame = 1U;
    config.validation_exit_after_presented_frames =
        scenario == "production" ? 900U : 300U;
    if (scenario == "production") {
        config.stage17_skill_stones_validation =
            platform::Stage17SkillStonesValidationScenario::production_sequence;
        config.validation_summary_file = run / "production-summary.txt";
    } else if (scenario == "restart") {
        config.stage17_skill_stones_validation =
            platform::Stage17SkillStonesValidationScenario::restarted_loadout;
        config.validation_summary_file = run / "restart-summary.txt";
    } else {
        return false;
    }
    return platform::run_raylib_host(config) == platform::HostExitCode::success
        && regular_nonempty(*config.validation_summary_file);
}

[[nodiscard]] bool production_summary_valid(
    const std::map<std::string, std::string>& fields) noexcept {
    return field_is(fields, "scenario", "production_sequence")
        && field_is(fields, "result", "pass")
        && field_is(fields, "initial_slots",
            "draw_slash,storm_swords,none,none,none")
        && field_is(fields, "final_slots",
            "none,draw_slash,none,none,storm_swords")
        && field_is(fields, "owned_active_bits", "3")
        && field_is(fields, "support_none_count", "25")
        && field_is(fields, "empty_slots_none", "1")
        && field_is(fields, "draw_accepted", "1")
        && field_is(fields, "draw_hit_count", "2")
        && field_is(fields, "storm_accepted", "1")
        && field_is(fields, "storm_strike_hit_count", "3")
        && field_is(fields, "storm_finisher_hit_count", "1")
        && field_is(fields, "storm_strike_count", "12")
        && field_is(fields, "storm_center_locked", "1")
        && field_is(fields, "storm_player_moved", "1")
        && field_is(fields, "public_input_path", "1")
        && field_is(fields, "production_transactions", "1");
}

[[nodiscard]] bool restart_summary_valid(
    const std::map<std::string, std::string>& fields) noexcept {
    return field_is(fields, "scenario", "restarted_loadout")
        && field_is(fields, "result", "pass")
        && field_is(fields, "initial_slots",
            "none,draw_slash,none,none,storm_swords")
        && field_is(fields, "restart_persisted", "1")
        && field_is(fields, "restart_cooldowns_zero", "1");
}

[[nodiscard]] bool write_final_state(const std::filesystem::path& run) {
    static_assert(persistence::kCheckpointFormatVersion == 8U);
    std::ofstream stream(run / "stage17-skill-stones-state.txt",
        std::ios::out | std::ios::trunc | std::ios::binary);
    stream << "schema=stage17-skill-stones-evidence-v1\n"
           << "result=PASS\n"
           << "renderer=raylib-6.0-opengl\n"
           << "window=1280x720\n"
           << "save_version=" << persistence::kCheckpointFormatVersion << "\n"
           << "initial_slots=draw_slash,storm_swords,none,none,none\n"
           << "final_slots=none,draw_slash,none,none,storm_swords\n"
           << "restarted_slots=none,draw_slash,none,none,storm_swords\n"
           << "owned_active_bits=3\n"
           << "support_none_count=25\n"
           << "digit_1_cast=accepted\n"
           << "digit_2_cast=accepted\n"
           << "digit_3_5_effect=none\n"
           << "draw_slash_hit_count=2\n"
           << "storm_strike_hit_count=3\n"
           << "storm_finisher_hit_count=1\n"
           << "storm_strike_count=12\n"
           << "storm_center_locked=true\n"
           << "loadout_transactions=remove1,equip5,swap2_5\n"
           << "restart_persisted=true\n"
           << "cooldown_persisted=false\n"
           << "public_input_path=true\n"
           << "production_transactions=true\n";
    return static_cast<bool>(stream);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 5 && argv[1] != nullptr
            && std::string{argv[1]} == "--scenario") {
        return run_host_child(std::filesystem::absolute(argv[3]), argv[2],
            std::stoull(argv[4])) ? 0 : 1;
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

    constexpr std::uint64_t kProductionSeed = 170017U;
    if (!run_child(run, "production", kProductionSeed)) return 5;
    const auto production = read_fields(run / "production-summary.txt");
    if (!production_summary_valid(production)) {
        std::cerr << "stage17 production summary rejected\n";
        return 6;
    }
    if (!run_child(run, "restart", kProductionSeed)) return 7;
    const auto restart = read_fields(run / "restart-summary.txt");
    if (!restart_summary_valid(restart)) {
        std::cerr << "stage17 restart summary rejected\n";
        return 8;
    }
    if (!write_final_state(run)) return 9;
    std::cout << "stage17 raylib skill stones scenario=PASS\n";
    return 0;
}
