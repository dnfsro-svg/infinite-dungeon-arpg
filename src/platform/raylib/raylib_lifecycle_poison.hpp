#pragma once

#include <raylib.h>

namespace arpg::platform::raylib_lifecycle_poison {

struct Blocked final {
    constexpr Blocked() noexcept = default;
    Blocked(const Blocked&) = delete;
    Blocked& operator=(const Blocked&) = delete;

    template <typename... Args>
    void operator()(Args&&...) const = delete;

    Blocked* operator&() = delete;
    const Blocked* operator&() const = delete;
};

inline constexpr Blocked blocked{};
inline constexpr bool active = true;

}  // namespace arpg::platform::raylib_lifecycle_poison

#ifndef ARPG_ALLOW_RAYLIB_SET_CONFIG_FLAGS
#define SetConfigFlags ::arpg::platform::raylib_lifecycle_poison::blocked
#endif

#ifndef ARPG_ALLOW_RAYLIB_INIT_WINDOW
#define InitWindow ::arpg::platform::raylib_lifecycle_poison::blocked
#endif

#ifndef ARPG_ALLOW_RAYLIB_IS_WINDOW_READY
#define IsWindowReady ::arpg::platform::raylib_lifecycle_poison::blocked
#endif

#ifndef ARPG_ALLOW_RAYLIB_CLOSE_WINDOW
#define CloseWindow ::arpg::platform::raylib_lifecycle_poison::blocked
#endif

#ifndef ARPG_ALLOW_RAYLIB_WINDOW_SHOULD_CLOSE
#define WindowShouldClose ::arpg::platform::raylib_lifecycle_poison::blocked
#endif
