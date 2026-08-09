#pragma once

#include "pause_menu_state.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <optional>

namespace arpg::platform {

inline constexpr std::size_t kPauseMenuRowCapacity = 21U;
inline constexpr std::size_t kPauseMenuPrimaryTextCapacity = 96U;
inline constexpr std::size_t kPauseMenuSavedTextCapacity = 64U;

struct PauseMenuRowView final {
    std::array<char, kPauseMenuPrimaryTextCapacity> primary{};
    std::array<char, kPauseMenuSavedTextCapacity> saved{};
};

struct PauseMenuCenteredTextLayout final {
    Rectangle backing{};
    Rectangle text{};
};

struct PauseMenuLayout final {
    Rectangle panel{};
    Rectangle title{};
    Rectangle rows[kPauseMenuRowCapacity]{};
    Rectangle footer{};
    std::size_t visible_row_count{};
    float title_font_size{};
    float row_font_size{};
    float footer_font_size{};
};

struct PauseMenuView final {
    std::array<PauseMenuRowView, kPauseMenuRowCapacity> rows{};
    std::size_t row_count{};
    std::size_t selected_row{};
    const char* title{};
    const char* message{};
    const char* footer{};
};

[[nodiscard]] PauseMenuLayout pause_menu_layout(
    int width,
    int height) noexcept;

[[nodiscard]] PauseMenuLayout pause_menu_layout(
    int width,
    int height,
    std::size_t visible_row_count) noexcept;

[[nodiscard]] PauseMenuCenteredTextLayout centered_pause_text_layout(
    Rectangle row_bounds,
    float text_width,
    float text_height,
    float horizontal_inset) noexcept;

[[nodiscard]] PauseMenuView make_pause_menu_view(
    const PauseMenuState& state) noexcept;

[[nodiscard]] std::optional<std::size_t> hit_test_pause_row(
    const PauseMenuLayout& layout,
    Vector2 point) noexcept;

}  // namespace arpg::platform
