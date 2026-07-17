#pragma once

#include "pause_menu_view.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

inline constexpr std::size_t kPauseMenuRenderOpCapacity = 21U;

enum class PauseMenuRenderOpKind : std::uint8_t {
    dim,
    panel,
    title,
    row,
    message,
    footer,
};

struct PauseMenuRenderOp final {
    PauseMenuRenderOpKind kind{};
    std::size_t row_index{};
    bool selected{};
};

struct PauseMenuRenderPlan final {
    std::array<PauseMenuRenderOp, kPauseMenuRenderOpCapacity> ops{};
    std::size_t op_count{};
    bool has_message{};
};

[[nodiscard]] PauseMenuRenderPlan make_pause_menu_render_plan(
    const PauseMenuView& view) noexcept;

void draw_pause_menu(const PauseMenuState& state) noexcept;

}  // namespace arpg::platform
