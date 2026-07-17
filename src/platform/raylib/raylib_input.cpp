#include "raylib_input.hpp"

#include <raylib.h>

#if defined(_WIN32)
extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int key);
#endif

namespace arpg::platform {
namespace {

#if defined(_WIN32)
constexpr int kVkBack = 0x08;
constexpr int kVkTab = 0x09;
constexpr int kVkReturn = 0x0D;
constexpr int kVkEscape = 0x1B;
constexpr int kVkLeft = 0x25;
constexpr int kVkUp = 0x26;
constexpr int kVkRight = 0x27;
constexpr int kVkDown = 0x28;
constexpr int kVkInsert = 0x2D;
constexpr int kVkDelete = 0x2E;
constexpr int kVkF1 = 0x70;
constexpr int kVkLeftShift = 0xA0;
constexpr int kVkRightShift = 0xA1;
constexpr int kVkLeftControl = 0xA2;
constexpr int kVkRightControl = 0xA3;
constexpr int kVkLeftAlt = 0xA4;
constexpr int kVkRightAlt = 0xA5;

int virtual_key(int key) noexcept {
    if ((key >= KEY_ZERO && key <= KEY_NINE)
            || (key >= KEY_A && key <= KEY_Z)) {
        return key;
    }
    if (key >= KEY_F1 && key <= KEY_F12)
        return kVkF1 + (key - KEY_F1);
    switch (key) {
    case KEY_ESCAPE: return kVkEscape;
    case KEY_ENTER: return kVkReturn;
    case KEY_TAB: return kVkTab;
    case KEY_BACKSPACE: return kVkBack;
    case KEY_INSERT: return kVkInsert;
    case KEY_DELETE: return kVkDelete;
    case KEY_LEFT_CONTROL: return kVkLeftControl;
    case KEY_RIGHT_CONTROL: return kVkRightControl;
    case KEY_LEFT_SHIFT: return kVkLeftShift;
    case KEY_RIGHT_SHIFT: return kVkRightShift;
    case KEY_LEFT_ALT: return kVkLeftAlt;
    case KEY_RIGHT_ALT: return kVkRightAlt;
    case KEY_UP: return kVkUp;
    case KEY_DOWN: return kVkDown;
    case KEY_LEFT: return kVkLeft;
    case KEY_RIGHT: return kVkRight;
    default: return 0;
    }
}

short asynchronous_key_state(int key) noexcept {
    const int mapped = virtual_key(key);
    return mapped == 0 ? 0 : GetAsyncKeyState(mapped);
}
#endif

}  // namespace

bool platform_key_pressed(int key) noexcept {
#if defined(_WIN32)
    return IsKeyPressed(key)
        || (asynchronous_key_state(key) & 0x0001) != 0;
#else
    return IsKeyPressed(key);
#endif
}

bool platform_key_down(int key) noexcept {
#if defined(_WIN32)
    const short state = asynchronous_key_state(key);
    return IsKeyDown(key)
        || (state & static_cast<short>(0x8000)) != 0
        || (state & 0x0001) != 0;
#else
    return IsKeyDown(key);
#endif
}

DeathInputGate death_input_gate(bool death_saving, bool death_pending,
    const FrameKeyState& keys) noexcept {
    const bool death_active = death_saving || death_pending;
    return {
        !death_saving && death_pending && keys.e,
        keys.f12 || keys.v,
        keys.f1,
        keys.escape,
        !death_active,
    };
}

}  // namespace arpg::platform
