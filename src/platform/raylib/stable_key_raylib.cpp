#include "stable_key_raylib.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace arpg::platform {
namespace {

struct StableKeyEntry final {
    settings::StableKey stable{};
    int raylib{};
    const char* label{};
};

constexpr std::array<StableKeyEntry,
    static_cast<std::size_t>(settings::StableKey::count)> kStableKeys{{
    {settings::StableKey::a, KEY_A, "A"},
    {settings::StableKey::b, KEY_B, "B"},
    {settings::StableKey::c, KEY_C, "C"},
    {settings::StableKey::d, KEY_D, "D"},
    {settings::StableKey::e, KEY_E, "E"},
    {settings::StableKey::f, KEY_F, "F"},
    {settings::StableKey::g, KEY_G, "G"},
    {settings::StableKey::h, KEY_H, "H"},
    {settings::StableKey::i, KEY_I, "I"},
    {settings::StableKey::j, KEY_J, "J"},
    {settings::StableKey::k, KEY_K, "K"},
    {settings::StableKey::l, KEY_L, "L"},
    {settings::StableKey::m, KEY_M, "M"},
    {settings::StableKey::n, KEY_N, "N"},
    {settings::StableKey::o, KEY_O, "O"},
    {settings::StableKey::p, KEY_P, "P"},
    {settings::StableKey::q, KEY_Q, "Q"},
    {settings::StableKey::r, KEY_R, "R"},
    {settings::StableKey::s, KEY_S, "S"},
    {settings::StableKey::t, KEY_T, "T"},
    {settings::StableKey::u, KEY_U, "U"},
    {settings::StableKey::v, KEY_V, "V"},
    {settings::StableKey::w, KEY_W, "W"},
    {settings::StableKey::x, KEY_X, "X"},
    {settings::StableKey::y, KEY_Y, "Y"},
    {settings::StableKey::z, KEY_Z, "Z"},
    {settings::StableKey::digit_0, KEY_ZERO, "0"},
    {settings::StableKey::digit_1, KEY_ONE, "1"},
    {settings::StableKey::digit_2, KEY_TWO, "2"},
    {settings::StableKey::digit_3, KEY_THREE, "3"},
    {settings::StableKey::digit_4, KEY_FOUR, "4"},
    {settings::StableKey::digit_5, KEY_FIVE, "5"},
    {settings::StableKey::digit_6, KEY_SIX, "6"},
    {settings::StableKey::digit_7, KEY_SEVEN, "7"},
    {settings::StableKey::digit_8, KEY_EIGHT, "8"},
    {settings::StableKey::digit_9, KEY_NINE, "9"},
    {settings::StableKey::arrow_up, KEY_UP, "Up"},
    {settings::StableKey::arrow_down, KEY_DOWN, "Down"},
    {settings::StableKey::arrow_left, KEY_LEFT, "Left"},
    {settings::StableKey::arrow_right, KEY_RIGHT, "Right"},
    {settings::StableKey::space, KEY_SPACE, "Space"},
    {settings::StableKey::left_shift, KEY_LEFT_SHIFT, "Left Shift"},
    {settings::StableKey::right_shift, KEY_RIGHT_SHIFT, "Right Shift"},
    {settings::StableKey::left_control, KEY_LEFT_CONTROL, "Left Ctrl"},
    {settings::StableKey::right_control, KEY_RIGHT_CONTROL, "Right Ctrl"},
}};

}  // namespace

int raylib_key(settings::StableKey key) noexcept {
    const std::size_t index = static_cast<std::size_t>(key);
    return index < kStableKeys.size() ? kStableKeys[index].raylib : KEY_NULL;
}

std::optional<settings::StableKey> stable_key_from_raylib(int key) noexcept {
    for (const StableKeyEntry& entry : kStableKeys) {
        if (entry.raylib == key) return entry.stable;
    }
    return std::nullopt;
}

const char* stable_key_label(settings::StableKey key) noexcept {
    const std::size_t index = static_cast<std::size_t>(key);
    return index < kStableKeys.size() ? kStableKeys[index].label : "Unknown";
}

}  // namespace arpg::platform
