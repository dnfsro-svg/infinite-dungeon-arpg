#include "launcher_platform_win32.hpp"

#include <Windows.h>
#include <shellapi.h>

#include <limits>
#include <string_view>
#include <system_error>
#include <vector>

namespace arpg::launcher {
namespace {

std::wstring format_failure(
    const std::wstring_view operation,
    const unsigned long error_code) {
    wchar_t* system_message = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        0,
        reinterpret_cast<wchar_t*>(&system_message),
        0,
        nullptr);

    std::wstring message{operation};
    message += L"（错误代码 ";
    message += std::to_wstring(error_code);
    message += L"）：";
    if (length != 0U && system_message != nullptr) {
        std::wstring detail{system_message, length};
        LocalFree(system_message);
        while (!detail.empty() &&
            (detail.back() == L'\r' || detail.back() == L'\n' ||
                detail.back() == L' ')) {
            detail.pop_back();
        }
        message += detail;
    }
    else {
        message += L"无法获取系统错误消息";
    }
    return message;
}

PlatformResult failure(
    const std::wstring_view operation,
    const unsigned long error_code) {
    return {false, error_code, format_failure(operation, error_code)};
}

}  // namespace

std::optional<std::filesystem::path> current_executable_path() noexcept {
    try {
        std::vector<wchar_t> buffer(MAX_PATH);
        constexpr std::size_t kMaximumPathCharacters = 32768U;

        while (buffer.size() <= kMaximumPathCharacters) {
            SetLastError(ERROR_SUCCESS);
            const DWORD length = GetModuleFileNameW(
                nullptr,
                buffer.data(),
                static_cast<DWORD>(buffer.size()));
            if (length == 0U) {
                return std::nullopt;
            }
            if (length < buffer.size()) {
                return std::filesystem::path{
                    std::wstring{buffer.data(), static_cast<std::size_t>(length)}};
            }

            if (buffer.size() == kMaximumPathCharacters) {
                return std::nullopt;
            }
            const std::size_t next_size =
                (buffer.size() > kMaximumPathCharacters / 2U)
                ? kMaximumPathCharacters
                : buffer.size() * 2U;
            buffer.resize(next_size);
        }
    }
    catch (...) {
    }
    return std::nullopt;
}

PlatformResult launch_game(const LaunchRequest& request) noexcept {
    try {
        std::vector<wchar_t> command_line(
            request.command_line.begin(), request.command_line.end());
        command_line.push_back(L'\0');

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (CreateProcessW(
                request.application.c_str(), command_line.data(), nullptr, nullptr,
                FALSE, 0, nullptr, request.working_directory.c_str(), &startup, &process) ==
            FALSE) {
            const DWORD error_code = GetLastError();
            return failure(L"无法启动游戏", error_code);
        }

        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return {true, ERROR_SUCCESS, {}};
    }
    catch (...) {
        return failure(L"无法启动游戏", ERROR_NOT_ENOUGH_MEMORY);
    }
}

PlatformResult open_save_directory(const std::filesystem::path& path) noexcept {
    try {
        std::error_code error;
        std::filesystem::create_directories(path, error);
        if (error) {
            return failure(
                L"无法创建存档目录",
                static_cast<unsigned long>(error.value()));
        }

        const HINSTANCE result = ShellExecuteW(
            nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        const INT_PTR result_code = reinterpret_cast<INT_PTR>(result);
        if (result_code <= 32) {
            return failure(
                L"无法打开存档目录",
                static_cast<unsigned long>(result_code));
        }
        return {true, ERROR_SUCCESS, {}};
    }
    catch (...) {
        return failure(L"无法打开存档目录", ERROR_NOT_ENOUGH_MEMORY);
    }
}

}  // namespace arpg::launcher
