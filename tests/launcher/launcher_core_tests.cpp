#include "launcher_core.hpp"
#include "test_framework.hpp"

#include <fstream>
#include <string>
#include <system_error>

namespace {

using arpg::launcher::InstallationState;
using arpg::launcher::InstallationStatus;

constexpr std::wstring_view kRequiredAssetDirectories[] = {
    L"assets/fonts",
    L"assets/player",
    L"assets/skills",
    L"assets/stage12",
    L"assets/stage14/audio",
    L"assets/stage15/audio",
};

class TemporaryInstallation final {
public:
    explicit TemporaryInstallation(std::wstring_view name) noexcept {
        std::error_code error;
        root_ = std::filesystem::temp_directory_path(error) /
            L"桌面 启动器 核心测试" / std::wstring{name};
        if (error) {
            root_.clear();
            return;
        }
        std::filesystem::remove_all(root_, error);
        error.clear();
        std::filesystem::create_directories(root_, error);
    }

    ~TemporaryInstallation() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    const std::filesystem::path& root() const noexcept {
        return root_;
    }

    std::filesystem::path launcher_path() const {
        return root_ / L"desktop_launcher.exe";
    }

    void create_ready_files() const {
        std::error_code error;
        for (const auto directory : kRequiredAssetDirectories) {
            std::filesystem::create_directories(root_ / directory, error);
            error.clear();
        }
        std::ofstream game{root_ / L"arpg_game.exe", std::ios::binary};
    }

private:
    std::filesystem::path root_;
};

arpg::test::Failure ready_installation_is_accepted() noexcept {
    TemporaryInstallation installation{L"ready installation"};
    installation.create_ready_files();

    const auto status = arpg::launcher::inspect_installation(installation.launcher_path());
    ARPG_REQUIRE(status.state == InstallationState::ready);
    ARPG_REQUIRE(status.ready());
    ARPG_REQUIRE(status.application_directory == installation.root());
    ARPG_REQUIRE(status.game_executable == installation.root() / L"arpg_game.exe");
    ARPG_REQUIRE(arpg::launcher::installation_message(status) == L"游戏已就绪");
    return {};
}

arpg::test::Failure missing_game_is_reported() noexcept {
    TemporaryInstallation installation{L"missing game"};
    std::error_code error;
    for (const auto directory : kRequiredAssetDirectories) {
        std::filesystem::create_directories(installation.root() / directory, error);
        ARPG_REQUIRE(!error);
    }

    const auto status = arpg::launcher::inspect_installation(installation.launcher_path());
    ARPG_REQUIRE(status.state == InstallationState::missing_game_executable);
    ARPG_REQUIRE(status.missing_path == installation.root() / L"arpg_game.exe");
    ARPG_REQUIRE(arpg::launcher::installation_message(status) == L"缺少 arpg_game.exe");
    return {};
}

arpg::test::Failure missing_asset_directory_is_reported() noexcept {
    TemporaryInstallation installation{L"missing asset"};
    installation.create_ready_files();
    std::error_code error;
    std::filesystem::remove_all(installation.root() / L"assets/stage12", error);
    ARPG_REQUIRE(!error);

    const auto status = arpg::launcher::inspect_installation(installation.launcher_path());
    ARPG_REQUIRE(status.state == InstallationState::missing_asset_directory);
    ARPG_REQUIRE(status.missing_path == installation.root() / L"assets/stage12");
    ARPG_REQUIRE(arpg::launcher::installation_message(status) == L"缺少资源：assets/stage12");
    return {};
}

arpg::test::Failure ready_installation_builds_argument_free_request() noexcept {
    TemporaryInstallation installation{L"launch request"};
    installation.create_ready_files();

    const auto status = arpg::launcher::inspect_installation(installation.launcher_path());
    const auto request = arpg::launcher::make_launch_request(status);
    ARPG_REQUIRE(request.has_value());
    ARPG_REQUIRE(request->application == status.game_executable);
    ARPG_REQUIRE(request->working_directory == status.application_directory);
    ARPG_REQUIRE(request->command_line == L"\"" + status.game_executable.native() + L"\"");
    ARPG_REQUIRE(request->command_line.find(L"--") == std::wstring::npos);
    return {};
}

arpg::test::Failure long_chinese_game_path_is_preserved_verbatim() noexcept {
    const std::wstring segment(96U, L'路');
    const std::filesystem::path game_path =
        std::filesystem::path{L"C:\\长路径 空格"} / segment / segment / segment / L"arpg_game.exe";
    const InstallationStatus status{
        InstallationState::ready,
        game_path.parent_path(),
        game_path,
        {},
    };

    const auto request = arpg::launcher::make_launch_request(status);
    ARPG_REQUIRE(game_path.native().size() > 260U);
    ARPG_REQUIRE(request.has_value());
    ARPG_REQUIRE(request->application.native() == game_path.native());
    ARPG_REQUIRE(request->command_line == L"\"" + game_path.native() + L"\"");
    return {};
}

arpg::test::Failure local_app_data_builds_existing_default_save_path() noexcept {
    const std::wstring local_app_data = L"C:\\Users\\测试 用户\\AppData\\Local";
    const auto save_directory = arpg::launcher::default_save_directory(local_app_data);
    ARPG_REQUIRE(save_directory.has_value());
    ARPG_REQUIRE(*save_directory ==
        std::filesystem::path{local_app_data} / L"InfiniteDungeon" / L"save");
    return {};
}

arpg::test::Failure empty_local_app_data_is_rejected() noexcept {
    ARPG_REQUIRE(!arpg::launcher::default_save_directory(L"").has_value());
    return {};
}

}  // namespace

arpg::test::TestSuite launcher_core_suite() noexcept {
    static const arpg::test::TestCase cases[] = {
        {"ready installation is accepted", ready_installation_is_accepted},
        {"missing game is reported", missing_game_is_reported},
        {"missing asset directory is reported", missing_asset_directory_is_reported},
        {"ready installation builds argument-free request", ready_installation_builds_argument_free_request},
        {"long Chinese game path is preserved verbatim", long_chinese_game_path_is_preserved_verbatim},
        {"local app data builds existing default save path", local_app_data_builds_existing_default_save_path},
        {"empty local app data is rejected", empty_local_app_data_is_rejected},
    };
    return arpg::test::make_suite("launcher_core", cases);
}
