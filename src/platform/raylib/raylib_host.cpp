#include "raylib_host.hpp"

#include "combat_audio.hpp"
#include "combat_feedback.hpp"
#include "combat_key_bindings.hpp"
#include "combat_renderer.hpp"
#include "core/fixed_step.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon_view_math.hpp"

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

void submit_frame_actions(dungeon::DungeonSession& session) noexcept {
    for (const CombatKeyBinding& binding : kCombatKeyBindings) {
        if (IsKeyPressed(binding.key)) {
            static_cast<void>(session.queue_action(binding.action));
        }
    }
}

void drain_events(
    dungeon::DungeonSession& session,
    CombatRenderer& renderer,
    CombatFeedback& feedback,
    CombatAudio& audio) noexcept {
    while (const auto event = session.try_pop_combat_event()) {
        renderer.consume_event(*event);
        feedback.consume(*event);
        audio.consume_event(*event);
    }
    while (const auto event = session.try_pop_event()) {
        renderer.consume_dungeon_event(*event);
        if (dungeon_event_clears_transients(event->kind)) {
            renderer.clear_combat_transients();
            feedback.clear();
            audio.stop_all();
        }
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
    dungeon::DungeonSession session{
        dungeon::DungeonSessionConfig{config.root_seed, 0}};
    dungeon::DungeonSnapshot current = session.snapshot();
    dungeon::DungeonSnapshot previous = current;
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
            session.reset_current_room();
            current = session.snapshot();
            previous = current;
            drain_events(session, renderer, feedback, audio);
        }

        submit_frame_actions(session);
        const combat::MovementInput movement{
            key_direction(KEY_A, KEY_D),
            key_direction(KEY_W, KEY_S),
        };
        const float frame_seconds = GetFrameTime();
        feedback.update(frame_seconds);
        renderer.update(frame_seconds);
        const core::FixedStepFrame frame = fixed_step.advance(
            static_cast<double>(frame_seconds));

        for (std::uint32_t step = 0; step < frame.steps; ++step) {
            previous = current;
            session.tick(movement);
            current = session.snapshot();
            drain_events(session, renderer, feedback, audio);
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
        EndDrawing();
        if (take_screenshot) {
            TakeScreenshot("room-loop.png");
        }
    }

    audio.shutdown();
    CloseWindow();
    return HostExitCode::success;
}

}  // namespace arpg::platform
