#pragma once

#include "combat/input_buffer.hpp"

#include <raylib.h>

#include <array>

namespace arpg::platform {

struct CombatKeyBinding final {
    int key{};
    combat::Action action{};
};

inline constexpr std::array<CombatKeyBinding, 3> kCombatKeyBindings{{
    {KEY_J, combat::Action::light},
    {KEY_K, combat::Action::jump},
    {KEY_L, combat::Action::launcher},
}};

inline constexpr int kInventoryKey = KEY_I;
inline constexpr int kPassiveOverlayKey = KEY_P;

}  // namespace arpg::platform
