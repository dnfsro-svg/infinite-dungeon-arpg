#pragma once

namespace arpg::platform::direct_input_poison {

struct Blocked final {
    template <typename... Args>
    void operator()(Args&&...) const = delete;

    const Blocked* operator&() const = delete;
};

inline constexpr Blocked blocked{};

inline constexpr bool active = true;

}  // namespace arpg::platform::direct_input_poison

#define platform_key_pressed ::arpg::platform::direct_input_poison::blocked
#define platform_key_down ::arpg::platform::direct_input_poison::blocked
#define IsKeyPressed ::arpg::platform::direct_input_poison::blocked
#define IsKeyPressedRepeat ::arpg::platform::direct_input_poison::blocked
#define IsKeyDown ::arpg::platform::direct_input_poison::blocked
#define IsKeyReleased ::arpg::platform::direct_input_poison::blocked
#define IsKeyUp ::arpg::platform::direct_input_poison::blocked
#define GetKeyPressed ::arpg::platform::direct_input_poison::blocked
#define GetCharPressed ::arpg::platform::direct_input_poison::blocked
#define IsMouseButtonPressed ::arpg::platform::direct_input_poison::blocked
#define IsMouseButtonDown ::arpg::platform::direct_input_poison::blocked
#define IsMouseButtonReleased ::arpg::platform::direct_input_poison::blocked
#define IsMouseButtonUp ::arpg::platform::direct_input_poison::blocked
#define GetMouseX ::arpg::platform::direct_input_poison::blocked
#define GetMouseY ::arpg::platform::direct_input_poison::blocked
#define GetMousePosition ::arpg::platform::direct_input_poison::blocked
#define GetMouseDelta ::arpg::platform::direct_input_poison::blocked
#define GetMouseWheelMove ::arpg::platform::direct_input_poison::blocked
#define GetMouseWheelMoveV ::arpg::platform::direct_input_poison::blocked
#define IsWindowFocused ::arpg::platform::direct_input_poison::blocked
