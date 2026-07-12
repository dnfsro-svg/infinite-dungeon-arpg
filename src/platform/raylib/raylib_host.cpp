#include "raylib_host.hpp"

#include "combat_audio.hpp"
#include "combat_feedback.hpp"
#include "combat_key_bindings.hpp"
#include "combat_renderer.hpp"
#include "core/fixed_step.hpp"
#include "dungeon_runtime.hpp"
#include "dungeon_view_math.hpp"
#include "persistence/save_paths.hpp"

#include <raylib.h>

#include <cstdint>

#if !defined(RAYLIB_VERSION_MAJOR) || !defined(RAYLIB_VERSION_MINOR) \
    || !defined(RAYLIB_VERSION_PATCH)
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

void drain_events(dungeon::DungeonSession& session, CombatRenderer& renderer,
    CombatFeedback& feedback, CombatAudio& audio) noexcept {
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

void draw_recovery_screen(const DungeonRenderStatus& status) noexcept {
    BeginDrawing();
    ClearBackground(Color{21, 11, 27, 255});
    DrawText("SAVE RECOVERY REQUIRED", 48, 70, 32, Color{255, 169, 194, 255});
    DrawText("Both save slots are invalid. Press N to archive them and create a new run.",
        48, 122, 18, RAYWHITE);
    DrawText(TextFormat("Save error %u", static_cast<unsigned>(status.error)),
        48, 156, 16, Color{255, 202, 126, 255});
    EndDrawing();
}

}  // namespace

HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
    bool window_ready = false;
    try {
        const auto save_directory = config.save_directory.has_value()
            ? config.save_directory : persistence::default_save_directory();
        if (!save_directory.has_value()) {
            return HostExitCode::save_initialization_failed;
        }
        DungeonRuntimeConfig runtime_config{};
        runtime_config.save.directory = *save_directory;
        runtime_config.new_run_seed = config.new_run_seed;
        DungeonRuntime runtime(runtime_config);
        const bool initialized = runtime.initialize();
        if (!initialized && runtime.state() != DungeonRuntimeState::recovery_required) {
            return HostExitCode::save_initialization_failed;
        }

        SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
        InitWindow(config.window_width, config.window_height, config.window_title);
        window_ready = IsWindowReady();
        if (!window_ready) {
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
        CombatRenderer renderer;
        CombatFeedback feedback;
        CombatAudio audio;
        const bool audio_ready = audio.initialize();
        bool draw_debug = false;
        dungeon::DungeonSnapshot current{};
        dungeon::DungeonSnapshot previous{};
        if (runtime.session() != nullptr) {
            current = runtime.session()->snapshot();
            previous = current;
            drain_events(*runtime.session(), renderer, feedback, audio);
        }

        while (!WindowShouldClose()) {
            const bool take_screenshot = IsKeyPressed(KEY_F12);
            if (IsKeyPressed(KEY_F1)) {
                draw_debug = !draw_debug;
            }
            if (recovery_requested(runtime.state() == DungeonRuntimeState::recovery_required,
                    IsKeyPressed(KEY_N))) {
                if (runtime.recover_with_new_run() && runtime.session() != nullptr) {
                    current = runtime.session()->snapshot();
                    previous = current;
                    drain_events(*runtime.session(), renderer, feedback, audio);
                }
            }
            if (runtime.state() == DungeonRuntimeState::recovery_required) {
                draw_recovery_screen(runtime.render_status());
                if (take_screenshot) {
                    TakeScreenshot("stage3-dungeon-rules.png");
                }
                continue;
            }
            dungeon::DungeonSession* const session = runtime.session();
            if (session == nullptr) {
                break;
            }
            if (IsKeyPressed(KEY_R)) {
                session->reset_current_room();
                current = session->snapshot();
                previous = current;
                drain_events(*session, renderer, feedback, audio);
            }
            submit_frame_actions(*session);
            if (IsKeyPressed(KEY_E)) {
                const auto snapshot = session->snapshot();
                const bool in_range = snapshot.combat.has_value()
                    && can_prompt_descent(
                        snapshot, snapshot.combat->player.position);
                static_cast<void>(session->request_descent(in_range));
            }

            const combat::MovementInput movement{key_direction(KEY_A, KEY_D),
                key_direction(KEY_W, KEY_S)};
            const float frame_seconds = GetFrameTime();
            feedback.update(frame_seconds);
            renderer.update(frame_seconds);
            const core::FixedStepFrame frame = fixed_step.advance(
                static_cast<double>(frame_seconds));
            for (std::uint32_t step = 0; step < frame.steps; ++step) {
                previous = current;
                session->tick(movement);
                runtime.service_pending_transition();
                current = session->snapshot();
                drain_events(*session, renderer, feedback, audio);
            }

            BeginDrawing();
            ClearBackground(Color{13, 17, 27, 255});
            renderer.draw(previous, current, runtime.render_status(),
                static_cast<float>(frame.interpolation_alpha), draw_debug,
                feedback, audio_ready);
            EndDrawing();
            if (take_screenshot) {
                TakeScreenshot("stage3-dungeon-rules.png");
            }
        }
        audio.shutdown();
        CloseWindow();
        return HostExitCode::success;
    } catch (...) {
        if (window_ready) {
            CloseWindow();
        }
        return HostExitCode::save_initialization_failed;
    }
}

}  // namespace arpg::platform
