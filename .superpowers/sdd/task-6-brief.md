### Task 6: raylib 6.0 Graybox Host and Diagnostics HUD

**Worktree:** E:\game\.worktrees\m00-raylib-host

**Files:**
- Modify: src/platform/raylib/raylib_host.cpp

**Interfaces:**
- Consumes frozen RaylibHostConfig and FixedStepRunner from Task 2.
- Keeps raylib types private to raylib_host.cpp.
- Produces a 1280×720 resizable graybox window, 60 FPS render limit, 60 Hz fixed update, seven HUD diagnostics, Esc exit, and close-button exit.

- [ ] **Step 1: Run the baseline window and verify the visual red state**

Run:

~~~powershell
.\scripts\Build.ps1 -Preset windows-msvc-release -Fresh
Start-Process -FilePath .\out\build\windows-msvc-release\bin\arpg_game.exe -WindowStyle Normal -PassThru
~~~

Expected: the window opens and exits normally, but only shows a flat dark background, the baseline title, and tick count. It does not yet satisfy the graybox-room or complete-HUD acceptance criteria.

- [ ] **Step 2: Implement the low-cost 2.5D room and complete HUD**

Replace src/platform/raylib/raylib_host.cpp with:

~~~cpp
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
namespace {

[[nodiscard]] Vector2 lerp(
    Vector2 from,
    Vector2 to,
    float amount) noexcept {
    return {
        from.x + (to.x - from.x) * amount,
        from.y + (to.y - from.y) * amount,
    };
}

void draw_graybox_room() noexcept {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());

    const Vector2 back_left{width * 0.20F, height * 0.22F};
    const Vector2 back_right{width * 0.80F, height * 0.22F};
    const Vector2 floor_left{width * 0.04F, height * 0.92F};
    const Vector2 floor_right{width * 0.96F, height * 0.92F};

    DrawRectangleGradientV(
        0,
        0,
        GetScreenWidth(),
        GetScreenHeight(),
        Color{13, 17, 27, 255},
        Color{28, 32, 43, 255});

    DrawRectangle(
        static_cast<int>(back_left.x),
        0,
        static_cast<int>(back_right.x - back_left.x),
        static_cast<int>(back_left.y),
        Color{31, 37, 51, 255});

    DrawTriangle(
        Vector2{0.0F, 0.0F},
        back_left,
        floor_left,
        Color{22, 27, 39, 255});
    DrawTriangle(
        Vector2{0.0F, 0.0F},
        floor_left,
        Vector2{0.0F, height},
        Color{22, 27, 39, 255});
    DrawTriangle(
        Vector2{width, 0.0F},
        floor_right,
        back_right,
        Color{22, 27, 39, 255});
    DrawTriangle(
        Vector2{width, 0.0F},
        Vector2{width, height},
        floor_right,
        Color{22, 27, 39, 255});

    const Color floor{45, 51, 63, 255};
    DrawTriangle(back_left, floor_left, floor_right, floor);
    DrawTriangle(back_left, floor_right, back_right, floor);

    const Color grid = Color{87, 99, 119, 110};
    for (int column = 0; column <= 10; ++column) {
        const float amount =
            static_cast<float>(column) / 10.0F;
        DrawLineEx(
            lerp(back_left, back_right, amount),
            lerp(floor_left, floor_right, amount),
            1.0F,
            grid);
    }
    for (int row = 0; row <= 8; ++row) {
        const float linear =
            static_cast<float>(row) / 8.0F;
        const float perspective = linear * linear;
        DrawLineEx(
            lerp(back_left, floor_left, perspective),
            lerp(back_right, floor_right, perspective),
            1.0F,
            grid);
    }

    DrawLineEx(back_left, back_right, 3.0F, Color{118, 130, 151, 255});
    DrawLineEx(back_left, floor_left, 3.0F, Color{91, 103, 124, 255});
    DrawLineEx(back_right, floor_right, 3.0F, Color{91, 103, 124, 255});
}

void draw_diagnostics(
    const core::FixedStepFrame& frame,
    std::uint64_t root_seed) noexcept {
    DrawRectangleRounded(
        Rectangle{20.0F, 20.0F, 390.0F, 228.0F},
        0.08F,
        6,
        Color{7, 10, 17, 218});
    DrawRectangleRoundedLines(
        Rectangle{20.0F, 20.0F, 390.0F, 228.0F},
        0.08F,
        6,
        Color{91, 114, 151, 255});

    constexpr int x = 40;
    constexpr int font_size = 20;
    constexpr int line_height = 28;
    int y = 38;
    const Color text{218, 226, 239, 255};
    const Color accent{110, 207, 255, 255};

    DrawText(TextFormat("raylib %s", RAYLIB_VERSION), x, y, font_size, accent);
    y += line_height;
    DrawText(
        TextFormat(
            "seed 0x%016llX",
            static_cast<unsigned long long>(root_seed)),
        x,
        y,
        font_size,
        text);
    y += line_height;
    DrawText(
        TextFormat(
            "tick %llu",
            static_cast<unsigned long long>(frame.total_ticks)),
        x,
        y,
        font_size,
        text);
    y += line_height;
    DrawText(
        TextFormat(
            "fixed steps %u / %u",
            frame.steps,
            core::FixedStepRunner::kMaxStepsPerFrame),
        x,
        y,
        font_size,
        text);
    y += line_height;
    DrawText(
        TextFormat("alpha %.3f", frame.interpolation_alpha),
        x,
        y,
        font_size,
        text);
    y += line_height;
    DrawText(
        TextFormat("dropped %.6f s", frame.dropped_seconds),
        x,
        y,
        font_size,
        text);
    y += line_height;
    DrawText(
        TextFormat(
            "invalid dt %llu",
            static_cast<unsigned long long>(
                frame.invalid_input_count)),
        x,
        y,
        font_size,
        text);
}

}  // namespace

HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(config.window_width, config.window_height, config.window_title);
    if (!IsWindowReady()) {
        TraceLog(LOG_ERROR, "raylib window initialization failed");
        return HostExitCode::window_initialization_failed;
    }

    SetWindowMinSize(800, 450);
    SetExitKey(KEY_ESCAPE);
    SetTargetFPS(60);

    core::FixedStepRunner fixed_step;
    while (!WindowShouldClose()) {
        const core::FixedStepFrame frame =
            fixed_step.advance(static_cast<double>(GetFrameTime()));

        BeginDrawing();
        draw_graybox_room();
        draw_diagnostics(frame, config.root_seed);
        EndDrawing();
    }

    CloseWindow();
    return HostExitCode::success;
}

}  // namespace arpg::platform
~~~

- [ ] **Step 3: Build and run all headless checks**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
.\scripts\Test.ps1 -Preset windows-msvc-release
~~~

Expected: core.units and architecture.core_no_raylib pass in both configurations. No test initializes a window.

- [ ] **Step 4: Perform the host visual green check**

Run:

~~~powershell
$process = Start-Process -FilePath .\out\build\windows-msvc-release\bin\arpg_game.exe -WindowStyle Normal -PassThru
$process.Id
~~~

Inspect the visible window, then exit with Esc.

Expected:

- A resizable 1280×720 low-cost perspective room is visible.
- The HUD shows raylib 6.0, the root seed, total tick, fixed steps, alpha, dropped seconds, and invalid-dt count.
- The tick advances while alpha remains in [0, 1).
- Esc closes the process normally.

- [ ] **Step 5: Commit the host**

Run:

~~~powershell
git -c safe.directory=E:/game/.worktrees/m00-raylib-host diff --check
git -c safe.directory=E:/game/.worktrees/m00-raylib-host add src/platform/raylib/raylib_host.cpp
git -c safe.directory=E:/game/.worktrees/m00-raylib-host -c user.name=Codex -c user.email=codex@local commit -m "feat: render stage 0 raylib graybox"
~~~

Expected: task/m00-raylib-host is clean after one host commit.

---

