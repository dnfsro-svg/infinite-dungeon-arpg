#pragma once

namespace arpg::platform {

[[nodiscard]] bool platform_key_pressed(int raylib_key) noexcept;
[[nodiscard]] bool platform_key_down(int raylib_key) noexcept;

}  // namespace arpg::platform
