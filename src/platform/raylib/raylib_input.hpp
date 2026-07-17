#pragma once

namespace arpg::platform {

struct DeathInputGate final {
    bool continue_death{};
    bool screenshot{};
    bool debug_toggle{};
    bool exit{};
    bool forward_gameplay{};
};

struct FrameKeyState final {
    bool e{};
    bool f12{};
    bool v{};
    bool f1{};
    bool escape{};
    bool movement{};
    bool attack{};
    bool reset{};
    bool inventory{};
    bool passives{};
    bool mouse_gameplay{};
    bool enter{};
    bool recovery{};
    bool focus_lost{};
};

[[nodiscard]] bool platform_key_pressed(int raylib_key) noexcept;
[[nodiscard]] bool platform_key_down(int raylib_key) noexcept;
[[nodiscard]] DeathInputGate death_input_gate(
    bool death_saving, bool death_pending,
    const FrameKeyState& keys) noexcept;

}  // namespace arpg::platform
