#include <windows.h>

#include <string>
#include <string_view>

namespace {

std::wstring g_key_marker_path;

void write_marker(const std::wstring& path) {
    if (path.empty()) return;
    const HANDLE marker = CreateFileW(path.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (marker == INVALID_HANDLE_VALUE) return;
    constexpr char contents[] = "key received\n";
    DWORD written{};
    WriteFile(marker, contents, sizeof(contents) - 1U, &written, nullptr);
    CloseHandle(marker);
}

void append_key_event_marker(const char* direction, WPARAM virtual_key) {
    if (g_key_marker_path.empty()) return;
    const HANDLE marker = CreateFileW(g_key_marker_path.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (marker == INVALID_HANDLE_VALUE) return;
    SetFilePointer(marker, 0, nullptr, FILE_END);
    const std::string line = std::string(direction) + ':'
        + std::to_string(static_cast<unsigned long long>(virtual_key)) + '\n';
    DWORD written{};
    WriteFile(marker, line.data(), static_cast<DWORD>(line.size()),
        &written, nullptr);
    CloseHandle(marker);
}

LRESULT CALLBACK window_procedure(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
        append_key_event_marker("down", wparam);
    }
    if (message == WM_KEYUP || message == WM_SYSKEYUP) {
        append_key_event_marker("up", wparam);
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace

int WINAPI wWinMain(
    HINSTANCE instance, HINSTANCE, PWSTR command_line, int show_command) {
    const std::wstring arguments = command_line;
    constexpr std::wstring_view two_windows_prefix = L"two-windows:";
    const bool create_two_windows =
        arguments.rfind(two_windows_prefix, 0U) == 0U;
    const std::wstring ready_marker_path = create_two_windows
        ? arguments.substr(two_windows_prefix.size())
        : std::wstring{};
    if (!create_two_windows) {
        g_key_marker_path = command_line;
        if (g_key_marker_path.size() >= 2U
            && g_key_marker_path.front() == L'"'
            && g_key_marker_path.back() == L'"') {
            g_key_marker_path = g_key_marker_path.substr(
                1U, g_key_marker_path.size() - 2U);
        }
    }

    constexpr wchar_t class_name[] = L"KeyboardDriverWindowFixture";
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = window_procedure;
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name;
    if (RegisterClassW(&window_class) == 0) return 1;

    const int window_count = create_two_windows ? 2 : 1;
    for (int index = 0; index < window_count; ++index) {
        const HWND window = CreateWindowExW(
            0, class_name, L"Keyboard driver target fixture",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320, 180,
            nullptr, nullptr, instance, nullptr);
        if (window == nullptr) return 2;
        ShowWindow(window, show_command);
        UpdateWindow(window);
    }
    write_marker(ready_marker_path);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
