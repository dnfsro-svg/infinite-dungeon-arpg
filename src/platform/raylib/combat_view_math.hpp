#pragma once

#include "combat/combat_types.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "dungeon/dungeon_render_snapshot.hpp"
#include "dungeon_view_math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

struct ScreenProjection final {
    float x{};
    float y{};
    float ground_y{};
    float scale{};
};

struct CombatCameraView final {
    combat::Vec3 center{};
    float visible_width{24.0F};
    float visible_depth{11.0F};
};

struct ActorDrawItem final {
    combat::Vec3 position{};
    std::uint8_t index{};
};

enum class MonsterShapeId : std::uint8_t {
    bomber,
    charger,
    bulwark,
    support,
    shooter,
    dasher,
    chaser,
    hazard_caster,
};

enum class MonsterWarningMode : std::uint8_t {
    none,
    telegraph,
    active,
};

enum class HazardVisualMode : std::uint8_t {
    hidden,
    telegraph,
    active,
};

struct MonsterVisual final {
    Rgba8 body{};
    Rgba8 accent{};
    Rgba8 warning{};
    MonsterShapeId shape{MonsterShapeId::chaser};
    const char* role_label{u8"未知"};
    MonsterWarningMode warning_mode{MonsterWarningMode::none};
    bool priority_warning{};
};

struct MonsterLabelTextStyle final {
    int affix_font_size{11};
    int role_font_size{14};
    int phase_font_size{12};
    int outline_pixels{2};
};

struct SceneLabelStyle final {
    Color text_color{};
    Color backing_color{};
    float horizontal_padding{};
    float vertical_padding{};
};

struct SceneLabelLayout final {
    Rectangle backing{};
    Vector2 text_position{};
};

inline constexpr std::size_t kMonsterSceneLabelCapacity = 5U;

enum class MonsterSceneLabelKind : std::uint8_t {
    role,
    phase,
    affix,
};

struct MonsterSceneLabelSlot final {
    MonsterSceneLabelKind kind{MonsterSceneLabelKind::role};
    std::size_t affix_index{};
};

struct MonsterSceneLabelSequence final {
    std::array<MonsterSceneLabelSlot, kMonsterSceneLabelCapacity> slots{};
    std::size_t count{};
};

struct MonsterSceneLabelEntry final {
    Vector2 text_size{};
    SceneLabelStyle style{};
};

struct MonsterSceneLabelStackLayout final {
    std::array<SceneLabelLayout, kMonsterSceneLabelCapacity> plates{};
    std::size_t plate_count{};
    Rectangle bounds{};
    bool visible{};
};

struct MonsterPresentationPlan final {
    float resource_bar_offset_y{};
    std::size_t debug_label_lane{};
    bool resource_bars_visible{};
    bool role_label_visible{};
    bool phase_label_visible{};
};

struct AffixBadge final {
    const char* short_name{"?"};
    const char* tier_text{u8"词缀Ⅰ"};
    combat::MonsterAffixDanger danger{combat::MonsterAffixDanger::low};
    Rgba8 color{};
};

enum class DebugWorldVolumeKind : std::uint8_t {
    assist,
    attack,
    hurt,
};

struct AffixOutline final {
    Rgba8 color{};
    std::uint8_t alpha{255U};
};

[[nodiscard]] CombatCameraView make_combat_camera_view(
    combat::Vec3 interpolated_player,
    float width,
    float height) noexcept;

[[nodiscard]] dungeon::WorldViewQuery make_world_view_query(
    CombatCameraView camera,
    float width,
    float height,
    std::uint64_t camera_version) noexcept;

[[nodiscard]] combat::Vec3 interpolate_combat_position(
    combat::Vec3 from,
    combat::Vec3 to,
    float interpolation_alpha) noexcept;

[[nodiscard]] ScreenProjection project_combat_position(
    combat::Vec3 position,
    CombatCameraView view,
    float width,
    float height) noexcept;

[[nodiscard]] ScreenProjection project_combat_position(
    combat::Vec3 position,
    float width,
    float height) noexcept;

void sort_actor_draw_items(
    std::array<ActorDrawItem, 4>& items) noexcept;

[[nodiscard]] Rgba8 monster_ecology_color(
    dungeon::DungeonElement ecology) noexcept;
[[nodiscard]] MonsterVisual monster_visual(
    combat::MonsterId id,
    combat::MonsterAiPhase phase,
    dungeon::DungeonElement ecology) noexcept;
[[nodiscard]] const char* monster_phase_label(
    combat::MonsterAiPhase phase) noexcept;
[[nodiscard]] const char* debug_world_volume_label(
    DebugWorldVolumeKind kind) noexcept;
[[nodiscard]] MonsterLabelTextStyle monster_label_text_style(
    float viewport_scale) noexcept;
[[nodiscard]] SceneLabelStyle scene_label_style(
    Color requested_text_color) noexcept;
[[nodiscard]] SceneLabelLayout scene_label_layout(
    Vector2 anchor, Vector2 text_size,
    float viewport_width, float viewport_height,
    SceneLabelStyle style) noexcept;
[[nodiscard]] MonsterSceneLabelSequence monster_scene_label_sequence(
    bool draw_debug, std::size_t affix_count) noexcept;
[[nodiscard]] Rectangle monster_actor_label_exclusion(
    ScreenProjection projected) noexcept;
[[nodiscard]] MonsterSceneLabelStackLayout monster_scene_label_stack_layout(
    const std::array<MonsterSceneLabelEntry,
        kMonsterSceneLabelCapacity>& entries,
    std::size_t entry_count,
    const std::array<Rectangle, 3U>& visible_resource_bars,
    std::size_t visible_resource_bar_count,
    Rectangle actor_bounds,
    std::size_t label_lane,
    float projection_scale,
    float viewport_width,
    float viewport_height) noexcept;
[[nodiscard]] MonsterPresentationPlan monster_presentation_plan(
    std::size_t label_lane, bool draw_debug) noexcept;
[[nodiscard]] AffixBadge monster_affix_badge(
    combat::MonsterAffixInstance affix) noexcept;
[[nodiscard]] AffixOutline monster_affix_outline(
    combat::MonsterAffixInstance affix,
    std::uint64_t tick) noexcept;
[[nodiscard]] bool blink_affix_warning_visible(
    const combat::MonsterSnapshot& monster) noexcept;
[[nodiscard]] float blink_affix_warning_actor_radius(
    const combat::MonsterSnapshot& monster) noexcept;
[[nodiscard]] float blink_affix_warning_ground_radius(
    const combat::MonsterSnapshot& monster) noexcept;
[[nodiscard]] bool monster_visible(
    const combat::MonsterSnapshot& monster) noexcept;
[[nodiscard]] float player_hp_ratio(
    const combat::PlayerSnapshot& player) noexcept;
[[nodiscard]] HazardVisualMode hazard_visual_mode(
    const combat::HazardSnapshot& hazard) noexcept;
[[nodiscard]] bool uses_generic_hazard_pass(
    const combat::HazardSnapshot& hazard) noexcept;
[[nodiscard]] Rgba8 hazard_color(combat::HazardKind kind) noexcept;
[[nodiscard]] ScreenProjection project_projectile_position(
    const combat::ProjectileSnapshot& projectile,
    CombatCameraView view,
    float width,
    float height) noexcept;
[[nodiscard]] ScreenProjection project_projectile_position(
    const combat::ProjectileSnapshot& projectile,
    float width,
    float height) noexcept;
[[nodiscard]] ScreenProjection project_hazard_center(
    const combat::HazardSnapshot& hazard,
    CombatCameraView view,
    float width,
    float height) noexcept;
[[nodiscard]] ScreenProjection project_hazard_center(
    const combat::HazardSnapshot& hazard,
    float width,
    float height) noexcept;

}  // namespace arpg::platform
