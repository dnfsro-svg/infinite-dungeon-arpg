#pragma once

#include "combat/combat_types.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "dungeon_view_math.hpp"

#include <array>
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
    const char* role_label{"CHASER"};
    MonsterWarningMode warning_mode{MonsterWarningMode::none};
    bool priority_warning{};
};

struct MonsterLabelTextStyle final {
    int affix_font_size{11};
    int role_font_size{14};
    int phase_font_size{12};
    int outline_pixels{2};
};

struct AffixBadge final {
    const char* short_name{"?"};
    const char* tier_text{"M1"};
    combat::MonsterAffixDanger danger{combat::MonsterAffixDanger::low};
    Rgba8 color{};
};

struct AffixOutline final {
    Rgba8 color{};
    std::uint8_t alpha{255U};
};

[[nodiscard]] CombatCameraView make_combat_camera_view(
    combat::Vec3 interpolated_player,
    float width,
    float height) noexcept;

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
[[nodiscard]] MonsterLabelTextStyle monster_label_text_style(
    float projection_scale) noexcept;
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
