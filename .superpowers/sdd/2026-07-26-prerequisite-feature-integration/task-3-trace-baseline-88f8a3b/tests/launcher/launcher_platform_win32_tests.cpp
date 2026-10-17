#include "launcher_platform_win32.hpp"
#include "test_framework.hpp"

#include <Windows.h>

#include <filesystem>
#include <string>
#include <system_error>

namespace {

class TemporaryPlatformDirectory final {
public:
    TemporaryPlatformDirectory() noexcept {
        std::error_code error;
        root_ = std::filesystem::temp_directory_path(error) /
            L"桌面启动器平台测试" /
            std::to_wstring(GetCurrentProcessId());
        if (error) {
            root_.clear();
            return;
        }
        std::filesystem::remove_all(root_, error);
        error.clear();
        std::filesystem::create_directories(root_, error);
        if (error) {
            root_.clear();
        }
    }

    ~TemporaryPlatformDirectory() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    const std::filesystem::path& root() const noexcept {
        return root_;
    }

private:
    std::filesystem::path root_;
};

arpg::test::Failure failed_game_launch_reports_numeric_error_code() noexcept {
    TemporaryPlatformDirectory temporary;
    ARPG_REQUIRE(!temporary.root().empty());

    const std::filesystem::path missing_game =
        temporary.root() / L"确定不存在的游戏.exe";
    const arpg::launcher::LaunchRequest request{
        missing_game,
        temporary.root(),
        L"\"" + missing_game.native() + L"\"",
    };

    const arpg::launcher::PlatformResult result =
        arpg::launcher::launch_game(request);
    ARPG_REQUIRE(!result.ok);
    ARPG_REQUIRE(result.error_code != ERROR_SUCCESS);
    ARPG_REQUIRE(result.message.find(L"无法启动游戏") != std::wstring::npos);
    ARPG_REQUIRE(
        result.message.find(std::to_wstring(result.error_code)) !=
        std::wstring::npos);
    return {};
}

}  // namespace

arpg::test::TestSuite launcher_platform_win32_suite() noexcept {
    static const arpg::test::TestCase cases[] = {
        {"failed game launch reports numeric error code",
            failed_game_launch_reports_numeric_error_code},
    };
    return arpg::test::make_suite("launcher_platform_win32", cases);
}
