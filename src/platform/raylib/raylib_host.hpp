#pragma once

#include "core/fixed_step.hpp"
#include "dungeon/dungeon_types.hpp"
#include "material_asset_types.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace arpg::dungeon {
struct AutoPickupPolicy;
}

namespace arpg::settings {
enum class LootFilterMode : std::uint8_t;
enum class SettingsLoadStatus : std::uint8_t;
struct SettingsData;
class SettingsStore;
}

namespace arpg::platform {

struct DeathInputGate;
struct FrameKeyState;
struct PhysicalKeySnapshot;
enum class PauseCommand : std::uint8_t;
enum class PauseScreen : std::uint8_t;
struct PauseMenuState;
struct WindowSettingsBackend;

enum class Stage10ValidationScenario : std::uint8_t {
    none,
    abyss_door,
    thunderstorm_warning,
    hunting_flames_warning,
    chaos_expansion,
    reward_chest,
    pending_reward,
    exit_confirmation,
    player_death,
    room_reset,
    leave_started,
    restarted_failed,
    abyss_hole_descent,
};

enum class Stage11ValidationScenario : std::uint8_t {
    none,
    normal_death_recap,
    restart_same_recap,
    deep_continue,
    floor_one_continue,
    abyss_death_recap,
};

enum class Stage11BValidationScenario : std::uint8_t {
    none,
    paused_freeze,
    settings_page,
    rebound_attack,
    conflict_swap,
    restarted_settings,
    single_slot_recovery,
    corrupt_defaults,
};

enum class Stage11CHudValidationScenario : std::uint8_t {
    none,
    normal_combat,
    low_health_status,
    cleared_exit,
    abyss_abandon,
    level_up_points,
    debug_overlay,
};

enum class Stage11DLootValidationScenario : std::uint8_t {
    none,
    show_all,
    magic_or_better,
    rare_only,
    rare_only_abyss,
    preview_cancel,
    pickup_feedback,
};

enum class Stage17SkillStonesValidationScenario : std::uint8_t {
    none,
    production_sequence,
    restarted_loadout,
};

enum class Stage12UiShowcase : std::uint8_t {
    none,
    material_gallery,
    inventory,
    skill_stones,
    pause,
};

struct Stage12MonsterMaterialDrawStatus final {
    bool presenter_visible{};
    bool use_material_frame{};
    MaterialAtlasId atlas{MaterialAtlasId::count};
    std::uint16_t frame_index{};
    bool drawn{};
};

struct Stage12MaterialRuntimeStatus final {
    bool shader_pipeline_ready{};
    bool water_ecology_ready{};
    bool water_environment_resident{};
    bool water_bulwark_resident{};
    bool water_support_resident{};
    bool lightning_ecology_ready{};
    bool lightning_environment_resident{};
    bool lightning_shooter_resident{};
    bool lightning_dasher_resident{};
    Stage12MonsterMaterialDrawStatus lightning_shooter_draw{};
    Stage12MonsterMaterialDrawStatus lightning_dasher_draw{};
    bool chaos_ecology_ready{};
    bool chaos_environment_resident{};
    bool chaos_chaser_resident{};
    bool chaos_hazard_resident{};
    Stage12MonsterMaterialDrawStatus chaos_chaser_draw{};
    Stage12MonsterMaterialDrawStatus chaos_hazard_draw{};
    bool items_ui_resident{};
    std::array<std::uint64_t, 6U> equipment_slot_draws{};
    std::array<std::uint64_t, 4U> rarity_draws{};
    std::array<std::uint64_t, items::kMaterialCount> material_draws{};
    bool ui_material_resident{};
    std::array<std::uint64_t, 40U> ui_material_draws{};
};

struct RaylibHostConfig final {
    int window_width{1280};
    int window_height{720};
    const char* window_title{"Infinite Dungeon - Stage 3 Dungeon Rules"};
    std::optional<std::filesystem::path> save_directory{};
    std::optional<std::filesystem::path> settings_directory{};
    // F12 writes into this caller-owned directory when provided, isolating
    // concurrent formal runs from the legacy executable-directory capture.
    std::optional<std::filesystem::path> screenshot_directory{};
    std::optional<std::uint64_t> new_run_seed{};
    bool validation_capture{};
    std::uint32_t validation_exit_after_presented_frames{};
    Stage10ValidationScenario stage10_validation{
        Stage10ValidationScenario::none};
    Stage11ValidationScenario stage11_validation{
        Stage11ValidationScenario::none};
    Stage11BValidationScenario stage11b_validation{
        Stage11BValidationScenario::none};
    Stage11CHudValidationScenario stage11c_hud_validation{
        Stage11CHudValidationScenario::none};
    Stage11DLootValidationScenario stage11d_loot_validation{
        Stage11DLootValidationScenario::none};
    Stage17SkillStonesValidationScenario stage17_skill_stones_validation{
        Stage17SkillStonesValidationScenario::none};
    std::uint8_t validation_abyss_direction{0xFFU};
    std::uint32_t validation_steps_per_frame{};
    std::optional<std::filesystem::path> validation_capture_file{};
    std::optional<std::filesystem::path> validation_summary_file{};
    bool validation_request_screenshot{};
    bool stage12_material_showcase{};
    bool stage12_material_showcase_hide_monsters{};
    Stage12UiShowcase stage12_ui_showcase{Stage12UiShowcase::none};
    std::optional<dungeon::DungeonElement> stage12_material_showcase_ecology{};
    std::optional<std::filesystem::path>
        stage12_material_baseline_capture_file{};
    Stage12MaterialRuntimeStatus* stage12_material_runtime_status{};
};

struct HostFrameGateResult final {
    bool forward_gameplay{};
    core::FixedStepFrame fixed_step{};
};

[[nodiscard]] HostFrameGateResult gate_host_frame(
    core::FixedStepRunner& fixed_step,
    bool& pause_latched,
    bool paused,
    double frame_seconds) noexcept;

[[nodiscard]] dungeon::AutoPickupPolicy loot_pickup_policy(
    settings::LootFilterMode mode) noexcept;

[[nodiscard]] settings::LootFilterMode renderer_loot_filter_mode(
    PauseScreen screen,
    const settings::SettingsData& live_settings,
    const settings::SettingsData& draft_settings) noexcept;

[[nodiscard]] DeathInputGate host_death_input_gate(
    bool death_saving,
    bool death_pending,
    FrameKeyState keys,
    const PhysicalKeySnapshot& physical_keys) noexcept;

struct HostSettingsNotice final {
    bool recovered_defaults_pending{};
};

[[nodiscard]] HostSettingsNotice make_host_settings_notice(
    settings::SettingsLoadStatus status) noexcept;
void consume_host_settings_notice(
    HostSettingsNotice& notice,
    PauseScreen previous_screen,
    PauseMenuState& pause_menu) noexcept;

[[nodiscard]] bool settle_host_pause_command(
    PauseCommand command,
    bool window_close_requested,
    PauseMenuState& pause_menu,
    settings::SettingsData& live_settings,
    settings::SettingsData& input_settings,
    const settings::SettingsStore& settings_store,
    WindowSettingsBackend settings_backend);

enum class HostExitCode : int {
    success = 0,
    window_initialization_failed = 1,
    invalid_arguments = 2,
    save_initialization_failed = 3,
};

[[nodiscard]] HostExitCode run_raylib_host(
    const RaylibHostConfig& config) noexcept;

}  // namespace arpg::platform
