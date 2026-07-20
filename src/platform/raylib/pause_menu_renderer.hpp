#pragma once

#include "pause_menu_view.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

inline constexpr std::size_t kPauseMenuRenderOpCapacity = 26U;

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

class PauseMenuRenderer final {
public:
    PauseMenuRenderer() noexcept = default;
    ~PauseMenuRenderer() noexcept;
    PauseMenuRenderer(const PauseMenuRenderer&) = delete;
    PauseMenuRenderer& operator=(const PauseMenuRenderer&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool has_cjk_font() const noexcept { return owns_font_; }
    void draw(const PauseMenuState& state) const noexcept;

private:
    Font font_{};
    bool owns_font_{};
};

void draw_pause_menu(const PauseMenuState& state) noexcept;

}  // namespace arpg::platform
