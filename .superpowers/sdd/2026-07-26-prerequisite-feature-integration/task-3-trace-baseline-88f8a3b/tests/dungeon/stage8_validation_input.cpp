#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {

WORD virtual_key(const char* name) noexcept {
    if (name == nullptr) return 0U;
    if (name[0] != '\0' && name[1] == '\0') {
        return static_cast<WORD>(std::toupper(
            static_cast<unsigned char>(name[0])));
    }
    if (_stricmp(name, "ESC") == 0) return VK_ESCAPE;
    if (_stricmp(name, "F12") == 0) return VK_F12;
    if (_stricmp(name, "CTRL") == 0) return VK_CONTROL;
    return 0U;
}

bool send_key(HWND window, WORD key, DWORD hold_milliseconds) noexcept {
    const LPARAM scan_code = static_cast<LPARAM>(
        MapVirtualKeyW(key, MAPVK_VK_TO_VSC)) << 16U;
    if (PostMessageW(window, WM_KEYDOWN, key, 1U | scan_code) == FALSE)
        return false;
    Sleep(hold_milliseconds);
    return PostMessageW(window, WM_KEYUP, key,
        1U | scan_code | (1ULL << 30U) | (1ULL << 31U)) != FALSE;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argv == nullptr) return 2;
    const WORD key = virtual_key(argv[1]);
    if (key == 0U) return 3;
    const int hold = argc >= 3 ? std::atoi(argv[2]) : 80;
    const int repeat = argc >= 4 ? std::atoi(argv[3]) : 1;
    const int pause = argc >= 5 ? std::atoi(argv[4]) : 80;
    if (hold < 1 || repeat < 1 || pause < 0) return 4;
    HWND const window = FindWindowW(
        nullptr, L"Infinite Dungeon - Stage 8 Validation");
    if (window == nullptr) return 5;
    static_cast<void>(SetForegroundWindow(window));
    for (int index = 0; index < repeat; ++index) {
        if (!send_key(window, key, static_cast<DWORD>(hold))) return 6;
        if (index + 1 < repeat) Sleep(static_cast<DWORD>(pause));
    }
    return 0;
}
