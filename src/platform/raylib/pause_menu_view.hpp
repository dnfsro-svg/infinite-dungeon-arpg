#pragma once

#include "pause_menu_state.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <optional>

namespace arpg::platform {

inline constexpr std::size_t kPauseMenuRowCapacity = 21U;
inline constexpr std::size_t kPauseMenuRowTextCapacity = 64U;

struct PauseMenuLayout final {
    Rectangle panel{};
    Rectangle title{};
    Rectangle rows[kPauseMenuRowCapacity]{};
    Rectangle footer{};
};

struct PauseMenuView final {
    std::array<std::array<char, kPauseMenuRowTextCapacity>,
        kPauseMenuRowCapacity> rows{};
    std::size_t row_count{};
    std::size_t selected_row{};
    const char* title{};
    const char* message{};
};

[[nodiscard]] PauseMenuLayout pause_menu_layout(
    int width,
    int height) noexcept;

[[nodiscard]] PauseMenuView make_pause_menu_view(
    const PauseMenuState& state) noexcept;

[[nodiscard]] std::optional<std::size_t> hit_test_pause_row(
    const PauseMenuLayout& layout,
    Vector2 point) noexcept;

}  // namespace arpg::platform
