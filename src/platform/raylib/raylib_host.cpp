#include "raylib_host.hpp"

#include "core/fixed_step.hpp"

#include <raylib.h>

#include <cstdint>

#if !defined(RAYLIB_VERSION_MAJOR) || \
    !defined(RAYLIB_VERSION_MINOR) || \
    !defined(RAYLIB_VERSION_PATCH)
#error "raylib version macros are unavailable"
#endif

static_assert(RAYLIB_VERSION_MAJOR == 6, "raylib 6.0.0 is required");
static_assert(RAYLIB_VERSION_MINOR == 0, "raylib 6.0.0 is required");
static_assert(RAYLIB_VERSION_PATCH == 0, "raylib 6.0.0 is required");

namespace arpg::platform {

HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
    InitWindow(config.window_width, config.window_height, config.window_title);
    if (!IsWindowReady()) {
        TraceLog(LOG_ERROR, "raylib window initialization failed");
        return HostExitCode::window_initialization_failed;
    }

    SetExitKey(KEY_ESCAPE);
    SetTargetFPS(60);

    core::FixedStepRunner fixed_step;
    while (!WindowShouldClose()) {
        const core::FixedStepFrame frame =
            fixed_step.advance(static_cast<double>(GetFrameTime()));

        BeginDrawing();
        ClearBackground(Color{18, 21, 29, 255});
        DrawText("Stage 0 integration baseline", 32, 32, 24, RAYWHITE);
        DrawText(
            TextFormat(
                "tick: %llu",
                static_cast<unsigned long long>(frame.total_ticks)),
            32,
            72,
            20,
            LIGHTGRAY);
        EndDrawing();
    }

    CloseWindow();
    return HostExitCode::success;
}

}  // namespace arpg::platform
