#include "raylib_host.hpp"

#include "combat_audio.hpp"
#include "combat_feedback.hpp"
#include "combat_key_bindings.hpp"
#include "combat_renderer.hpp"
#include "core/fixed_step.hpp"
#include "dungeon_runtime.hpp"
#include "dungeon_view_math.hpp"
#include "inventory_renderer.hpp"
#include "passive_tree_renderer.hpp"
#include "passive_tree_view_math.hpp"
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

struct FrameToggleInput final {
    bool take_screenshot{};
    bool toggle_debug{};
};

FrameToggleInput sample_frame_toggle_input() noexcept {
    return {IsKeyPressed(KEY_F12), IsKeyPressed(KEY_F1)};
}

combat::MovementInput sample_movement_input() noexcept {
    return {key_direction(KEY_A, KEY_D), key_direction(KEY_W, KEY_S)};
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
        SetExitKey(KEY_NULL);
        core::FixedStepRunner fixed_step;
        CombatRenderer renderer;
        CombatFeedback feedback;
        CombatAudio audio;
        InventoryRenderer inventory;
        const bool audio_ready = audio.initialize();
        bool draw_debug = false;
        bool passive_overlay_open = false;
        bool exit_requested = false;
        dungeon::DungeonSnapshot current{};
        dungeon::DungeonSnapshot previous{};
        if (runtime.session() != nullptr) {
            current = runtime.session()->snapshot();
            previous = current;
            drain_events(*runtime.session(), renderer, feedback, audio);
        }

        while (!WindowShouldClose() && !exit_requested) {
            const FrameToggleInput frame_toggles = sample_frame_toggle_input();
            if (recovery_requested(runtime.state() == DungeonRuntimeState::recovery_required,
                    IsKeyPressed(KEY_N))) {
                if (runtime.recover_with_new_run() && runtime.session() != nullptr) {
                    current = runtime.session()->snapshot();
                    previous = current;
                    drain_events(*runtime.session(), renderer, feedback, audio);
                }
            }
            if (runtime.state() == DungeonRuntimeState::recovery_required) {
                if (inventory.is_open()) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                }
                if (IsKeyPressed(KEY_ESCAPE)) {
                    exit_requested = true;
                    continue;
                }
                draw_recovery_screen(runtime.render_status());
                if (frame_toggles.take_screenshot) {
                    TakeScreenshot("stage3-dungeon-rules.png");
                }
                continue;
            }
            dungeon::DungeonSession* const session = runtime.session();
            if (session == nullptr) {
                break;
            }
            bool inventory_toggled_this_frame = false;
            if (runtime.state() != DungeonRuntimeState::running
                && inventory.is_open()) {
                inventory.close();
                fixed_step.clear_accumulator();
                inventory_toggled_this_frame = true;
            }
            if (!passive_tree_can_open(current)) {
                passive_overlay_open = false;
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                if (inventory.is_open()) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                    inventory_toggled_this_frame = true;
                } else if (passive_overlay_open) {
                    passive_overlay_open = false;
                } else {
                    exit_requested = true;
                    continue;
                }
            } else if (IsKeyPressed(kInventoryKey)) {
                if (inventory.is_open()) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                    inventory_toggled_this_frame = true;
                } else if (inventory_can_open(
                        runtime.state() == DungeonRuntimeState::running,
                        passive_overlay_open)) {
                    inventory.open(*session, current);
                    fixed_step.clear_accumulator();
                    previous = current;
                    inventory_toggled_this_frame = true;
                }
            }
            if (!inventory.is_open() && !inventory_toggled_this_frame
                && IsKeyPressed(kPassiveOverlayKey)
                && passive_overlay_can_toggle(inventory.is_open())
                && passive_tree_can_open(current)) {
                passive_overlay_open = !passive_overlay_open;
            }
            if (!inventory.is_open() && frame_toggles.toggle_debug) {
                draw_debug = !draw_debug;
            }
            if (inventory.is_open()
                && inventory.process_input(runtime, current)) {
                current = session->snapshot();
                previous = current;
                drain_events(*session, renderer, feedback, audio);
                if (runtime.state() != DungeonRuntimeState::running) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                    inventory_toggled_this_frame = true;
                }
            }
            const PassiveOverlayInputGate passive_input_gate = passive_overlay_input_gate(
                passive_overlay_open);
            const InventoryInputGate inventory_gate = inventory_input_gate(
                inventory.is_open() || inventory_toggled_this_frame);
            const bool forward_actions = passive_input_gate.forward_actions
                && inventory_gate.forward_actions;
            const bool forward_movement = passive_input_gate.forward_movement
                && inventory_gate.forward_movement;
            const bool forward_descent = passive_input_gate.forward_descent
                && inventory_gate.forward_descent;
            if (forward_actions && inventory_gate.forward_room_reset
                && IsKeyPressed(KEY_R)) {
                session->reset_current_room();
                current = session->snapshot();
                previous = current;
                drain_events(*session, renderer, feedback, audio);
            }
            if (passive_overlay_open && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                const auto selected = hit_test_passive_node(
                    {GetMousePosition().x, GetMousePosition().y},
                    static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
                if (selected.has_value()) {
                    const bool allocated = (current.passive_tree.allocated_bits
                        & (1ULL << *selected)) != 0U;
                    static_cast<void>(allocated
                        ? session->request_passive_refund(*selected)
                        : session->request_passive_allocation(*selected));
                    current = session->snapshot();
                }
            }
            if (forward_actions) {
                submit_frame_actions(*session);
            }
            if (forward_descent && IsKeyPressed(KEY_E)) {
                const auto snapshot = session->snapshot();
                const bool in_range = snapshot.combat.has_value()
                    && can_prompt_descent(
                        snapshot, snapshot.combat->player.position);
                static_cast<void>(session->request_descent(in_range));
            }

            const combat::MovementInput movement = forward_movement
                ? sample_movement_input() : combat::MovementInput{};
            const float frame_seconds = GetFrameTime();
            feedback.update(frame_seconds);
            renderer.update(frame_seconds);
            core::FixedStepFrame frame{};
            if (!inventory.is_open() && !inventory_toggled_this_frame) {
                frame = fixed_step.advance(static_cast<double>(frame_seconds));
            } else {
                previous = current;
            }
            for (std::uint32_t step = 0; step < frame.steps; ++step) {
                previous = current;
                session->tick(movement);
                runtime.service_pending_save();
                current = session->snapshot();
                if (!passive_tree_can_open(current)) {
                    passive_overlay_open = false;
                }
                if (runtime.state() != DungeonRuntimeState::running
                    && inventory.is_open()) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                }
                drain_events(*session, renderer, feedback, audio);
            }

            BeginDrawing();
            ClearBackground(Color{13, 17, 27, 255});
            renderer.draw(previous, current, runtime.render_status(),
                static_cast<float>(frame.interpolation_alpha), draw_debug,
                feedback, audio_ready);
            if (passive_overlay_open) {
                draw_passive_tree_overlay(current, runtime.render_status());
            }
            if (inventory.is_open()) {
                inventory.draw(*session, current, runtime.render_status());
            }
            EndDrawing();
            if (!inventory.is_open() && frame_toggles.take_screenshot) {
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
