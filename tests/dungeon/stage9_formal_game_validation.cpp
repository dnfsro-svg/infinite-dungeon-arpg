#include "raylib_host.hpp"

#include <filesystem>

int main(int argc, char** argv) {
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) return 2;
    const std::filesystem::path executable = std::filesystem::absolute(argv[0]);
    const std::filesystem::path validation_save_directory =
        executable.parent_path() / "stage9-formal-game-validation";
    std::error_code directory_error;
    std::filesystem::create_directories(validation_save_directory, directory_error);
    if (directory_error) return 3;

    arpg::platform::RaylibHostConfig config{};
    config.window_title = "Infinite Dungeon - Stage 9 Formal Game Validation";
    config.save_directory = validation_save_directory;
    config.new_run_seed = 2U;
    config.validation_capture = true;
    config.validation_exit_after_presented_frames = 60U;
    return static_cast<int>(arpg::platform::run_raylib_host(config));
}
