#include "launcher_core.hpp"

#include <array>
#include <system_error>

namespace arpg::launcher {

namespace {

inline constexpr std::array<std::wstring_view, 6U> kRequiredAssetDirectories{{
    L"assets/fonts",
    L"assets/player",
    L"assets/skills",
    L"assets/stage12",
    L"assets/stage14/audio",
    L"assets/stage15/audio",
}};

InstallationStatus make_status(
    InstallationState state,
    const std::filesystem::path& application_directory,
    const std::filesystem::path& game_executable = {},
    const std::filesystem::path& missing_path = {}) noexcept {
    return {state, application_directory, game_executable, missing_path};
}

}  // namespace

bool InstallationStatus::ready() const noexcept {
    return state == InstallationState::ready;
}

InstallationStatus inspect_installation(
    const std::filesystem::path& launcher_executable) noexcept {
    try {
        const std::filesystem::path application_directory =
            launcher_executable.parent_path();
        std::error_code error;
        if (application_directory.empty() ||
            !std::filesystem::is_directory(application_directory, error)) {
            return make_status(
                InstallationState::invalid_launcher_path,
                application_directory,
                {},
                launcher_executable);
        }

        const std::filesystem::path game_executable =
            application_directory / L"arpg_game.exe";
        error.clear();
        if (!std::filesystem::is_regular_file(game_executable, error)) {
            return make_status(
                InstallationState::missing_game_executable,
                application_directory,
                game_executable,
                game_executable);
        }

        for (const auto directory : kRequiredAssetDirectories) {
            const std::filesystem::path asset_directory =
                application_directory / directory;
            error.clear();
            if (!std::filesystem::is_directory(asset_directory, error)) {
                return make_status(
                    InstallationState::missing_asset_directory,
                    application_directory,
                    game_executable,
                    asset_directory);
            }
        }

        return make_status(
            InstallationState::ready,
            application_directory,
            game_executable);
    }
    catch (...) {
        return make_status(
            InstallationState::invalid_launcher_path,
            {},
            {},
            launcher_executable);
    }
}

std::optional<LaunchRequest> make_launch_request(
    const InstallationStatus& status) noexcept {
    if (!status.ready()) {
        return std::nullopt;
    }

    try {
        return LaunchRequest{
            status.game_executable,
            status.application_directory,
            L"\"" + status.game_executable.native() + L"\"",
        };
    }
    catch (...) {
        return std::nullopt;
    }
}

std::optional<std::filesystem::path> default_save_directory(
    const std::wstring_view local_app_data) noexcept {
    if (local_app_data.empty()) {
        return std::nullopt;
    }

    try {
        return std::filesystem::path{std::wstring{local_app_data}} /
            L"InfiniteDungeon" / L"save";
    }
    catch (...) {
        return std::nullopt;
    }
}

std::wstring installation_message(const InstallationStatus& status) {
    switch (status.state) {
    case InstallationState::ready:
        return L"游戏已就绪";
    case InstallationState::invalid_launcher_path:
        return L"启动器路径无效";
    case InstallationState::missing_game_executable:
        return L"缺少 arpg_game.exe";
    case InstallationState::missing_asset_directory:
        return L"缺少资源：" +
            status.missing_path.lexically_relative(status.application_directory).generic_wstring();
    }

    return L"启动器路径无效";
}

}  // namespace arpg::launcher
