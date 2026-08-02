#include "raylib_host.hpp"

#include <chrono>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) return 2;
    const std::filesystem::path executable = std::filesystem::absolute(argv[0]);
    const std::filesystem::path validation_save_directory =
        executable.parent_path() / "stage9-formal-game-validation";
    std::error_code directory_error;
    std::filesystem::create_directories(validation_save_directory, directory_error);
    if (directory_error) return 3;

    std::filesystem::path validation_run_directory{};
    const auto run_stamp = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    for (unsigned attempt = 0U; attempt < 128U; ++attempt) {
        const std::filesystem::path candidate = validation_save_directory
            / ("run-" + std::to_string(run_stamp) + "-"
                + std::to_string(attempt));
        std::error_code create_error;
        if (std::filesystem::create_directory(candidate, create_error)) {
            validation_run_directory = candidate;
            break;
        }
        if (create_error) return 4;
    }
    if (validation_run_directory.empty()) return 5;

    arpg::platform::RaylibHostConfig config{};
    config.window_title = "Infinite Dungeon - Stage 9 Formal Game Validation";
    config.save_directory = validation_run_directory;
    config.settings_directory = validation_run_directory;
    config.new_run_seed = 2U;
    // Keep the original 60-presented-frame evidence cadence, but write to the
    // caller-owned path while gameplay persistence stays in the fresh run
    // directory above.  Reusing the evidence directory as a save directory
    // made later invocations restore damage or death from earlier invocations.
    config.validation_capture_file = validation_save_directory
        / "stage8-validation-001.png";
    config.validation_exit_after_presented_frames = 60U;
    const int result = static_cast<int>(arpg::platform::run_raylib_host(config));
    std::error_code cleanup_error;
    std::filesystem::remove_all(validation_run_directory, cleanup_error);
    return result;
}
