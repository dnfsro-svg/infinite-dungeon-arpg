#include "raylib_host.hpp"

#include "dungeon_runtime.hpp"

#include "persistence/room_progress_codec.hpp"
#include "persistence/save_store.hpp"
#include "platform/settings/settings_store.hpp"
#include "platform/settings/settings_types.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace platform = arpg::platform;
namespace persistence = arpg::persistence;
namespace settings = arpg::settings;

std::filesystem::path g_executable_path{};

[[nodiscard]] std::uint64_t hash_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::uint64_t hash = 1469598103934665603ULL;
    char byte{};
    while (stream.get(byte)) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

struct FileFingerprint final {
    std::uint64_t hash{};
    std::uintmax_t size{};
    bool valid{};
};

[[nodiscard]] FileFingerprint fingerprint_file(const std::filesystem::path& path) {
    std::error_code error{};
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    return !error && size != 0U ? FileFingerprint{hash_file(path), size, true}
                                : FileFingerprint{};
}

[[nodiscard]] bool same_fingerprint(
    const FileFingerprint& lhs, const FileFingerprint& rhs) noexcept {
    return lhs.valid && rhs.valid && lhs.hash == rhs.hash && lhs.size == rhs.size;
}

[[nodiscard]] bool establish_character_slots(
    const std::filesystem::path& directory) {
    platform::DungeonRuntimeConfig config{};
    config.save.directory = directory;
    config.new_run_seed = 0x11B11BULL;
    platform::DungeonRuntime runtime(config);
    if (!runtime.initialize() || runtime.state() != platform::DungeonRuntimeState::running) {
        return false;
    }
    persistence::SaveStore store({directory});
    const persistence::SaveLoadResult loaded = store.load();
    if (loaded.state != persistence::SaveLoadState::ready
            || loaded.checkpoint.commit_generation == UINT64_MAX) {
        return false;
    }
    auto second_generation = loaded.checkpoint;
    ++second_generation.commit_generation;
    return store.commit(second_generation).state
        == persistence::SaveCommitState::committed
        && fingerprint_file(directory / "run_a.sav").valid
        && fingerprint_file(directory / "run_b.sav").valid;
}

[[nodiscard]] bool valid_current_character_slot(
    const std::filesystem::path& path, std::uint64_t& revision) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return false;
    const std::streampos end = stream.tellg();
    if (end <= 0) return false;
    const auto size = static_cast<std::size_t>(end);
    std::vector<std::uint8_t> bytes(size);
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return stream && persistence::inspect_checkpoint_latest_envelope(
        bytes.data(), bytes.size(), revision) == persistence::CodecError::none
        && revision != 0U;
}

[[nodiscard]] std::string quote_command_argument(
    const std::filesystem::path& value) {
    std::string quoted{"\""};
    for (const char character : value.string()) {
        if (character == '\"') quoted += '\\';
        quoted += character;
    }
    quoted += '\"';
    return quoted;
}

[[nodiscard]] std::map<std::string, std::string> read_summary(
    const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::map<std::string, std::string> values{};
    std::string line{};
    while (std::getline(stream, line)) {
        const std::size_t separator = line.find('=');
        if (separator != std::string::npos) {
            values.emplace(line.substr(0U, separator), line.substr(separator + 1U));
        }
    }
    return values;
}

[[nodiscard]] bool capture_is_fresh(const std::filesystem::path& path) {
    std::error_code error{};
    return std::filesystem::exists(path, error) && !error
        && std::filesystem::file_size(path, error) > 1024U && !error;
}

[[nodiscard]] bool run_host_once(const std::filesystem::path& root,
    const std::filesystem::path& save_directory,
    const std::filesystem::path& settings_directory,
    platform::Stage11BValidationScenario scenario,
    const char* name) {
    platform::RaylibHostConfig config{};
    config.window_width = 1280;
    config.window_height = 720;
    config.window_title = "Stage11B Formal Validation";
    config.save_directory = save_directory;
    config.settings_directory = settings_directory;
    config.new_run_seed = 0x11B11BULL;
    config.stage11b_validation = scenario;
    config.validation_steps_per_frame = 1U;
    config.validation_exit_after_presented_frames = 180U;
    config.validation_capture_file = root / (std::string{name} + ".png");
    config.validation_summary_file = root / (std::string{name} + ".txt");
    return platform::run_raylib_host(config) == platform::HostExitCode::success
        && capture_is_fresh(*config.validation_capture_file);
}

[[nodiscard]] std::optional<platform::Stage11BValidationScenario> scenario_from_name(
    const std::string& name) {
    using Scenario = platform::Stage11BValidationScenario;
    if (name == "pause") return Scenario::paused_freeze;
    if (name == "settings") return Scenario::settings_page;
    if (name == "rebound") return Scenario::rebound_attack;
    if (name == "swap") return Scenario::conflict_swap;
    if (name == "restart") return Scenario::restarted_settings;
    if (name == "single-slot") return Scenario::single_slot_recovery;
    if (name == "corrupt") return Scenario::corrupt_defaults;
    return std::nullopt;
}

[[nodiscard]] bool run_child(const std::filesystem::path& root,
    const std::filesystem::path& save_directory,
    const std::filesystem::path& settings_directory, const char* name) {
    const std::filesystem::path script = root
        / (std::string{"stage11b-child-"} + name + ".cmd");
    std::ofstream stream(script, std::ios::out | std::ios::trunc);
    stream << "@echo off\r\n" << quote_command_argument(g_executable_path)
        << " --scenario " << quote_command_argument(name) << " "
        << quote_command_argument(root) << " "
        << quote_command_argument(save_directory) << " "
        << quote_command_argument(settings_directory) << "\r\n";
    if (!stream) return false;
    stream.close();
    const std::string command = "call " + quote_command_argument(script);
    return std::system(command.c_str()) == 0;
}

[[nodiscard]] bool save_second_slot(const std::filesystem::path& directory) {
    settings::SettingsStore store(directory);
    const auto loaded = store.load();
    settings::SettingsData draft = loaded.settings;
    draft.master_sfx_percent = draft.master_sfx_percent == 100U ? 95U : 100U;
    return store.save(loaded.settings, draft).status
        == settings::SettingsSaveStatus::committed;
}

[[nodiscard]] bool write_corrupt_settings(const std::filesystem::path& directory) {
    std::error_code error{};
    std::filesystem::create_directories(directory, error);
    if (error) return false;
    for (const char* name : {"settings-a.bin", "settings-b.bin"}) {
        std::ofstream stream(directory / name, std::ios::binary | std::ios::trunc);
        stream << "not a settings slot";
        if (!stream) return false;
    }
    return true;
}

[[nodiscard]] bool equals(const std::map<std::string, std::string>& values,
    const char* key, const char* expected) {
    const auto found = values.find(key);
    return found != values.end() && found->second == expected;
}

[[nodiscard]] bool nonzero(const std::map<std::string, std::string>& values,
    const char* key) {
    const auto found = values.find(key);
    return found != values.end() && found->second != "0" && !found->second.empty();
}

[[nodiscard]] const std::string& value_or_empty(
    const std::map<std::string, std::string>& values, const char* key) {
    static const std::string empty{};
    const auto found = values.find(key);
    return found == values.end() ? empty : found->second;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 6 && argv[1] != nullptr && std::string{argv[1]} == "--scenario") {
        const auto scenario = scenario_from_name(argv[2]);
        if (!scenario.has_value()) return 2;
        return run_host_once(argv[3], argv[4], argv[5], *scenario, argv[2]) ? 0 : 1;
    }
    if (argc != 2 || argv[1] == nullptr) {
        std::cerr << "usage: arpg_stage11b_settings_formal <evidence-directory>\n";
        return 2;
    }
    g_executable_path = std::filesystem::absolute(argv[0]);
    const std::filesystem::path root = std::filesystem::absolute(argv[1]);
    const std::filesystem::path saves = root / "saves";
    const std::filesystem::path main_settings = root / "settings-main";
    std::error_code error{};
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(saves, error);
    if (error) return 3;
    if (!establish_character_slots(saves)) return 3;

    bool ok = run_child(root, saves, main_settings, "rebound");
    const auto rebound = read_summary(root / "rebound.txt");
    ok = ok && equals(rebound, "old_attack_count", "0")
        && nonzero(rebound, "new_attack_count")
        && equals(rebound, "light_attack", "U");
    const FileFingerprint run_a_before = fingerprint_file(saves / "run_a.sav");
    const FileFingerprint run_b_before = fingerprint_file(saves / "run_b.sav");
    const bool settings_slot_saved = save_second_slot(main_settings);
    const FileFingerprint run_a_after = fingerprint_file(saves / "run_a.sav");
    const FileFingerprint run_b_after = fingerprint_file(saves / "run_b.sav");
    ok = ok && settings_slot_saved
        && same_fingerprint(run_a_before, run_a_after)
        && same_fingerprint(run_b_before, run_b_after);

    ok = ok && run_child(root, saves, main_settings, "restart");
    const auto restart = read_summary(root / "restart.txt");
    ok = ok && equals(restart, "light_attack", "U");

    std::filesystem::remove(main_settings / "settings-a.bin", error);
    ok = ok && !error && run_child(root, saves, main_settings, "single-slot");
    const auto single_slot = read_summary(root / "single-slot.txt");
    ok = ok && equals(single_slot, "load_status", "2")
        && equals(single_slot, "light_attack", "U");

    const auto corrupt_settings = root / "settings-corrupt";
    ok = ok && write_corrupt_settings(corrupt_settings)
        && run_child(root, saves, corrupt_settings, "corrupt");
    const auto corrupt = read_summary(root / "corrupt.txt");
    ok = ok && equals(corrupt, "load_status", "3")
        && equals(corrupt, "light_attack", "J")
        && equals(corrupt, "recovery_notice_visible", "1");

    ok = ok && run_child(root, saves, root / "settings-pause", "pause");
    const auto pause = read_summary(root / "pause.txt");
    ok = ok && equals(pause, "paused_tick_before",
        value_or_empty(pause, "paused_tick_after").c_str())
        && nonzero(pause, "paused_tick_before")
        && equals(pause, "pause_capture_while_paused", "1")
        && value_or_empty(pause, "resume_tick_after")
            == std::to_string(std::stoull(value_or_empty(pause,
                "resume_tick_before")) + 1U)
        && equals(pause, "player_monster_hash_before",
            value_or_empty(pause, "player_monster_hash_after").c_str())
        && nonzero(pause, "player_monster_hash_before");

    ok = ok && run_child(root, saves, root / "settings-page", "settings");
    ok = ok && run_child(root, saves, root / "settings-swap", "swap");
    const auto swap = read_summary(root / "swap.txt");
    ok = ok && equals(swap, "light_attack", "K") && equals(swap, "jump", "J");

    std::uint64_t final_run_a_revision{};
    std::uint64_t final_run_b_revision{};
    const bool final_run_a_checkpoint_valid = valid_current_character_slot(
        saves / "run_a.sav", final_run_a_revision);
    const bool final_run_b_checkpoint_valid = valid_current_character_slot(
        saves / "run_b.sav", final_run_b_revision);
    ok = ok && final_run_a_checkpoint_valid
        && final_run_b_checkpoint_valid;
    std::ofstream report(root / "stage11b-settings-evidence.txt", std::ios::trunc);
    report << "character_hash_scope=settings_transaction\n"
           << "run_a_hash_before=" << run_a_before.hash << '\n'
           << "run_a_hash_after=" << run_a_after.hash << '\n'
           << "run_b_hash_before=" << run_b_before.hash << '\n'
           << "run_b_hash_after=" << run_b_after.hash << '\n'
           << "run_a_size_before=" << run_a_before.size << '\n'
           << "run_a_size_after=" << run_a_after.size << '\n'
           << "run_b_size_before=" << run_b_before.size << '\n'
           << "run_b_size_after=" << run_b_after.size << '\n'
           << "final_run_a_checkpoint_valid="
           << (final_run_a_checkpoint_valid ? 1 : 0) << '\n'
           << "final_run_b_checkpoint_valid="
           << (final_run_b_checkpoint_valid ? 1 : 0) << '\n'
           << "final_run_a_revision=" << final_run_a_revision << '\n'
           << "final_run_b_revision=" << final_run_b_revision << '\n'
           << "rebound_old_attack=" << value_or_empty(rebound, "old_attack_count") << '\n'
           << "rebound_new_attack=" << value_or_empty(rebound, "new_attack_count") << '\n'
           << "paused_tick_before=" << value_or_empty(pause, "paused_tick_before") << '\n'
           << "paused_tick_after=" << value_or_empty(pause, "paused_tick_after") << '\n'
           << "pause_capture_while_paused="
           << value_or_empty(pause, "pause_capture_while_paused") << '\n'
           << "resume_tick_before=" << value_or_empty(pause, "resume_tick_before") << '\n'
           << "resume_tick_after=" << value_or_empty(pause, "resume_tick_after") << '\n'
           << "player_monster_hash_before=" << value_or_empty(pause, "player_monster_hash_before") << '\n'
           << "player_monster_hash_after=" << value_or_empty(pause, "player_monster_hash_after") << '\n'
           << "committed_revision=" << value_or_empty(rebound, "committed_revision") << '\n'
           << "restart_binding=" << value_or_empty(restart, "light_attack") << '\n'
           << "single_slot_status=" << value_or_empty(single_slot, "load_status") << '\n'
           << "corrupt_default_status=" << value_or_empty(corrupt, "load_status") << '\n'
           << "corrupt_notice_visible="
           << value_or_empty(corrupt, "recovery_notice_visible") << '\n'
           << "swap_pair=" << value_or_empty(swap, "light_attack") << "/" << value_or_empty(swap, "jump") << '\n'
           << "result=" << (ok ? "pass" : "fail") << '\n';
    return ok && report ? 0 : 1;
}
