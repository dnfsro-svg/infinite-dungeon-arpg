#include "raylib_host.hpp"

#include "combat_feedback.hpp"
#include "combat_renderer.hpp"
#include "combat_view_math.hpp"
#include "combat/active_skill_runtime.hpp"
#include "combat/fire_room_obstacle.hpp"
#include "combat/room_bounds.hpp"
#include "control_hints.hpp"
#include "death_overlay_font.hpp"
#include "core/fixed_step.hpp"
#include "dungeon_runtime.hpp"
#include "dungeon_view_math.hpp"
#include "game_audio.hpp"
#include "host_input.hpp"
#include "host_launch_options.hpp"
#include "host_settings_runtime.hpp"
#include "host_validation.hpp"
#include "host_window_lifetime.hpp"
#include "inventory_renderer.hpp"
#include "passive_tree_renderer.hpp"
#include "passive_tree_view_math.hpp"
#include "pause_menu_renderer.hpp"
#include "pause_menu_state.hpp"
#include "pause_menu_view.hpp"
#include "ui_material.hpp"
#include "ui_text_bounds_audit.hpp"
#include "persistence/save_paths.hpp"
#include "platform/settings/settings_store.hpp"
#include "platform/settings/settings_types.hpp"
#include "window_settings.hpp"

#include <raylib.h>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstddef>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall SetForegroundWindow(void*);
#endif

#include "direct_input_poison.hpp"

static_assert(arpg::platform::direct_input_poison::active,
    "direct input poison must be active in raylib_host.cpp");

#if !defined(RAYLIB_VERSION_MAJOR) || !defined(RAYLIB_VERSION_MINOR) \
    || !defined(RAYLIB_VERSION_PATCH)
#error "raylib version macros are unavailable"
#endif

static_assert(RAYLIB_VERSION_MAJOR == 6, "raylib 6.0.0 is required");
static_assert(RAYLIB_VERSION_MINOR == 0, "raylib 6.0.0 is required");
static_assert(RAYLIB_VERSION_PATCH == 0, "raylib 6.0.0 is required");
static_assert(FLAG_VSYNC_HINT != 0, "raylib VSync flag must remain available");

namespace arpg::platform {
namespace {

[[nodiscard]] bool stable_pressed(
    const PhysicalKeySnapshot& snapshot,
    settings::StableKey key) noexcept {
    const std::size_t index = static_cast<std::size_t>(key);
    return index < snapshot.pressed.size() && snapshot.pressed[index];
}

[[nodiscard]] std::optional<settings::StableKey> captured_stable_key(
    const PhysicalKeySnapshot& snapshot) noexcept {
    for (std::size_t index = 0U; index < snapshot.pressed.size(); ++index) {
        if (snapshot.pressed[index]) {
            return static_cast<settings::StableKey>(index);
        }
    }
    return std::nullopt;
}

[[nodiscard]] PauseInput pause_input_from_snapshot(
    const PhysicalKeySnapshot& snapshot,
    bool escape_consumed,
    bool capture_binding) noexcept {
    PauseInput input{};
    input.escape = snapshot.escape && !escape_consumed;
    input.enter = snapshot.enter;
    input.up = stable_pressed(snapshot, settings::StableKey::arrow_up);
    input.down = stable_pressed(snapshot, settings::StableKey::arrow_down);
    input.left = stable_pressed(snapshot, settings::StableKey::arrow_left);
    input.right = stable_pressed(snapshot, settings::StableKey::arrow_right);
    input.activate = snapshot.mouse_left;
    input.focus_lost = snapshot.focus_lost;
    if (capture_binding) input.captured_key = captured_stable_key(snapshot);
    return input;
}

void drain_events(dungeon::DungeonSession& session, CombatRenderer& renderer,
    CombatFeedback& feedback, GameAudio& audio,
    HostValidationRuntime* validation_runtime) noexcept {
    while (const auto event = session.try_pop_combat_event()) {
        validation_runtime->observe_combat_event(*event);
        renderer.consume_event(*event);
        feedback.consume(*event);
        audio.consume_event(*event);
    }
    while (const auto event = session.try_pop_event()) {
        renderer.consume_dungeon_event(*event);
        if (dungeon_event_clears_transients(event->kind)) {
            renderer.clear_combat_transients();
            feedback.clear();
            audio.clear_combat_transients();
        }
    }
}

[[nodiscard]] AudioBusLevels audio_bus_levels(
    const settings::SettingsData& value) noexcept {
    return {value.master_sfx_percent, value.sfx_percent, value.music_percent,
        value.ambience_percent, value.ui_percent};
}

[[nodiscard]] UiAudioCueMask pause_audio_cues(PauseScreen before,
    PauseScreen after, PauseCommand command) noexcept {
    UiAudioCueMask cues{};
    if (before == PauseScreen::closed && after != PauseScreen::closed) {
        cues = static_cast<UiAudioCueMask>(
            cues | ui_audio_cue_mask(UiAudioCue::open));
    } else if (before != PauseScreen::closed && after == PauseScreen::closed) {
        cues = static_cast<UiAudioCueMask>(
            cues | ui_audio_cue_mask(UiAudioCue::close));
    }
    switch (command) {
    case PauseCommand::preview:
        cues = static_cast<UiAudioCueMask>(
            cues | ui_audio_cue_mask(UiAudioCue::navigate));
        break;
    case PauseCommand::apply:
    case PauseCommand::quit:
        cues = static_cast<UiAudioCueMask>(
            cues | ui_audio_cue_mask(UiAudioCue::confirm));
        break;
    case PauseCommand::rollback:
        cues = static_cast<UiAudioCueMask>(
            cues | ui_audio_cue_mask(UiAudioCue::cancel));
        break;
    case PauseCommand::none:
    case PauseCommand::resume:
        break;
    }
    return cues;
}

void draw_recovery_screen(const DungeonRenderStatus& status) noexcept {
    BeginDrawing();
    ClearBackground(Color{21, 11, 27, 255});
    DrawText("SAVE RECOVERY REQUIRED", 48, 70, 32, Color{255, 169, 194, 255});
    DrawText("Both save slots are invalid. Press N to archive them and create a new run.",
        48, 122, 18, RAYWHITE);
    DrawText(TextFormat("Save error %u", static_cast<unsigned>(status.error)),
        48, 156, 16, Color{255, 202, 126, 255});
}

// Production screenshots intentionally read after EndDrawing presents the frame.
// With raylib 6 on Windows, reading the default framebuffer before that point
// produced black 1920x1080 captures despite correct render/framebuffer dimensions.
[[nodiscard]] bool present_frame_and_maybe_capture(const char* path) noexcept {
    EndDrawing();
    if (path == nullptr) return true;
    Image image = LoadImageFromScreen();
    if (image.data == nullptr) return false;
    const bool exported = ExportImage(image, path);
    UnloadImage(image);
    return exported;
}

void draw_stage12_ui_material_gallery(const MaterialPack& assets) noexcept {
    constexpr float kCell = 104.0F;
    constexpr float kGap = 8.0F;
    constexpr std::size_t kColumns = 8U;
    constexpr std::size_t kRows = 5U;
    const float width = kCell * static_cast<float>(kColumns)
        + kGap * static_cast<float>(kColumns - 1U);
    const float height = kCell * static_cast<float>(kRows)
        + kGap * static_cast<float>(kRows - 1U);
    const float left = (static_cast<float>(GetScreenWidth()) - width) * 0.5F;
    const float top = (static_cast<float>(GetScreenHeight()) - height) * 0.5F;
    DrawRectangleRounded({left - 16.0F, top - 16.0F,
        width + 32.0F, height + 32.0F}, 0.025F, 6,
        Color{3, 6, 11, 244});
    for (std::size_t index{}; index < kUiMaterialSprites.size(); ++index) {
        const std::size_t row = index / kColumns;
        const std::size_t column = index % kColumns;
        static_cast<void>(assets.draw_to(kUiMaterialSprites[index],
            {left + static_cast<float>(column) * (kCell + kGap),
             top + static_cast<float>(row) * (kCell + kGap), kCell, kCell}));
    }
}

std::optional<std::string> host_screenshot_path(
    const RaylibHostConfig& config) noexcept {
    try {
        const std::filesystem::path directory = config.screenshot_directory
            .value_or(std::filesystem::path{GetApplicationDirectory()});
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) {
            TraceLog(LOG_WARNING, "failed to create screenshot directory");
            return std::nullopt;
        }
        return (directory / "stage8-equipment-loot.png").string();
    } catch (...) {
        TraceLog(LOG_WARNING, "failed to construct screenshot path");
        return std::nullopt;
    }
}

void apply_stage12_material_showcase(dungeon::DungeonSnapshot& snapshot,
    std::optional<dungeon::DungeonElement> ecology,
    bool hide_monsters, bool hide_items) noexcept {
    if (!snapshot.combat.has_value()) return;
    if (ecology.has_value()) snapshot.ecology = *ecology;
    if (hide_items) {
        snapshot.ground_item_count = 0U;
        snapshot.ground_material_count = 0U;
        snapshot.ground_health_potion_count = 0U;
    } else {
        constexpr std::array<items::ItemSlot, 6U> item_slots{{
            items::ItemSlot::weapon, items::ItemSlot::helmet,
            items::ItemSlot::chest, items::ItemSlot::gloves,
            items::ItemSlot::boots, items::ItemSlot::accessory,
        }};
        constexpr std::array<items::ItemRarity, 6U> item_rarities{{
            items::ItemRarity::rare, items::ItemRarity::magic,
            items::ItemRarity::normal, items::ItemRarity::normal,
            items::ItemRarity::magic, items::ItemRarity::rare,
        }};
        snapshot.ground_item_count = static_cast<std::uint16_t>(
            item_slots.size());
        for (std::size_t index{}; index < item_slots.size(); ++index) {
            dungeon::GroundItemSnapshot& item = snapshot.ground_items[index];
            item = {};
            item.ordinal = static_cast<std::uint16_t>(index + 1U);
            item.source = index == 3U
                ? dungeon::GroundItemSource::abyss_chest
                : dungeon::GroundItemSource::monster_drop;
            item.abyss_reward_ordinal = index == 3U ? 0U : 0xFFU;
            item.position = stage12_material_showcase_position(
                -5.0F + static_cast<float>(index) * 2.0F, -3.2F);
            item.item_id = index + 1U;
            item.base_id = static_cast<std::uint8_t>(index + 1U);
            item.item_level = 60U;
            item.slot = item_slots[index];
            item.rarity = item_rarities[index];
        }
        snapshot.ground_material_count = static_cast<std::uint16_t>(
            items::kMaterialCount);
        for (std::size_t index{}; index < items::kMaterialCount; ++index) {
            dungeon::GroundMaterialSnapshot& material =
                snapshot.ground_materials[index];
            material = {};
            material.ordinal = static_cast<std::uint16_t>(index + 1U);
            material.source = items::material_is_coupon(
                    static_cast<items::MaterialId>(index))
                ? dungeon::GroundMaterialSource::monster_coupon
                : dungeon::GroundMaterialSource::monster_common;
            material.position = stage12_material_showcase_position(
                -4.5F + static_cast<float>(index % 7U) * 1.5F,
                2.0F + static_cast<float>(index / 7U) * 1.6F);
            material.material = static_cast<items::MaterialId>(index);
        }
    }
    constexpr std::array<combat::MonsterId, 8> ids{{
        combat::MonsterId::fire_bomber, combat::MonsterId::fire_charger,
        combat::MonsterId::water_bulwark, combat::MonsterId::water_support,
        combat::MonsterId::lightning_shooter, combat::MonsterId::lightning_dasher,
        combat::MonsterId::chaos_chaser, combat::MonsterId::chaos_hazard,
    }};
    constexpr std::array<combat::Vec3, 8> positions{{
        stage12_material_showcase_column_position(0U, -2.0F),
        stage12_material_showcase_column_position(1U, -2.0F),
        stage12_material_showcase_column_position(2U, -2.0F),
        stage12_material_showcase_column_position(3U, -2.0F),
        stage12_material_showcase_column_position(0U, 1.5F),
        stage12_material_showcase_column_position(1U, 1.5F),
        stage12_material_showcase_column_position(2U, 1.5F),
        stage12_material_showcase_column_position(3U, 1.5F),
    }};
    auto& combat_snapshot = *snapshot.combat;
    if (hide_monsters) {
        combat_snapshot.monster_count = 0U;
        snapshot.remaining_targets = 0U;
        for (combat::MonsterSnapshot& monster : combat_snapshot.monsters) {
            monster = {};
        }
        return;
    }
    combat_snapshot.monster_count = static_cast<std::uint16_t>(ids.size());
    snapshot.remaining_targets = static_cast<std::uint32_t>(ids.size());
    for (std::size_t index = 0U; index < ids.size(); ++index) {
        combat::MonsterSnapshot& monster = combat_snapshot.monsters[index];
        monster = {};
        monster.active = true;
        monster.generation = 1U;
        monster.monster_ordinal = static_cast<combat::MonsterOrdinal>(index);
        monster.id = ids[index];
        monster.position = positions[index];
        monster.spawn = positions[index];
        monster.facing = combat::Facing::right;
        monster.hp = 100;
        monster.max_hp = 100;
        monster.ai_phase = combat::MonsterAiPhase::active;
    }
}

void apply_stage12_material_showcase_world(
    const dungeon::DungeonSnapshot& showcase,
    dungeon::DungeonRenderSnapshot& world) noexcept {
    world.ecology = showcase.ecology;
    world.has_combat = showcase.combat.has_value();
    if (showcase.combat.has_value()) {
        world.combat = *showcase.combat;
    } else {
        world.combat = {};
    }
    world.equipment_count = static_cast<std::uint16_t>((std::min)(
        static_cast<std::size_t>(showcase.ground_item_count),
        world.equipment.size()));
    std::copy_n(showcase.ground_items.begin(), world.equipment_count,
        world.equipment.begin());
    world.material_count = static_cast<std::uint16_t>((std::min)(
        static_cast<std::size_t>(showcase.ground_material_count),
        world.materials.size()));
    std::copy_n(showcase.ground_materials.begin(), world.material_count,
        world.materials.begin());
    world.health_potion_count = static_cast<std::uint16_t>((std::min)(
        static_cast<std::size_t>(showcase.ground_health_potion_count),
        world.health_potions.size()));
    std::copy_n(showcase.ground_health_potions.begin(),
        world.health_potion_count, world.health_potions.begin());
}

}  // namespace

dungeon::AutoPickupPolicy loot_pickup_policy(
    settings::LootFilterMode mode) noexcept {
    switch (mode) {
    case settings::LootFilterMode::show_all:
        return {items::ItemRarity::normal};
    case settings::LootFilterMode::magic_or_better:
        return {items::ItemRarity::magic};
    case settings::LootFilterMode::rare_only:
        return {items::ItemRarity::rare};
    }
    return {};
}

DeathInputGate host_death_input_gate(
    bool death_saving,
    bool death_pending,
    FrameKeyState keys,
    const PhysicalKeySnapshot& physical_keys) noexcept {
    keys.e = stable_pressed(physical_keys, settings::StableKey::e);
    return death_input_gate(death_saving, death_pending, keys);
}

RaylibHostConfig make_production_host_config(HostLaunchOptions options) {
    RaylibHostConfig config{};
    config.save_directory = std::move(options.save_directory);
    config.settings_directory = std::move(options.settings_directory);
    config.screenshot_directory = std::move(options.screenshot_directory);
    config.new_run_seed = options.new_run_seed;
    config.continue_pending_death_on_launch = true;
    return config;
}

HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
    HostWindowLifetime window{raylib_host_window_backend()};
    std::unique_ptr<CombatRenderer> renderer_storage;
    try {
        const auto save_directory = config.save_directory.has_value()
            ? config.save_directory : persistence::default_save_directory();
        if (!save_directory.has_value()) {
            return HostExitCode::save_initialization_failed;
        }
        const auto settings_directory = config.settings_directory.has_value()
            ? config.settings_directory : save_directory;
        if (!settings_directory.has_value()) {
            return HostExitCode::save_initialization_failed;
        }
        settings::SettingsStore settings_store(*settings_directory);
        const settings::SettingsLoadResult loaded = settings_store.load();
        HostSettingsNotice settings_notice =
            make_host_settings_notice(loaded.status);
        settings::SettingsData committed_settings = loaded.settings;
        if (settings::validate_settings(committed_settings)
                != settings::SettingsValidationError::none) {
            committed_settings = settings::default_settings();
        }
        DungeonRuntimeConfig runtime_config{};
        runtime_config.save.directory = *save_directory;
        runtime_config.new_run_seed = config.new_run_seed;
        runtime_config.continue_pending_death_on_initialize =
            config.continue_pending_death_on_launch;
        const auto runtime_storage =
            std::make_unique<DungeonRuntime>(runtime_config);
        DungeonRuntime& runtime = *runtime_storage;
        const bool initialized = runtime.initialize();
        if (!initialized && runtime.state() != DungeonRuntimeState::recovery_required) {
            return HostExitCode::save_initialization_failed;
        }
        if ((config.stage12_material_background_only
                || config.stage12_material_icons_only)
                && runtime.state() == DungeonRuntimeState::recovery_required) {
            TraceLog(LOG_ERROR,
                "Stage 12 background-only capture refused a recovery save");
            return HostExitCode::save_initialization_failed;
        }
        dungeon::DungeonRenderSnapshot* const render_world =
            runtime.render_snapshot_storage();
        if (render_world == nullptr) {
            return HostExitCode::save_initialization_failed;
        }

        const auto validation_runtime =
            HostValidationRuntime::create(config, loaded.status);
        if (validation_runtime == nullptr) {
            return HostExitCode::save_initialization_failed;
        }
        if (!window.initialize(config, committed_settings)) {
            TraceLog(LOG_ERROR, "raylib window initialization failed");
            return HostExitCode::window_initialization_failed;
        }
        const bool directory_changed = config.validation_capture
            ? ChangeDirectory(save_directory->string().c_str())
            : ChangeDirectory(GetApplicationDirectory());
        if (!directory_changed) {
            TraceLog(LOG_WARNING, "failed to use screenshot working directory");
        }
        SetWindowMinSize(800, 450);
        SetExitKey(KEY_NULL);
#ifdef _WIN32
        if (config.stage11b_validation != Stage11BValidationScenario::none
                || config.stage11c_hud_validation
                    != Stage11CHudValidationScenario::none
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN foreground
                || config.stage11d_loot_validation
                    != Stage11DLootValidationScenario::none
// STAGE11D_LOOT_VALIDATION_SEAM_END foreground
                || config.stage17_skill_stones_validation
                    != Stage17SkillStonesValidationScenario::none
                ) {
            static_cast<void>(SetForegroundWindow(GetWindowHandle()));
        }
#endif
        core::FixedStepRunner fixed_step;
        renderer_storage = std::make_unique<CombatRenderer>();
        CombatRenderer& renderer = *renderer_storage;
        const bool hud_resources_ready = renderer.initialize_resources();
        validation_runtime->set_render_readiness(
            hud_resources_ready, renderer.active_skill_assets_ready());
        const auto pause_menu_renderer_storage =
            std::make_unique<PauseMenuRenderer>();
        PauseMenuRenderer& pause_menu_renderer = *pause_menu_renderer_storage;
        static_cast<void>(pause_menu_renderer.initialize());
        CombatFeedback feedback;
        const auto audio_storage = std::make_unique<GameAudio>();
        GameAudio& audio = *audio_storage;
        const auto inventory_storage = std::make_unique<InventoryRenderer>();
        InventoryRenderer& inventory = *inventory_storage;
        const bool audio_ready = audio.initialize();
        if (audio_ready) {
            SetMasterVolume(1.0F);
        }
        const WindowSettingsBackend settings_backend =
            raylib_window_settings_backend();
        PauseMenuState pause_menu{};
        pause_menu.committed = committed_settings;
        pause_menu.draft = committed_settings;
        settings::SettingsData live_settings = committed_settings;
        settings::SettingsData input_settings = committed_settings;
        HostSettingsRuntime settings_runtime{&settings_notice, &pause_menu,
            &live_settings, &input_settings, &settings_store,
            settings_backend};
        ControlHints control_hints{};
        refresh_control_hints(control_hints, input_settings);
        bool pause_latched = false;
        bool draw_debug = false;
        bool passive_overlay_open = false;
        bool exit_requested = false;
        bool window_close_latched = false;
        const auto begin_clean_exit = [&]() noexcept {
            if (runtime.state() == DungeonRuntimeState::running
                    && runtime.session() != nullptr) {
                if (runtime.clean_shutdown_state()
                        == CleanShutdownState::ready) {
                    exit_requested = true;
                } else {
                    static_cast<void>(runtime.request_clean_shutdown());
                }
                return;
            }
            exit_requested = true;
        };
        std::uint32_t presented_frame_count = 0U;
        std::uint64_t camera_version = 0U;
        unsigned validation_capture_tick = 0U;
        unsigned validation_capture_count = 0U;
        const std::string validation_capture_prefix = config.validation_capture
            ? (*save_directory / "stage8-validation-").string()
            : std::string{};
        const auto validation_capture_path = [&]() noexcept
                -> std::optional<std::string> {
            if (!config.validation_capture
                    || validation_capture_count >= 2000U
                    || ++validation_capture_tick < 60U) {
                return std::nullopt;
            }
            validation_capture_tick = 0U;
            ++validation_capture_count;
            return std::string{TextFormat("%s%03u.png",
                validation_capture_prefix.c_str(), validation_capture_count)};
        };
        const auto current_storage =
            std::make_unique<dungeon::DungeonSnapshot>();
        const auto previous_storage =
            std::make_unique<dungeon::DungeonSnapshot>();
        const auto presented_snapshot_storage =
            std::make_unique<dungeon::DungeonSnapshot>();
        dungeon::DungeonSnapshot& current = *current_storage;
        dungeon::DungeonSnapshot& previous = *previous_storage;
        dungeon::DungeonSnapshot& presented_snapshot =
            *presented_snapshot_storage;
        if (runtime.session() != nullptr) {
            runtime.session()->snapshot(current);
            previous = current;
            validation_runtime->observe_snapshot(current);
            drain_events(*runtime.session(), renderer, feedback, audio,
                validation_runtime.get());
            if (config.stage12_ui_showcase == Stage12UiShowcase::inventory
                    || config.stage12_ui_showcase
                        == Stage12UiShowcase::skill_stones) {
                inventory.open(*runtime.session(), current);
                if (config.stage12_ui_showcase
                        == Stage12UiShowcase::skill_stones) {
                    inventory.show_skill_stones_page();
                }
            } else if (config.stage12_ui_showcase
                    == Stage12UiShowcase::pause) {
                pause_menu.screen = PauseScreen::root;
                pause_menu.selected_row = 1U;
            }
        }

        while (!exit_requested) {
            runtime.pump_persistence_frame();
            if (runtime.clean_shutdown_state()
                    == CleanShutdownState::canceled) {
                // PollInputEvents clears raylib's native GLFW close flag after
                // each close event. Re-arm only our edge latch here so a later
                // genuine close request can start a new exact-save attempt.
                window_close_latched = false;
            }
            if (runtime.clean_shutdown_state() == CleanShutdownState::ready
                    || runtime.clean_shutdown_state()
                        == CleanShutdownState::faulted) {
                exit_requested = true;
                continue;
            }
            if (runtime.session() != nullptr) {
                previous = current;
                runtime.session()->snapshot(current);
                drain_events(*runtime.session(), renderer, feedback, audio,
                    validation_runtime.get());
            }
            validation_runtime->observe_snapshot(current);
            const bool window_close_requested = WindowShouldClose();
            if (!window_close_requested) window_close_latched = false;
            const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
            const bool gameplay_rearm_was_required =
                runtime.gameplay_rearm_required();
            const PhysicalKeySnapshot stage17_physical_keys =
                validation_runtime->inject_physical_edges(
                    sampled_physical_keys, input_settings, current,
                    gameplay_rearm_was_required);
            const PhysicalKeySnapshot& physical_keys =
                validation_runtime->death_input_snapshot();
            if (gameplay_rearm_was_required
                    && gameplay_controls_physically_released(
                        stage17_physical_keys)) {
                runtime.acknowledge_gameplay_rearmed();
            }
            HostFrameInput frame_input = map_host_frame_input(
                input_settings, stage17_physical_keys);
            if (recovery_requested(runtime.state() == DungeonRuntimeState::recovery_required,
                    frame_input.keys.recovery)) {
                if (runtime.recover_with_new_run() && runtime.session() != nullptr) {
                    runtime.session()->snapshot(current);
                    previous = current;
                    drain_events(*runtime.session(), renderer, feedback, audio,
                        validation_runtime.get());
                }
            }
            if (runtime.state() == DungeonRuntimeState::recovery_required) {
                if (config.stage12_material_background_only
                    || config.stage12_material_icons_only) {
                    TraceLog(LOG_ERROR,
                        "Stage 12 background-only capture entered recovery state");
                    exit_requested = true;
                    continue;
                }
                if (inventory.is_open()) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                }
                if (frame_input.keys.escape
                        || (window_close_requested
                            && !window_close_latched)) {
                    if (window_close_requested) window_close_latched = true;
                    begin_clean_exit();
                    continue;
                }
                const float recovery_frame_seconds = GetFrameTime();
                audio.update({false, current.is_abyss, true,
                        current.death.has_value()},
                    audio_bus_levels(live_settings), 0U,
                    recovery_frame_seconds);
                // A recovery screen owns presentation, not simulation.  It still
                // receives exactly one HUD observation before its presented frame.
                renderer.observe_presented_hud_frame(HudPresentedFrame::recovery,
                    previous, current, runtime.render_status(), control_hints,
                    GetFrameTime(), true);
                draw_recovery_screen(runtime.render_status());
                std::optional<std::string> capture_path =
                    validation_capture_path();
                if (frame_input.keys.f12 || frame_input.keys.v) {
                    capture_path = host_screenshot_path(config);
                }
                static_cast<void>(present_frame_and_maybe_capture(
                    capture_path.has_value() ? capture_path->c_str() : nullptr));
                ++presented_frame_count;
                continue;
            }
            dungeon::DungeonSession* const session = runtime.session();
            if (session == nullptr) {
                break;
            }
            const bool inventory_open_before = inventory.is_open();
            const bool passive_open_before = passive_overlay_open;
            const std::uint32_t inventory_count_before = current.inventory_count;
            bool inventory_toggled_this_frame = false;
            if (runtime.state() != DungeonRuntimeState::running
                && inventory.is_open()) {
                inventory.close();
                fixed_step.clear_accumulator();
                inventory_toggled_this_frame = true;
            }
            const bool death_saving = current.death.has_value()
                && current.death->saving;
            const bool death_pending = current.death.has_value()
                && current.death->can_continue;
            DeathInputGate death_gate = host_death_input_gate(
                death_saving, death_pending, frame_input.keys, physical_keys);
            if (validation_runtime->should_continue_death(current)) {
                death_gate.continue_death = true;
            }
            if (!death_gate.forward_gameplay) {
                if (inventory.is_open()) inventory.close();
                passive_overlay_open = false;
                inventory_toggled_this_frame = false;
            }
            if (!passive_tree_can_open(current)) {
                passive_overlay_open = false;
            }
            if (death_gate.exit && !death_gate.forward_gameplay) {
                begin_clean_exit();
            }
            dungeon::RequestResult death_continue_result =
                dungeon::RequestResult::rejected;
            if (death_gate.continue_death) {
                death_continue_result = runtime.request_death_continue();
                if (death_continue_result
                        != dungeon::RequestResult::rejected) {
                    previous = current;
                    session->snapshot(current);
                }
                validation_runtime->observe_death_continue_result(
                    death_continue_result);
            }
            if (!death_gate.forward_gameplay && window_close_requested
                    && !window_close_latched) {
                window_close_latched = true;
                begin_clean_exit();
            }
            bool escape_consumed = false;
            if (death_gate.forward_gameplay && frame_input.keys.escape) {
                if (inventory.is_open()) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                    inventory_toggled_this_frame = true;
                    escape_consumed = true;
                } else if (passive_overlay_open) {
                    passive_overlay_open = false;
                    escape_consumed = true;
                }
            } else if (death_gate.forward_gameplay
                    && pause_menu.screen == PauseScreen::closed
                    && frame_input.keys.inventory) {
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
            const bool passive_toggle_input_available =
                runtime.authority_requests_enabled()
                && !gameplay_rearm_was_required
                && death_gate.forward_gameplay
                && pause_menu.screen == PauseScreen::closed
                && !inventory.is_open()
                && !inventory_toggled_this_frame
                && passive_overlay_can_toggle(inventory.is_open());
            const PassiveTreeToggleAction passive_toggle_action =
                passive_tree_toggle_action(current,
                    frame_input.keys.passives,
                    passive_toggle_input_available);
            if (passive_toggle_action == PassiveTreeToggleAction::toggle) {
                passive_overlay_open = !passive_overlay_open;
            } else if (passive_toggle_action
                    == PassiveTreeToggleAction::show_full_clear_requirement) {
                renderer.publish_passive_tree_blocked();
            }
            if ((!inventory.is_open() || !death_gate.forward_gameplay)
                    && death_gate.debug_toggle) {
                draw_debug = !draw_debug;
            }
            if (death_gate.forward_gameplay
                && pause_menu.screen == PauseScreen::closed
                && inventory.is_open()
                && inventory.process_input(runtime, current, frame_input)) {
                session->snapshot(current);
                previous = current;
                drain_events(*session, renderer, feedback, audio,
                    validation_runtime.get());
                if (runtime.state() != DungeonRuntimeState::running) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                    inventory_toggled_this_frame = true;
                }
            }
            validation_runtime->observe_inventory(inventory, current);
            const PauseScreen pause_screen_before = pause_menu.screen;
            const bool pause_was_open =
                pause_screen_before != PauseScreen::closed;
            if (pause_was_open && physical_keys.mouse_left) {
                const PauseMenuView pause_view =
                    make_pause_menu_view(pause_menu);
                const PauseMenuLayout layout = pause_menu_layout(
                    GetScreenWidth(), GetScreenHeight(),
                    pause_view.row_count);
                const auto selected = hit_test_pause_row(
                    layout, physical_keys.mouse_position);
                if (selected.has_value()) pause_menu.selected_row = *selected;
            }
            const PauseContext pause_context{
                death_saving || death_pending,
                false,
                inventory.is_open(),
                passive_overlay_open,
                current.pending_save_kind.has_value(),
                !physical_keys.focus_lost,
            };
            PauseInput pause_input = pause_input_from_snapshot(
                physical_keys, escape_consumed,
                pause_menu.screen == PauseScreen::capture_binding);
            const PauseCommand pause_command = update_pause_menu(
                pause_menu, pause_context, pause_input);
            UiAudioCueMask ui_audio_cues = pause_audio_cues(
                pause_screen_before, pause_menu.screen, pause_command);
            settings_runtime.consume_notice(pause_screen_before);
            const std::uint64_t input_revision_before = input_settings.revision;
            if (settings_runtime.settle(
                    pause_command, window_close_requested)) {
                if (!window_close_requested || !window_close_latched) {
                    if (window_close_requested) window_close_latched = true;
                    begin_clean_exit();
                }
            }
            if (input_settings.revision != input_revision_before) {
                refresh_control_hints(control_hints, input_settings);
            }
            const bool pause_open = pause_menu.screen != PauseScreen::closed;
            const bool pause_blocks_gameplay = pause_open || pause_was_open;
            const bool gameplay_armed = runtime.authority_requests_enabled()
                && !gameplay_rearm_was_required;
            validation_runtime->observe_pause_transition(
                pause_was_open, pause_open);
            const float frame_seconds = GetFrameTime();
            HostFrameGateResult host_gate{};
            if (pause_open) {
                host_gate = gate_host_frame(fixed_step, pause_latched, true,
                    static_cast<double>(frame_seconds));
                previous = current;
            } else if (!inventory.is_open()
                    && !inventory_toggled_this_frame) {
                host_gate = gate_host_frame(fixed_step, pause_latched, false,
                    static_cast<double>(frame_seconds));
            } else {
                previous = current;
            }
            const PassiveOverlayInputGate passive_input_gate = passive_overlay_input_gate(
                passive_overlay_open);
            const InventoryInputGate inventory_gate = inventory_input_gate(
                inventory.is_open() || inventory_toggled_this_frame);
            const bool forward_actions = passive_input_gate.forward_actions
                && inventory_gate.forward_actions
                && death_gate.forward_gameplay
                && host_gate.forward_gameplay && !pause_blocks_gameplay
                && gameplay_armed;
            const bool forward_movement = passive_input_gate.forward_movement
                && inventory_gate.forward_movement
                && death_gate.forward_gameplay
                && host_gate.forward_gameplay && !pause_blocks_gameplay
                && gameplay_armed;
            const bool forward_descent = passive_input_gate.forward_descent
                && inventory_gate.forward_descent
                && death_gate.forward_gameplay
                && host_gate.forward_gameplay && !pause_blocks_gameplay
                && gameplay_armed;
            if (forward_actions && inventory_gate.forward_room_reset
                && frame_input.keys.reset) {
                const dungeon::RequestResult reset =
                    session->reset_current_room();
                if (reset != dungeon::RequestResult::rejected) {
                    session->snapshot(current);
                    previous = current;
                    drain_events(*session, renderer, feedback, audio,
                        validation_runtime.get());
                }
            }
            if (gameplay_armed && !pause_blocks_gameplay
                    && death_gate.forward_gameplay
                    && passive_overlay_open
                    && frame_input.keys.mouse_gameplay) {
                const auto selected = hit_test_passive_node(
                    {frame_input.mouse_position.x, frame_input.mouse_position.y},
                    static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
                if (selected.has_value()) {
                    const bool allocated = (current.passive_tree.allocated_bits
                        & (1ULL << *selected)) != 0U;
                    static_cast<void>(allocated
                        ? session->request_passive_refund(*selected)
                        : session->request_passive_allocation(*selected));
                    session->snapshot(current);
                }
            }
            if (forward_actions) {
                const SubmittedFrameActions submitted_actions =
                    submit_frame_actions(*session, frame_input);
                validation_runtime->observe_submitted_actions(submitted_actions);
            }
            if (forward_descent && frame_input.keys.e) {
                session->snapshot(current);
                const bool in_range = current.combat.has_value()
                    && can_prompt_descent(
                        current, current.combat->player.position);
                static_cast<void>(session->request_descent(in_range));
            }

            const combat::MovementInput movement = forward_movement
                ? frame_input.movement : combat::MovementInput{};
            feedback.update(frame_seconds);
            renderer.update(frame_seconds);
            core::FixedStepFrame frame = host_gate.fixed_step;
            if (host_gate.forward_gameplay) {
                if (config.stage10_validation
                            != Stage10ValidationScenario::none
                        || config.stage11_validation
                            != Stage11ValidationScenario::none
                        || config.stage11b_validation
                            != Stage11BValidationScenario::none
                        || config.stage11c_hud_validation
                            != Stage11CHudValidationScenario::none
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN fixed_step
                        || config.stage11d_loot_validation
                            != Stage11DLootValidationScenario::none
// STAGE11D_LOOT_VALIDATION_SEAM_END fixed_step
                        || config.stage17_skill_stones_validation
                            != Stage17SkillStonesValidationScenario::none
                        || (config.stage12_material_showcase
                            && config.validation_capture_file.has_value())
                        ) {
                    if (config.validation_steps_per_frame != 0U) {
                        frame.steps = config.validation_steps_per_frame;
                        frame.interpolation_alpha = 0.0;
                    }
                }
            }
            for (std::uint32_t step = 0; step < frame.steps; ++step) {
                previous = current;
                const bool step_death = current.death.has_value();
                combat::MovementInput step_movement{};
                if (!step_death) {
                    step_movement = validation_runtime->fixed_step_movement(
                        *session, current, movement);
                }
                runtime.fixed_tick(step_movement,
                    loot_pickup_policy(live_settings.loot_filter_mode));
                validation_runtime->observe_fixed_tick();
                session->snapshot(current);
                validation_runtime->observe_snapshot(current);
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN abyss_claim
                validation_runtime->observe_post_fixed_tick(
                    current, &session->item_state());
// STAGE11D_LOOT_VALIDATION_SEAM_END abyss_claim
                if (current.death.has_value()) {
                    if (inventory.is_open()) inventory.close();
                    passive_overlay_open = false;
                }
                if (!passive_tree_can_open(current)) {
                    passive_overlay_open = false;
                }
                if (runtime.state() != DungeonRuntimeState::running
                    && inventory.is_open()) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                }
                drain_events(*session, renderer, feedback, audio,
                    validation_runtime.get());
                if (validation_runtime->fixed_step_target_reached(current)) {
                    break;
                }
            }

            if (inventory.is_open() != inventory_open_before
                    || passive_overlay_open != passive_open_before) {
                const bool opened = inventory.is_open() || passive_overlay_open;
                ui_audio_cues = static_cast<UiAudioCueMask>(ui_audio_cues
                    | ui_audio_cue_mask(opened
                        ? UiAudioCue::open : UiAudioCue::close));
            }
            if (current.inventory_count > inventory_count_before) {
                ui_audio_cues = static_cast<UiAudioCueMask>(ui_audio_cues
                    | ui_audio_cue_mask(UiAudioCue::reward));
            }
            const settings::SettingsData& presented_audio_settings =
                pause_menu.screen == PauseScreen::settings
                    || pause_menu.screen == PauseScreen::capture_binding
                ? pause_menu.draft : live_settings;
            const bool combat_audio_active = current.combat.has_value()
                && (current.phase == dungeon::RoomPhase::combat
                    || current.phase == dungeon::RoomPhase::wave_delay)
                && current.remaining_targets != 0U;
            audio.update({combat_audio_active, current.is_abyss,
                    pause_blocks_gameplay || inventory.is_open()
                        || passive_overlay_open,
                    current.death.has_value()},
                audio_bus_levels(presented_audio_settings), ui_audio_cues,
                frame_seconds);

            validation_runtime->prepare_hud_snapshot(current);

            const bool stage12_item_baseline_frame =
                config.stage12_material_baseline_capture_file.has_value()
                && config.validation_exit_after_presented_frames >= 2U
                && presented_frame_count + 2U
                    == config.validation_exit_after_presented_frames;
            presented_snapshot = current;
            if (config.stage12_material_showcase) {
                apply_stage12_material_showcase(presented_snapshot,
                    config.stage12_material_showcase_ecology,
                    config.stage12_material_showcase_hide_monsters,
                    config.stage12_material_showcase_hide_items
                        || stage12_item_baseline_frame);
            }
            const dungeon::DungeonSnapshot& presented_hud_previous =
                config.stage12_material_showcase ? presented_snapshot : previous;
            const dungeon::DungeonSnapshot& presented_hud_current =
                config.stage12_material_showcase ? presented_snapshot : current;

            const settings::LootFilterMode presented_loot_filter =
                renderer_loot_filter_mode(
                    pause_menu.screen, live_settings, pause_menu.draft);
            renderer.set_loot_filter_mode(presented_loot_filter);
            const float screen_width = static_cast<float>(GetScreenWidth());
            const float screen_height = static_cast<float>(GetScreenHeight());
            const bool interpolate_camera = can_interpolate_room(
                previous, current);
            combat::Vec3 current_player{};
            if (current.combat.has_value()) {
                current_player = current.combat->player.position;
            }
            const combat::Vec3 previous_player = interpolate_camera
                ? previous.combat->player.position : current_player;
            const combat::Vec3 interpolated_player =
                interpolate_combat_position(previous_player, current_player,
                    static_cast<float>(frame.interpolation_alpha));
            const CombatCameraView frame_camera = make_combat_camera_view(
                interpolated_player, screen_width, screen_height);
            camera_version = camera_version
                    == (std::numeric_limits<std::uint64_t>::max)()
                ? 1U : camera_version + 1U;
            const dungeon::WorldViewQuery world_query = make_world_view_query(
                frame_camera, screen_width, screen_height, camera_version);
            if (!session->write_render_snapshot(world_query, *render_world)) {
                TraceLog(LOG_ERROR, "failed to publish bounded render snapshot");
                audio.shutdown();
                renderer.shutdown_resources();
                pause_menu_renderer.shutdown();
                return HostExitCode::save_initialization_failed;
            }
            if (config.stage12_material_showcase) {
                apply_stage12_material_showcase_world(
                    presented_snapshot, *render_world);
            }
            // The presented HUD consumes this exact bounded world snapshot;
            // publish it after all fixed-step and showcase transformations,
            // and before the frame is drawn.
            const HudPresentedFrame hud_presented_frame = current.death.has_value()
                ? HudPresentedFrame::death_overlay : HudPresentedFrame::normal;
            renderer.observe_presented_hud_frame(hud_presented_frame,
                presented_hud_previous, presented_hud_current,
                runtime.render_status(), control_hints,
                frame_seconds, pause_blocks_gameplay, render_world);
            validation_runtime->observe_hud(
                current, renderer.hud_model(), renderer.hud_notice_view(),
                draw_debug, GetScreenWidth(), GetScreenHeight());
            BeginDrawing();
            ClearBackground(Color{13, 17, 27, 255});
            reset_ui_text_bounds_audit();
            const GroundLootView ground_loot_view = [&]() noexcept {
                if (config.stage12_material_background_only) {
                    static_cast<void>(renderer.draw_room_background_only(
                        presented_snapshot.ecology));
                    return GroundLootView{};
                }
                if (config.stage12_material_icons_only) {
                    return renderer.draw_ground_loot_icons_only(
                        presented_snapshot, frame_camera);
                }
                return renderer.draw(
                    previous, presented_snapshot, *render_world, frame_camera,
                    runtime.render_status(),
                    static_cast<float>(frame.interpolation_alpha), draw_debug,
                    feedback, audio_ready);
            }();
            validation_runtime->observe_active_skill_draw(
                presented_snapshot, renderer.active_skill_draw_status());
            if (config.stage12_material_runtime_status != nullptr) {
                const MonsterMaterialDrawRuntimeStatus shooter_draw =
                    renderer.monster_material_draw_status(
                        combat::MonsterId::lightning_shooter);
                const MonsterMaterialDrawRuntimeStatus dasher_draw =
                    renderer.monster_material_draw_status(
                        combat::MonsterId::lightning_dasher);
                const MonsterMaterialDrawRuntimeStatus chaser_draw =
                    renderer.monster_material_draw_status(
                        combat::MonsterId::chaos_chaser);
                const MonsterMaterialDrawRuntimeStatus hazard_draw =
                    renderer.monster_material_draw_status(
                        combat::MonsterId::chaos_hazard);
                *config.stage12_material_runtime_status = {
                    renderer.material_pipeline_ready(),
                    renderer.material_ecology_ready(MaterialEcology::water),
                    renderer.material_atlas_available(
                        MaterialAtlasId::water_environment),
                    renderer.material_atlas_available(
                        MaterialAtlasId::water_bulwark),
                    renderer.material_atlas_available(
                        MaterialAtlasId::water_support),
                    renderer.material_ecology_ready(MaterialEcology::lightning),
                    renderer.material_atlas_available(
                        MaterialAtlasId::lightning_environment),
                    renderer.material_atlas_available(
                        MaterialAtlasId::lightning_shooter),
                    renderer.material_atlas_available(
                        MaterialAtlasId::lightning_dasher),
                    {shooter_draw.presenter_visible,
                        shooter_draw.use_material_frame, shooter_draw.atlas,
                        shooter_draw.frame_index, shooter_draw.drawn},
                    {dasher_draw.presenter_visible,
                        dasher_draw.use_material_frame, dasher_draw.atlas,
                        dasher_draw.frame_index, dasher_draw.drawn},
                    renderer.material_ecology_ready(MaterialEcology::chaos),
                    renderer.material_atlas_available(
                        MaterialAtlasId::chaos_environment),
                    renderer.material_atlas_available(
                        MaterialAtlasId::chaos_chaser),
                    renderer.material_atlas_available(
                        MaterialAtlasId::chaos_hazard),
                    {chaser_draw.presenter_visible,
                        chaser_draw.use_material_frame, chaser_draw.atlas,
                        chaser_draw.frame_index, chaser_draw.drawn},
                    {hazard_draw.presenter_visible,
                        hazard_draw.use_material_frame, hazard_draw.atlas,
                        hazard_draw.frame_index, hazard_draw.drawn},
                };
                auto& material_status =
                    *config.stage12_material_runtime_status;
                material_status.hud_ecology = renderer.hud_model().navigation.ecology;
                const RoomBackgroundDrawRuntimeStatus background_draw =
                    renderer.room_background_draw_status();
                material_status.room_background_ecology = background_draw.ecology;
                material_status.room_background_atlas = background_draw.atlas;
                material_status.room_background_resident = background_draw.resident;
                material_status.room_background_drawn = background_draw.drawn;
                material_status.room_background_source_width =
                    background_draw.source_width;
                material_status.room_background_source_height =
                    background_draw.source_height;
                material_status.room_background_scale = background_draw.scale;
                material_status.items_ui_resident =
                    renderer.material_atlas_available(MaterialAtlasId::items_ui);
                constexpr std::array<MaterialSpriteId, 6U> equipment_sprites{{
                    MaterialSpriteId::item_weapon, MaterialSpriteId::item_helmet,
                    MaterialSpriteId::item_chest, MaterialSpriteId::item_gloves,
                    MaterialSpriteId::item_boots, MaterialSpriteId::item_accessory,
                }};
                constexpr std::array<MaterialSpriteId, 4U> rarity_sprites{{
                    MaterialSpriteId::loot_rarity_normal,
                    MaterialSpriteId::loot_rarity_magic,
                    MaterialSpriteId::loot_rarity_rare,
                    MaterialSpriteId::loot_rarity_abyss,
                }};
                for (std::size_t index{}; index < equipment_sprites.size(); ++index) {
                    material_status.equipment_slot_draws[index] =
                        renderer.material_sprite_draw_count(equipment_sprites[index]);
                }
                for (std::size_t index{}; index < rarity_sprites.size(); ++index) {
                    material_status.rarity_draws[index] =
                        renderer.material_sprite_draw_count(rarity_sprites[index]);
                }
                for (std::size_t index{}; index < items::kMaterialCount; ++index) {
                    material_status.material_draws[index] =
                        renderer.material_sprite_draw_count(material_loot_sprite(
                            static_cast<items::MaterialId>(index)));
                }
                material_status.ui_material_resident =
                    renderer.material_atlas_available(
                        MaterialAtlasId::ui_material);
                for (std::size_t index{}; index < kUiMaterialSprites.size();
                     ++index) {
                    material_status.ui_material_draws[index] =
                        renderer.material_sprite_draw_count(
                            kUiMaterialSprites[index]);
                    material_status.ui_direct_stretch_draws[index] =
                        renderer.material_direct_stretch_draw_count(
                            kUiMaterialSprites[index]);
                }
                material_status.bundled_font_ready =
                    renderer.hud_font_ready()
                    && pause_menu_renderer.has_cjk_font();
                material_status.bundled_font_glyph_count =
                    renderer.hud_font_ready()
                    ? static_cast<std::uint16_t>((std::min)(
                        renderer.hud_font().glyphCount, 65535))
                    : 0U;
                const Font runtime_font = renderer.hud_font();
                material_status.bundled_font_source_base_size =
                    renderer.hud_font_ready()
                    ? static_cast<std::uint16_t>((std::max)(
                        runtime_font.baseSize, 0))
                    : 0U;
                material_status.bundled_font_atlas_bytes =
                    renderer.hud_font_ready()
                    ? ui_font_atlas_bytes(runtime_font.texture.width,
                        runtime_font.texture.height)
                    : 0U;
                material_status.bundled_font_total_atlas_bytes =
                    material_status.bundled_font_atlas_bytes
                        * kUiFontAtlasInstanceCount;
                material_status.bundled_font_total_atlas_byte_budget =
                    kUiFontTotalAtlasByteBudget;
            }
            validation_runtime->observe_ground_loot(
                current, pause_menu, runtime.render_status(),
                presented_loot_filter, ground_loot_view,
                renderer.hud_notice_view(), runtime.item_state(),
                GetScreenWidth(), GetScreenHeight());
            if (!config.stage12_material_background_only
                && !config.stage12_material_icons_only
                && passive_overlay_open) {
                draw_passive_tree_overlay(current, runtime.render_status());
            }
            if (!config.stage12_material_background_only
                && !config.stage12_material_icons_only
                && inventory.is_open()) {
                inventory.draw(*session, current, runtime.render_status(),
                    renderer.material_pack(), renderer.hud_font(),
                    renderer.hud_font_ready());
            }
            bool pause_cjk_ready = false;
            if (!config.stage12_material_background_only
                && !config.stage12_material_icons_only
                && pause_menu.screen != PauseScreen::closed) {
                pause_menu_renderer.draw(pause_menu, renderer.material_pack());
                pause_cjk_ready = pause_menu_renderer.has_cjk_font();
            }
            const PresentationDecision decision =
                validation_runtime->observe_presented_frame(
                    current, pause_menu, pause_cjk_ready);
            std::optional<std::string> capture_path =
                decision.stage17_capture_path;
            CaptureOwner selected_capture_owner =
                decision.capture_owner == CaptureOwner::stage17
                ? CaptureOwner::stage17 : CaptureOwner::none;
            bool generic_capture_path_selected = false;
            if (!capture_path.has_value() && stage12_item_baseline_frame) {
                capture_path =
                    config.stage12_material_baseline_capture_file->string();
            }
            if (config.stage12_material_runtime_status != nullptr) {
                const UiTextBoundsAuditStatus audit =
                    ui_text_bounds_audit_status();
                auto& material_status =
                    *config.stage12_material_runtime_status;
                for (std::size_t page{}; page < audit.pages.size(); ++page) {
                    material_status.ui_text_bounds_safe[page] =
                        audit.pages[page].bounds_safe;
                    material_status.ui_text_sizes_readable[page] =
                        audit.pages[page].sizes_readable;
                    material_status.ui_text_observed_roles[page] =
                        audit.pages[page].observed_roles;
                    material_status.ui_text_failed_bounds_roles[page] =
                        audit.pages[page].failed_bounds_roles;
                    material_status.ui_text_failed_size_roles[page] =
                        audit.pages[page].failed_size_roles;
                    material_status.ui_text_measured_counts[page] =
                        audit.pages[page].measured_text_count;
                    material_status.ui_text_minimum_display_sizes[page] =
                        audit.pages[page].minimum_display_font_size;
                }
            }
            if (!config.stage12_material_background_only
                && !config.stage12_material_icons_only
                && config.stage12_ui_showcase
                    == Stage12UiShowcase::material_gallery) {
                draw_stage12_ui_material_gallery(renderer.material_pack());
            }
            if (!capture_path.has_value()
                    && decision.capture_owner
                        == CaptureOwner::generic_validation) {
                capture_path = config.validation_capture_file->string();
                selected_capture_owner = CaptureOwner::generic_validation;
                generic_capture_path_selected = true;
            }
            if (death_gate.screenshot || (config.validation_request_screenshot
                    && presented_frame_count == 1U)) {
                generic_capture_path_selected = false;
                capture_path = host_screenshot_path(config);
            } else if (!capture_path.has_value()
                    && config.validation_capture_file.has_value()
                    && config.validation_exit_after_presented_frames != 0U
                    && config.stage10_validation == Stage10ValidationScenario::none
                    && config.stage11_validation == Stage11ValidationScenario::none
                    && config.stage11b_validation == Stage11BValidationScenario::none
                    && config.stage11c_hud_validation == Stage11CHudValidationScenario::none
                    && config.stage11d_loot_validation
                        == Stage11DLootValidationScenario::none
                    && presented_frame_count + 1U
                        >= config.validation_exit_after_presented_frames) {
                // Formal material validation captures an ordinary gameplay frame
                // at a caller-owned path without sharing the legacy F12 file.
                capture_path = config.validation_capture_file->string();
            } else if (!capture_path.has_value()) {
                capture_path = validation_capture_path();
            }
            const bool capture_succeeded =
                present_frame_and_maybe_capture(capture_path.has_value()
                    ? capture_path->c_str() : nullptr);
            const bool selected_generic_capture_succeeded_now =
                selected_capture_owner == CaptureOwner::generic_validation
                && generic_capture_path_selected && capture_succeeded;
            const bool selected_validation_capture_succeeded =
                selected_capture_owner == CaptureOwner::stage17
                ? capture_succeeded
                : selected_generic_capture_succeeded_now;
            validation_runtime->observe_capture_result(
                selected_capture_owner,
                selected_validation_capture_succeeded);
            ++presented_frame_count;
            const bool effective_generic_complete =
                decision.generic_capture_complete
                || selected_generic_capture_succeeded_now;
            if (config.validation_exit_after_presented_frames != 0U
                    && presented_frame_count
                        >= config.validation_exit_after_presented_frames) {
                begin_clean_exit();
            }
            if (decision.validation_complete
                    && (!config.validation_capture_file.has_value()
                        || effective_generic_complete)) {
                begin_clean_exit();
            }
        }
        validation_runtime->write_summaries(
            runtime.clean_shutdown_state(), pause_menu);
        audio.shutdown();
        renderer.shutdown_resources();
        pause_menu_renderer.shutdown();
        window.close();
        return HostExitCode::success;
    } catch (const std::exception& exception) {
        TraceLog(LOG_ERROR, "raylib host failed: %s", exception.what());
        if (renderer_storage != nullptr) {
            renderer_storage->shutdown_resources();
        }
        return HostExitCode::save_initialization_failed;
    } catch (...) {
        TraceLog(LOG_ERROR, "raylib host failed with an unknown exception");
        if (renderer_storage != nullptr) {
            renderer_storage->shutdown_resources();
        }
        return HostExitCode::save_initialization_failed;
    }
}

}  // namespace arpg::platform
