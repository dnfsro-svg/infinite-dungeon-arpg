#include "raylib_host.hpp"

#include <filesystem>

int main(int argc, char** argv) {
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) return 2;
    try {
        const std::filesystem::path executable =
            std::filesystem::absolute(argv[0]);
        arpg::platform::RaylibHostConfig config{};
        config.window_title = "Infinite Dungeon - Stage 8 Validation";
        config.save_directory = executable.parent_path().parent_path()
            / "stage8-window-validation";
        config.validation_capture = true;
        return static_cast<int>(arpg::platform::run_raylib_host(config));
    } catch (...) {
        return 3;
    }
}
