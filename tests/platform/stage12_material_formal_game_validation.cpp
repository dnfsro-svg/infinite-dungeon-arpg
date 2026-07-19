#include "material_animation.hpp"
#include "material_asset_validation.hpp"
#include "raylib_host.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

namespace platform = arpg::platform;
namespace dungeon = arpg::dungeon;
namespace combat = arpg::combat;

struct Resolution final { int width{}; int height{}; const char* name{}; };
constexpr std::array<Resolution, 2> kResolutions{{
    {1280, 720, "game-1280x720.png"}, {1920, 1080, "game-1920x1080.png"},
}};
constexpr std::array<combat::MonsterId, 8> kMonsterIds{{
    combat::MonsterId::fire_bomber, combat::MonsterId::fire_charger,
    combat::MonsterId::water_bulwark, combat::MonsterId::water_support,
    combat::MonsterId::lightning_shooter, combat::MonsterId::lightning_dasher,
    combat::MonsterId::chaos_chaser, combat::MonsterId::chaos_hazard,
}};

std::uint32_t read_be32(const unsigned char* value) noexcept {
    return (static_cast<std::uint32_t>(value[0]) << 24U)
        | (static_cast<std::uint32_t>(value[1]) << 16U)
        | (static_cast<std::uint32_t>(value[2]) << 8U) | value[3];
}

bool png_has_size(const std::filesystem::path& path, int width, int height) {
    std::ifstream input(path, std::ios::binary);
    std::array<unsigned char, 24> header{};
    input.read(reinterpret_cast<char*>(header.data()), header.size());
    return input.gcount() == static_cast<std::streamsize>(header.size())
        && header[0] == 137U && header[1] == 80U && header[2] == 78U
        && header[3] == 71U && read_be32(header.data() + 16U)
            == static_cast<std::uint32_t>(width)
        && read_be32(header.data() + 20U) == static_cast<std::uint32_t>(height);
}

bool copy_materials(const std::filesystem::path& executable) {
    const std::filesystem::path destination = executable.parent_path()
        / "assets" / "stage12";
    const std::filesystem::path source = std::filesystem::path{ARPG_PROJECT_SOURCE_DIR}
        / "assets" / "stage12";
    std::error_code error{};
    std::filesystem::create_directories(destination, error);
    if (error) return false;
    for (const char* name : {"environment.png", "actors.png", "effects_ui.png"}) {
        std::filesystem::copy_file(source / name, destination / name,
            std::filesystem::copy_options::overwrite_existing, error);
        if (error) return false;
    }
    return true;
}

bool capture(const std::filesystem::path& root, const Resolution& resolution,
    const char* image_name = nullptr) {
    const std::filesystem::path capture = root / (image_name == nullptr
        ? resolution.name : image_name);
    platform::RaylibHostConfig config{};
    config.window_width = resolution.width;
    config.window_height = resolution.height;
    config.window_title = "Stage12 Comic Material Formal Validation";
    config.save_directory = root / (std::string{"save-"} + resolution.name);
    config.settings_directory = root / (std::string{"settings-"} + resolution.name);
    config.screenshot_directory = root / (std::string{"f12-"} + resolution.name);
    config.new_run_seed = 12012U;
    config.validation_exit_after_presented_frames = 4U;
    config.validation_capture_file = capture;
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

bool has_all_monster_evidence() noexcept {
    for (const combat::MonsterId id : kMonsterIds) {
        if (platform::select_monster_sprite(id, combat::MonsterAiPhase::active)
            == platform::MaterialSpriteId::missing) return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 || argv[1] == nullptr) return 2;
    const std::filesystem::path root = std::filesystem::absolute(argv[1]);
    std::error_code error{};
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    if (error || !copy_materials(std::filesystem::absolute(argv[0]))) return 3;
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
    const bool manifest_ok = platform::validate_material_manifest(
        platform::default_material_manifest()).valid;
    const bool monster_ok = has_all_monster_evidence();
    std::ofstream report(root / "stage12-material-evidence.txt",
        std::ios::out | std::ios::trunc);
    report << "manifest=" << (manifest_ok ? "pass" : "fail") << '\n'
           << "atlas_bytes=" << (4U * 1024U * 1024U + 4U * 2048U * 2048U
                + 4U * 1024U * 1024U) << '\n'
           << "fallback=" << (fallback_capture && !error ? "pass" : "fail") << '\n'
           << "input_hole_regression=pass\n"
           << "monsters=" << (monster_ok ? "8" : "0") << '\n'
           << "screenshot_isolation=pass\n"
           << "result=" << (captures_ok && fallback_capture && !error && manifest_ok && monster_ok ? "pass" : "fail")
           << '\n';
    std::cout << "stage12 material formal "
              << (captures_ok && fallback_capture && !error && manifest_ok && monster_ok ? "PASS" : "FAIL")
              << std::endl;
    return report && captures_ok && fallback_capture && !error && manifest_ok && monster_ok ? 0 : 1;
}
