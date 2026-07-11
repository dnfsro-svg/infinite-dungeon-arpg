#include "raylib_host.hpp"

#include "combat/combat_world.hpp"
#include "combat_audio.hpp"
#include "combat_feedback.hpp"
#include "combat_key_bindings.hpp"
#include "combat_renderer.hpp"
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

std::int8_t key_direction(int negative_key, int positive_key) noexcept {
    const int negative = IsKeyDown(negative_key) ? 1 : 0;
    const int positive = IsKeyDown(positive_key) ? 1 : 0;
    return static_cast<std::int8_t>(positive - negative);
}

void submit_frame_actions(combat::CombatWorld& world) noexcept {
    for (const CombatKeyBinding& binding : kCombatKeyBindings) {
        if (IsKeyPressed(binding.key)) {
            static_cast<void>(world.queue_action(binding.action));
        }
    }
}

void drain_events(
    combat::CombatWorld& world,
    CombatRenderer& renderer,
    CombatFeedback& feedback,
    CombatAudio& audio) noexcept {
    while (const auto event = world.try_pop_event()) {
        renderer.consume_event(*event);
        feedback.consume(*event);
        audio.consume_event(*event);
    }
}

}  // namespace

HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(config.window_width, config.window_height, config.window_title);
    if (!IsWindowReady()) {
        TraceLog(LOG_ERROR, "raylib window initialization failed");
        return HostExitCode::window_initialization_failed;
    }
    if (!ChangeDirectory(GetApplicationDirectory())) {
        TraceLog(LOG_WARNING, "failed to use application directory");
    }

    SetWindowMinSize(800, 450);
    SetExitKey(KEY_ESCAPE);
    SetTargetFPS(60);

    core::FixedStepRunner fixed_step;
    combat::CombatWorld world;
    combat::CombatSnapshot current = world.snapshot();
    combat::CombatSnapshot previous = current;
    CombatRenderer renderer;
    CombatFeedback feedback;
    CombatAudio audio;
    const bool audio_ready = audio.initialize();
    bool draw_debug = false;

    while (!WindowShouldClose()) {
        const bool take_screenshot = IsKeyPressed(KEY_F12);
        if (IsKeyPressed(KEY_F1)) {
            draw_debug = !draw_debug;
        }

        if (IsKeyPressed(KEY_R)) {
            world.reset();
            current = world.snapshot();
            previous = current;
            drain_events(world, renderer, feedback, audio);
        }

        submit_frame_actions(world);
        const combat::MovementInput movement{
            key_direction(KEY_A, KEY_D),
            key_direction(KEY_W, KEY_S),
        };
        const float frame_seconds = GetFrameTime();
        feedback.update(frame_seconds);
        const core::FixedStepFrame frame = fixed_step.advance(
            static_cast<double>(frame_seconds));

        for (std::uint32_t step = 0; step < frame.steps; ++step) {
            previous = current;
            world.tick(movement);
            current = world.snapshot();
            drain_events(world, renderer, feedback, audio);
        }

        BeginDrawing();
        ClearBackground(Color{13, 17, 27, 255});
        renderer.draw(
            previous,
            current,
            static_cast<float>(frame.interpolation_alpha),
            draw_debug,
            feedback,
            audio_ready);
        if (take_screenshot) {
            TakeScreenshot("combat-lab.png");
        }
        EndDrawing();
    }

    audio.shutdown();
    CloseWindow();
    return HostExitCode::success;
}

}  // namespace arpg::platform
