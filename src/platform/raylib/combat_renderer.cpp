#include "combat_renderer.hpp"

#include "debug_overlay_renderer.hpp"
#include "dungeon_runtime.hpp"

#include <raylib.h>

#include <algorithm>

namespace arpg::platform {

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
    const dungeon::DungeonSnapshot& snapshot,
    settings::LootFilterMode mode,
    float width,
    float height) noexcept {
    CombatRenderPlan plan{};
    plan.ground_loot = build_ground_loot_view(snapshot, mode, width, height);
    plan.material_loot = build_material_loot_view(snapshot, width, height);
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

DoorRenderDecision door_render_decision(
    DoorVisualMode mode,
    dungeon::ExitDirection direction) noexcept {
    const DoorTheme theme = door_theme(direction);
    return {
        select_door_sprite(theme.element),
        theme.label,
        mode == DoorVisualMode::closed
            ? Rgba8{150U, 150U, 150U, 255U}
            : Rgba8{255U, 255U, 255U, 255U},
        theme.frame,
        mode == DoorVisualMode::closed,
    };
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
}

void CombatRenderer::set_loot_filter_mode(
    settings::LootFilterMode mode) noexcept {
    loot_filter_mode_ = mode;
}

void CombatRenderer::update(float frame_seconds) noexcept {
    transition_ = advance_transition(transition_, frame_seconds);
}

void CombatRenderer::observe_hud(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& control_hints,
    float frame_seconds,
    bool paused) noexcept {
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
    hud_projector_.build(model, current, runtime_status, control_hints);
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
    bool paused) noexcept {
    const auto index = hud_presented_frame_index(frame);
    if (!index.has_value()) return;
    observe_hud(previous, current, runtime_status, control_hints, frame_seconds, paused);
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
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    float interpolation_alpha,
    bool draw_debug,
    const CombatFeedback& feedback,
    bool audio_ready) noexcept {
    static_cast<void>(material_pack_.synchronize_residency(
        make_material_residency_request(current)));
    transition_ = transition_after_room_phase(transition_, current.phase);

    const CombatRenderPlan render_plan = make_combat_render_plan(current,
        loot_filter_mode_, static_cast<float>(GetScreenWidth()),
        static_cast<float>(GetScreenHeight()));

    const CameraOffset camera_offset = feedback.camera_offset();
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
            draw_room(current, render_plan.ground_loot, render_plan.material_loot);
            break;
        case CombatRenderStage::actors:
            draw_actors(previous, current,
                std::clamp(interpolation_alpha, 0.0F, 1.0F),
                draw_debug, feedback);
            if (current.combat.has_value()) {
                active_skill_renderer_.draw_world(*current.combat,
                    has_last_event_ ? &last_event_ : nullptr,
                    static_cast<float>(GetScreenWidth()),
                    static_cast<float>(GetScreenHeight()), material_pack_);
            }
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
            make_debug_overlay_diagnostics_plan(current, hud_model_.diagnostics,
                hud_notices_.dropped_count(), hud_binding_revision_, last_event_,
                has_last_event_, hud_renderer_.font_ready());
        debug_overlay_.draw(current, runtime_status, feedback, audio_ready, diagnostics);
    }

    const float overlay_alpha = transition_overlay_alpha(transition_.seconds_left);
    if (overlay_alpha > 0.0F) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
            Fade(BLACK, overlay_alpha));
    }
    death_overlay_.draw(current);
    return render_plan.ground_loot;
}

}  // namespace arpg::platform
