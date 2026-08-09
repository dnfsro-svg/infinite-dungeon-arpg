#include "combat_renderer.hpp"

#include "combat_view_math.hpp"
#include "debug_overlay_renderer.hpp"
#include "dungeon_runtime.hpp"

#include <raylib.h>

#include <algorithm>

namespace arpg::platform {
namespace {

void append_actor_obstacle(LootLabelObstacleSet& obstacles,
    combat::Vec3 position, const CombatCameraView& camera,
    CameraOffset camera_offset,
    float width, float height) noexcept {
    const ScreenProjection projected = project_combat_position(
        position, camera, width, height);
    constexpr float kHighestResourceBarOffset = 14.0F;
    const float obstacle_top = projected.y
        + monster_presentation_plan(0U, false).resource_bar_offset_y
            * projected.scale
        - kHighestResourceBarOffset + camera_offset.y;
    const float obstacle_bottom = projected.y
        + 14.0F * projected.scale + camera_offset.y;
    static_cast<void>(obstacles.append({
        projected.x - 75.0F * projected.scale + camera_offset.x,
        obstacle_top,
        150.0F * projected.scale,
        obstacle_bottom - obstacle_top,
    }));
}

LootLabelObstacleSet actor_label_obstacles(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current,
    float interpolation_alpha, const CombatCameraView& camera,
    CameraOffset camera_offset,
    float width, float height) noexcept {
    LootLabelObstacleSet obstacles{};
    if (!current.combat.has_value()) return obstacles;

    const combat::CombatSnapshot& current_combat = *current.combat;
    const combat::CombatSnapshot& previous_combat =
        can_interpolate_room(previous, current)
        ? *previous.combat : current_combat;
    const float alpha = std::clamp(interpolation_alpha, 0.0F, 1.0F);
    const combat::Vec3 player_position = interpolate_combat_position(
        previous_combat.player.position,
        current_combat.player.position, alpha);
    append_actor_obstacle(obstacles, player_position,
        camera, camera_offset, width, height);
    for (std::size_t index = 0U;
         index < current_combat.monster_count; ++index) {
        const combat::MonsterSnapshot& monster =
            current_combat.monsters[index];
        if (!monster.active) continue;
        combat::Vec3 position = monster.position;
        const combat::MonsterSnapshot& previous_monster =
            previous_combat.monsters[index];
        if (monster.monster_ordinal == previous_monster.monster_ordinal
                && previous_monster.active) {
            position = interpolate_combat_position(
                previous_monster.position, monster.position, alpha);
        }
        append_actor_obstacle(
            obstacles, position, camera, camera_offset, width, height);
    }
    return obstacles;
}

LootLabelObstacleSet actor_label_obstacles(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonRenderSnapshot& current,
    bool interpolate_previous,
    float interpolation_alpha, const CombatCameraView& camera,
    CameraOffset camera_offset,
    float width, float height) noexcept {
    LootLabelObstacleSet obstacles{};
    if (!current.has_combat) return obstacles;

    const combat::CombatSnapshot& current_combat = current.combat;
    const combat::CombatSnapshot& previous_combat = interpolate_previous
            && previous.combat.has_value()
        ? *previous.combat : current_combat;
    const float alpha = std::clamp(interpolation_alpha, 0.0F, 1.0F);
    const combat::Vec3 player_position = interpolate_combat_position(
        previous_combat.player.position,
        current_combat.player.position, alpha);
    append_actor_obstacle(obstacles, player_position,
        camera, camera_offset, width, height);
    for (std::size_t index = 0U;
         index < current_combat.monster_count; ++index) {
        const combat::MonsterSnapshot& monster =
            current_combat.monsters[index];
        if (!monster.active) continue;
        combat::Vec3 position = monster.position;
        const combat::MonsterSnapshot& previous_monster =
            previous_combat.monsters[index];
        if (interpolate_previous
                && monster.monster_ordinal
                    == previous_monster.monster_ordinal
                && previous_monster.active) {
            position = interpolate_combat_position(
                previous_monster.position, monster.position, alpha);
        }
        append_actor_obstacle(
            obstacles, position, camera, camera_offset, width, height);
    }
    return obstacles;
}

void require_world_room_atlases(MaterialResidencyRequest& request,
    dungeon::DungeonElement ecology) noexcept {
    switch (ecology) {
    case dungeon::DungeonElement::fire:
        request.require(MaterialAtlasId::fire_environment);
        request.require(MaterialAtlasId::fire_room_background);
        break;
    case dungeon::DungeonElement::water:
        request.require(MaterialAtlasId::water_environment);
        request.require(MaterialAtlasId::water_room_background);
        break;
    case dungeon::DungeonElement::lightning:
        request.require(MaterialAtlasId::lightning_environment);
        request.require(MaterialAtlasId::lightning_room_background);
        break;
    case dungeon::DungeonElement::chaos:
        request.require(MaterialAtlasId::chaos_environment);
        request.require(MaterialAtlasId::chaos_room_background);
        break;
    }
}

void require_world_monster_atlas(MaterialResidencyRequest& request,
    combat::MonsterId id) noexcept {
    switch (id) {
    case combat::MonsterId::fire_bomber:
        request.require(MaterialAtlasId::fire_bomber);
        break;
    case combat::MonsterId::fire_charger:
        request.require(MaterialAtlasId::fire_charger);
        break;
    case combat::MonsterId::water_bulwark:
        request.require(MaterialAtlasId::water_bulwark);
        break;
    case combat::MonsterId::water_support:
        request.require(MaterialAtlasId::water_support);
        break;
    case combat::MonsterId::lightning_shooter:
        request.require(MaterialAtlasId::lightning_shooter);
        break;
    case combat::MonsterId::lightning_dasher:
        request.require(MaterialAtlasId::lightning_dasher);
        break;
    case combat::MonsterId::chaos_chaser:
        request.require(MaterialAtlasId::chaos_chaser);
        break;
    case combat::MonsterId::chaos_hazard:
        request.require(MaterialAtlasId::chaos_hazard);
        break;
    case combat::MonsterId::count:
        break;
    }
}

MaterialResidencyRequest world_material_residency_request_impl(
    const dungeon::DungeonRenderSnapshot& world,
    const skills::SkillLoadoutState& skill_loadout) noexcept {
    MaterialResidencyRequest request = base_material_residency_request();
    for (const skills::ActiveSkillSlot& slot : skill_loadout.slots) {
        if (slot.active != skills::ActiveSkillId::none) {
            request.require(active_skill_material_atlas(slot.active));
        }
    }
    if (world.has_active_room) {
        require_world_room_atlases(request, world.ecology);
    }
    if (!world.has_combat) return request;
    const std::size_t monster_count = (std::min)(
        static_cast<std::size_t>(world.combat.monster_count),
        world.combat.monsters.size());
    for (std::size_t index = 0U; index < monster_count; ++index) {
        const combat::MonsterSnapshot& monster = world.combat.monsters[index];
        if (monster.active) require_world_monster_atlas(request, monster.id);
    }
    return request;
}

Color raylib_color(Rgba8 color) noexcept {
    return {color.r, color.g, color.b, color.a};
}

void draw_loot_suction(const LootSuctionPlan& plan,
    const MaterialPack& material_pack) noexcept {
    for (std::size_t index{}; index < plan.count; ++index) {
        const LootSuctionFlight& flight = plan.flights[index];
        const Vector2 center{flight.center.x, flight.center.y};
        const Color color = raylib_color(flight.color);
        if (flight.equipment) {
            const bool halo_drawn = material_pack.draw(flight.rarity_sprite,
                center, false, 0.38F);
            const bool item_drawn = material_pack.draw(flight.sprite,
                center, false, 0.28F, color);
            if (!halo_drawn) {
                DrawCircleLines(static_cast<int>(center.x),
                    static_cast<int>(center.y), 13.0F, color);
            }
            if (!item_drawn) DrawRectangleV(
                {center.x - 4.0F, center.y - 4.0F}, {8.0F, 8.0F}, color);
            continue;
        }

        if (!material_pack.draw(flight.sprite, center, false, 0.24F, color)) {
            DrawCircleV(center, 5.0F, color);
        }
        DrawCircleLines(static_cast<int>(center.x),
            static_cast<int>(center.y), 7.0F, Fade(color, 0.85F));
    }

    if (plan.destination_pulse <= 0.0F) return;
    const Vector2 destination{plan.destination.x, plan.destination.y};
    const float pulse = std::clamp(plan.destination_pulse, 0.0F, 1.0F);
    DrawCircleV(destination, 7.0F + 8.0F * (1.0F - pulse),
        Fade(RAYWHITE, 0.18F * pulse));
    DrawCircleLines(static_cast<int>(destination.x),
        static_cast<int>(destination.y), 10.0F + 10.0F * (1.0F - pulse),
        Fade(RAYWHITE, 0.9F * pulse));
}

}  // namespace

MaterialResidencyRequest world_material_residency_request(
    const dungeon::DungeonRenderSnapshot& world,
    const skills::SkillLoadoutState& skill_loadout) noexcept {
    return world_material_residency_request_impl(world, skill_loadout);
}

MaterialEcology material_ecology(
    dungeon::DungeonElement ecology) noexcept {
    switch (ecology) {
    case dungeon::DungeonElement::fire: return MaterialEcology::fire;
    case dungeon::DungeonElement::water: return MaterialEcology::water;
    case dungeon::DungeonElement::lightning: return MaterialEcology::lightning;
    case dungeon::DungeonElement::chaos: return MaterialEcology::chaos;
    }
    return MaterialEcology::common;
}

CombatRenderPlan make_combat_render_plan(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonRenderSnapshot& current,
    bool interpolate_previous,
    float interpolation_alpha,
    const CombatCameraView& camera,
    CameraOffset camera_offset,
    settings::LootFilterMode mode,
    float width,
    float height) noexcept {
    CombatRenderPlan plan{};
    LootLabelObstacleSet obstacles = actor_label_obstacles(previous, current,
        interpolate_previous, interpolation_alpha, camera, camera_offset,
        width, height);
    plan.ground_loot = build_ground_loot_view(
        current, mode, camera, width, height, obstacles);
    plan.material_loot = build_material_loot_view(
        current, camera, width, height, obstacles);
    plan.stages = {{
        CombatRenderStage::room,
        CombatRenderStage::actors,
        CombatRenderStage::ground_loot_labels,
        CombatRenderStage::normal_hud,
    }};
    plan.stage_count = plan.stages.size();
    return plan;
}

std::optional<std::size_t> hud_presented_frame_index(
    HudPresentedFrame frame) noexcept {
    const std::size_t index = static_cast<std::size_t>(frame);
    return index < static_cast<std::size_t>(HudPresentedFrame::count)
        ? std::optional<std::size_t>{index} : std::nullopt;
}

bool CombatRenderer::initialize_resources() noexcept {
    const bool death_font_ready = death_overlay_.initialize();
    const bool hud_font_ready = hud_renderer_.initialize();
    static_cast<void>(material_pack_.synchronize_residency(
        base_material_residency_request()));
    if (!death_font_ready || !hud_font_ready) {
        TraceLog(LOG_WARNING,
            "HUD overlays are using a fallback font; formal CJK validation will fail");
    }
    return death_font_ready && hud_font_ready;
}

void CombatRenderer::shutdown_resources() noexcept {
    material_pack_.unload();
    hud_renderer_.shutdown();
    death_overlay_.shutdown();
}

bool CombatRenderer::active_skill_assets_ready() const noexcept {
    return active_skill_renderer_.assets_ready();
}

bool CombatRenderer::material_pipeline_ready() const noexcept {
    return material_pack_.material_pipeline_ready();
}

bool CombatRenderer::material_ecology_ready(
    MaterialEcology ecology) const noexcept {
    return material_pack_.ecology_ready(ecology);
}

bool CombatRenderer::material_atlas_available(
    MaterialAtlasId atlas) const noexcept {
    return material_pack_.available(atlas);
}

const MaterialPack& CombatRenderer::material_pack() const noexcept {
    return material_pack_;
}

std::uint64_t CombatRenderer::material_sprite_draw_count(
    MaterialSpriteId sprite) const noexcept {
    return material_pack_.sprite_draw_count(sprite);
}

std::uint64_t CombatRenderer::material_direct_stretch_draw_count(
    MaterialSpriteId sprite) const noexcept {
    return material_pack_.direct_stretch_draw_count(sprite);
}

MonsterMaterialDrawRuntimeStatus CombatRenderer::monster_material_draw_status(
    combat::MonsterId monster) const noexcept {
    const std::size_t index = static_cast<std::size_t>(monster);
    if (index >= monster_material_draw_statuses_.size()) return {};
    return monster_material_draw_statuses_[index];
}

RoomBackgroundDrawRuntimeStatus CombatRenderer::room_background_draw_status()
    const noexcept {
    return room_background_draw_status_;
}

ActiveSkillDrawRuntimeStatus CombatRenderer::active_skill_draw_status()
    const noexcept {
    return active_skill_draw_status_;
}

DoorRenderDecision door_render_decision(
    DoorVisualMode mode,
    dungeon::ExitDirection direction,
    bool full_clear) noexcept {
    const DoorTheme theme = door_theme(direction);
    return {
        select_door_sprite(theme.element),
        theme.label,
        mode == DoorVisualMode::closed
            ? Rgba8{150U, 150U, 150U, 255U}
            : Rgba8{255U, 255U, 255U, 255U},
        theme.frame,
        mode == DoorVisualMode::closed,
        full_clear && mode == DoorVisualMode::open,
    };
}

HoleProjectedGeometry project_hole_geometry(
    combat::Vec3 world_position,
    const CombatCameraView& camera,
    float width,
    float height) noexcept {
    const ScreenProjection projected = project_combat_position(
        world_position, camera, width, height);
    return {{projected.x, projected.ground_y},
        74.0F * projected.scale,
        25.0F * projected.scale,
        2.0F * projected.scale};
}

void CombatRenderer::consume_event(const combat::CombatEvent& event) noexcept {
    last_event_ = event;
    has_last_event_ = true;
}

void CombatRenderer::consume_dungeon_event(
    const dungeon::DungeonEvent& event) noexcept {
    transition_ = transition_after_dungeon_event(transition_, event.kind);
}

void CombatRenderer::clear_combat_transients() noexcept {
    last_event_ = combat::CombatEvent{};
    has_last_event_ = false;
    monster_presenter_.reset();
    loot_suction_.clear();
}

void CombatRenderer::set_loot_filter_mode(
    settings::LootFilterMode mode) noexcept {
    loot_filter_mode_ = mode;
}

void CombatRenderer::update(float frame_seconds) noexcept {
    transition_ = advance_transition(transition_, frame_seconds);
}

void CombatRenderer::publish_passive_tree_blocked() noexcept {
    hud_notices_.publish_passive_tree_blocked();
}

void CombatRenderer::observe_hud(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& control_hints,
    float frame_seconds,
    bool paused,
    const dungeon::DungeonRenderSnapshot* presented_world) noexcept {
    const bool room_changed = previous.room_index != current.room_index
        || previous.room_instance_generation != current.room_instance_generation;
    const bool unsafe_to_play = room_changed
        || runtime_status.indicator == SaveIndicator::error
        || runtime_status.recovery_required || runtime_status.faulted;
    if (unsafe_to_play) {
        loot_suction_.clear();
        loot_suction_.observe(current, current, runtime_status);
    } else {
        loot_suction_.update(frame_seconds, paused);
        // The renderer may first observe the exact frame that commits a
        // pickup.  Attach a receipt-free baseline before processing it.
        loot_suction_.observe(previous, previous, {});
        loot_suction_.observe(previous, current, runtime_status);
    }
    const LootPickupFeedback pickup_feedback =
        loot_pickup_feedback_.observe(runtime_status);
    if (pickup_feedback.ready) {
        hud_notices_.publish_loot_pickup(
            pickup_feedback.text, pickup_feedback.abyss);
    }
    material_pickup_feedback_.update(frame_seconds, paused);
    const MaterialPickupFeedback material_feedback =
        material_pickup_feedback_.observe(current);
    if (material_feedback.ready) {
        hud_notices_.publish_loot_pickup(
            material_feedback.text, material_feedback.emphasized);
    }
    hud_notices_.observe(previous, current, runtime_status, control_hints,
        runtime_status.recovery_required);
    hud_notices_.update(frame_seconds, paused);
    HudViewModel model{};
    hud_projector_.build(
        model, current, runtime_status, control_hints, presented_world);
    attach_notice_view(model, hud_notices_.view());
    hud_model_ = model;
    const std::array<std::uint16_t, skills::kActiveSkillCount> cooldowns =
        current.combat.has_value()
        ? current.combat->skill_cooldowns
        : std::array<std::uint16_t, skills::kActiveSkillCount>{};
    active_skill_hud_model_ = make_active_skill_hud_model(
        current.skill_loadout, cooldowns);
    hud_layout_ = IsWindowReady()
        ? make_hud_layout(GetScreenWidth(), GetScreenHeight(), false)
        : HudLayout{};
    hud_binding_revision_ = control_hints.revision;
    ++hud_observation_count_;
}

void CombatRenderer::observe_presented_hud_frame(
    HudPresentedFrame frame,
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& control_hints,
    float frame_seconds,
    bool paused,
    const dungeon::DungeonRenderSnapshot* presented_world) noexcept {
    const auto index = hud_presented_frame_index(frame);
    if (!index.has_value()) return;
    observe_hud(previous, current, runtime_status, control_hints, frame_seconds,
        paused, presented_world);
    ++hud_presented_frame_counts_[*index];
}

const HudViewModel& CombatRenderer::hud_model() const noexcept {
    return hud_model_;
}

HudNoticeView CombatRenderer::hud_notice_view() const noexcept {
    return hud_notices_.view();
}

std::uint64_t CombatRenderer::hud_binding_revision() const noexcept {
    return hud_binding_revision_;
}

std::uint64_t CombatRenderer::hud_observation_count() const noexcept {
    return hud_observation_count_;
}

HudStaticFormattingDiagnostics
CombatRenderer::hud_static_formatting_diagnostics() const noexcept {
    return hud_projector_.static_formatting_diagnostics();
}

std::uint64_t CombatRenderer::hud_presented_frame_count(
    HudPresentedFrame frame) const noexcept {
    const auto index = hud_presented_frame_index(frame);
    return index.has_value() ? hud_presented_frame_counts_[*index] : 0U;
}

std::size_t CombatRenderer::loot_suction_active_count() const noexcept {
    return loot_suction_.active_count();
}

const ActiveSkillHudModel& CombatRenderer::active_skill_hud_model()
    const noexcept {
    return active_skill_hud_model_;
}

Font CombatRenderer::hud_font() const noexcept {
    return hud_renderer_.hud_font();
}

bool CombatRenderer::hud_font_ready() const noexcept {
    return hud_renderer_.font_ready();
}

GroundLootView CombatRenderer::draw(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current_hud,
    const dungeon::DungeonRenderSnapshot& world,
    const CombatCameraView& camera,
    const DungeonRenderStatus& runtime_status,
    float interpolation_alpha,
    bool draw_debug,
    const CombatFeedback& feedback,
    bool audio_ready) noexcept {
    active_skill_draw_status_ = {};
    static_cast<void>(material_pack_.synchronize_residency(
        world_material_residency_request(world, current_hud.skill_loadout)));
    transition_ = transition_after_room_phase(transition_, world.phase);

    ActiveSkillEffectPlan active_skill_plan{};
    if (world.has_combat) {
        const combat::CombatSnapshot& combat = world.combat;
        const skills::ActiveSkillId active_id = combat.active_skill.id;
        const bool material_ready = active_id != skills::ActiveSkillId::none
            && material_pack_.available(active_skill_material_atlas(active_id));
        active_skill_plan = make_active_skill_effect_plan(combat,
            has_last_event_ ? &last_event_ : nullptr, material_ready);
    }

    const float clamped_interpolation_alpha = std::clamp(
        interpolation_alpha, 0.0F, 1.0F);
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const bool interpolate_previous = can_interpolate_room(
        previous, current_hud) && world.has_combat;
    combat::Vec3 interpolated_player{};
    if (world.has_combat) {
        const combat::Vec3 current_player = world.combat.player.position;
        const combat::Vec3 previous_player = interpolate_previous
            ? previous.combat->player.position
            : current_player;
        interpolated_player = interpolate_combat_position(
            previous_player, current_player, clamped_interpolation_alpha);
    }
    const CameraOffset camera_offset = feedback.camera_offset();
    const CombatRenderPlan render_plan = make_combat_render_plan(
        previous, world, interpolate_previous,
        clamped_interpolation_alpha, camera,
        camera_offset, loot_filter_mode_, width, height);

    Camera2D world_camera{};
    world_camera.offset = {camera_offset.x, camera_offset.y};
    world_camera.zoom = 1.0F;
    BeginMode2D(world_camera);
    bool world_mode = true;
    for (std::size_t index = 0U; index < render_plan.stage_count; ++index) {
        const CombatRenderStage stage = render_plan.stages[index];
        if (stage == CombatRenderStage::ground_loot_labels && world_mode) {
            EndMode2D();
            world_mode = false;
        }
        switch (stage) {
        case CombatRenderStage::room:
            draw_room(world, render_plan.ground_loot,
                render_plan.material_loot, camera);
            draw_abyss_overlay(current_hud);
            break;
        case CombatRenderStage::actors:
            active_skill_draw_status_.base_player_drawn = draw_actors(
                previous, world, active_skill_plan, camera,
                interpolated_player,
                interpolate_previous,
                clamped_interpolation_alpha,
                draw_debug, feedback);
            if (world.has_combat) {
                const bool base_player_drawn =
                    active_skill_draw_status_.base_player_drawn;
                active_skill_draw_status_ = active_skill_renderer_.draw_world(
                    active_skill_plan, material_pack_,
                    camera, width, height);
                active_skill_draw_status_.base_player_drawn =
                    base_player_drawn;
            }
            draw_loot_suction(loot_suction_.build_plan(camera,
                interpolated_player, width, height), material_pack_);
            break;
        case CombatRenderStage::ground_loot_labels:
            hud_renderer_.draw_ground_loot(render_plan.ground_loot);
            hud_renderer_.draw_material_loot(render_plan.material_loot);
            break;
        case CombatRenderStage::normal_hud:
            draw_hud();
            active_skill_renderer_.draw_hud(active_skill_hud_model_,
                active_skill_hud_layout(GetScreenWidth(), GetScreenHeight()),
                hud_renderer_.hud_font(), hud_renderer_.font_ready(),
                material_pack_);
            break;
        }
    }
    if (world_mode) EndMode2D();

    if (draw_debug) {
        const DebugOverlayDiagnosticsPlan diagnostics =
            make_debug_overlay_diagnostics_plan(current_hud,
                hud_model_.diagnostics,
                hud_notices_.dropped_count(), hud_binding_revision_, last_event_,
                has_last_event_, hud_renderer_.font_ready());
        debug_overlay_.draw(current_hud, runtime_status, feedback,
            audio_ready, diagnostics);
    }

    const float overlay_alpha = transition_overlay_alpha(transition_.seconds_left);
    if (overlay_alpha > 0.0F) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
            Fade(BLACK, overlay_alpha));
    }
    death_overlay_.draw(current_hud, material_pack_);
    return render_plan.ground_loot;
}

}  // namespace arpg::platform
