#include "test_framework.hpp"

#include "combat_renderer.hpp"
#include "combat_view_math.hpp"
#include "control_hints.hpp"
#include "debug_overlay_renderer.hpp"
#include "dungeon_runtime.hpp"
#include "ground_loot_view.hpp"
#include "hud_font.hpp"
#include "pause_menu_state.hpp"
#include "platform/settings/settings_types.hpp"
#include "raylib_host.hpp"

#include <cstring>

namespace {

namespace dungeon = arpg::dungeon;
namespace items = arpg::items;
namespace platform = arpg::platform;
namespace settings = arpg::settings;

dungeon::GroundItemSnapshot ground_item(
    std::uint16_t ordinal,
    items::ItemRarity rarity) noexcept {
    dungeon::GroundItemSnapshot item{};
    item.ordinal = ordinal;
    item.item_id = 1000U + ordinal;
    item.base_id = 1U;
    item.item_level = 20U;
    item.rarity = rarity;
    return item;
}

platform::ControlHints committed_hints(std::uint64_t revision) noexcept {
    platform::ControlHints hints{};
    static constexpr char kPrimary[] = "WASD Move";
    static_assert(sizeof(kPrimary) <= 160U);
    std::memcpy(hints.primary.data(), kPrimary, sizeof(kPrimary));
    hints.revision = revision;
    return hints;
}

dungeon::DungeonSnapshot snapshot() noexcept {
    dungeon::DungeonSnapshot value{};
    value.commit_generation = 7U;
    value.room_index = 3U;
    value.phase = dungeon::RoomPhase::combat;
    value.has_active_room = true;
    value.remaining_targets = 2U;
    value.progression.level = 4U;
    return value;
}

platform::DungeonRenderStatus saved_status() noexcept {
    platform::DungeonRenderStatus status{};
    status.indicator = platform::SaveIndicator::saved;
    return status;
}

platform::DungeonRenderStatus recovery_status() noexcept {
    platform::DungeonRenderStatus status{};
    status.indicator = platform::SaveIndicator::error;
    status.recovery_required = true;
    return status;
}

arpg::test::Failure observation_is_once_per_presented_frame_and_read_only() noexcept {
    const dungeon::DungeonSnapshot previous = snapshot();
    const dungeon::DungeonSnapshot current = snapshot();
    platform::CombatRenderer renderer{};

    renderer.observe_hud(previous, current, saved_status(), committed_hints(3U),
        1.0F / 60.0F, false);
    ARPG_REQUIRE(renderer.hud_observation_count() == 1U);
    ARPG_REQUIRE(current.commit_generation == 7U);
    ARPG_REQUIRE(current.room_index == 3U);
    ARPG_REQUIRE(current.remaining_targets == 2U);

    // Recovery/death-owned presentation frames still observe the committed snapshots.
    renderer.observe_hud(previous, current, saved_status(), committed_hints(3U),
        1.0F / 60.0F, false);
    ARPG_REQUIRE(renderer.hud_observation_count() == 2U);
    return {};
}

arpg::test::Failure production_presentation_seam_observes_normal_recovery_and_death() noexcept {
    const dungeon::DungeonSnapshot previous = snapshot();
    dungeon::DungeonSnapshot current = previous;
    platform::CombatRenderer renderer{};

    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        previous, current, saved_status(), committed_hints(11U), 0.1F, false);
    ARPG_REQUIRE(renderer.hud_presented_frame_count(
        platform::HudPresentedFrame::normal) == 1U);
    ARPG_REQUIRE(renderer.hud_binding_revision() == 11U);

    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::recovery,
        current, current, recovery_status(), committed_hints(12U), 1.0F, true);
    ARPG_REQUIRE(renderer.hud_presented_frame_count(
        platform::HudPresentedFrame::recovery) == 1U);
    ARPG_REQUIRE(renderer.hud_notice_view().primary.kind
        == platform::HudNoticeKind::save_error);
    ARPG_REQUIRE(renderer.hud_notice_view().secondary.kind
        == platform::HudNoticeKind::recovery_required);
    ARPG_REQUIRE(renderer.hud_binding_revision() == 12U);

    current.death.emplace();
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::death_overlay,
        previous, current, saved_status(), committed_hints(13U), 0.1F, false);
    ARPG_REQUIRE(renderer.hud_presented_frame_count(
        platform::HudPresentedFrame::death_overlay) == 1U);
    ARPG_REQUIRE(renderer.hud_observation_count() == 3U);
    ARPG_REQUIRE(renderer.hud_binding_revision() == 13U);
    return {};
}

arpg::test::Failure presentation_frame_sentinel_rejects_invalid_enum_values() noexcept {
    const dungeon::DungeonSnapshot value = snapshot();
    dungeon::DungeonSnapshot changed = value;
    changed.depth = 99U;
    platform::CombatRenderer renderer{};
    const auto invalid = static_cast<platform::HudPresentedFrame>(255U);

    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        value, value, saved_status(), committed_hints(14U), 0.1F, false);
    const platform::HudViewModel model_before = renderer.hud_model();
    const platform::HudNoticeView notices_before = renderer.hud_notice_view();
    const std::uint64_t observations_before = renderer.hud_observation_count();
    const std::uint64_t revision_before = renderer.hud_binding_revision();
    ARPG_REQUIRE(!platform::hud_presented_frame_index(invalid).has_value());
    ARPG_REQUIRE(!platform::hud_presented_frame_index(
        platform::HudPresentedFrame::count).has_value());
    renderer.observe_presented_hud_frame(invalid, value, changed, saved_status(),
        committed_hints(15U), 0.1F, false);
    ARPG_REQUIRE(renderer.hud_observation_count() == observations_before);
    ARPG_REQUIRE(renderer.hud_presented_frame_count(invalid) == 0U);
    ARPG_REQUIRE(renderer.hud_presented_frame_count(
        platform::HudPresentedFrame::count) == 0U);
    ARPG_REQUIRE(renderer.hud_binding_revision() == revision_before);
    ARPG_REQUIRE(renderer.hud_model().navigation.depth
        == model_before.navigation.depth);
    ARPG_REQUIRE(renderer.hud_notice_view().primary.kind
        == notices_before.primary.kind);
    return {};
}

arpg::test::Failure debug_diagnostics_plan_carries_all_f1_only_counters() noexcept {
    dungeon::DungeonSnapshot value = snapshot();
    value.encounter.total_budget = 88U;
    value.encounter.current_wave_budget = 21U;
    value.diagnostics.ground_saturation_count = 5U;
    value.diagnostics.room_index_overflow = true;
    value.combat.emplace();
    value.combat->monster_count = 7U;
    value.combat->projectile_count = 9U;
    value.combat->hazard_count = 3U;
    value.combat->diagnostics.projectile_saturation_count = 2U;
    value.combat->diagnostics.projectile_invalid_owner_count = 4U;
    value.combat->diagnostics.hazard_saturation_count = 6U;
    value.combat->diagnostics.hazard_invalid_owner_count = 8U;
    arpg::combat::CombatEvent event{};
    event.kind = arpg::combat::CombatEventKind::hit;
    event.target_ordinal = 5U;
    platform::HudBuildDiagnostics hud{};
    hud.clamped_values = 1U;
    hud.truncated_texts = 2U;
    hud.combat_snapshot_missing = false;

    const platform::DebugOverlayDiagnosticsPlan plan =
        platform::make_debug_overlay_diagnostics_plan(value, hud, 10U, 99U,
            event, true, true);
    ARPG_REQUIRE(plan.total_budget == 88U);
    ARPG_REQUIRE(plan.current_wave_budget == 21U);
    ARPG_REQUIRE(plan.active_monsters == 7U);
    ARPG_REQUIRE(plan.active_projectiles == 9U);
    ARPG_REQUIRE(plan.active_hazards == 3U);
    ARPG_REQUIRE(plan.projectile_saturation == 2U);
    ARPG_REQUIRE(plan.projectile_invalid_owner == 4U);
    ARPG_REQUIRE(plan.hazard_saturation == 6U);
    ARPG_REQUIRE(plan.hazard_invalid_owner == 8U);
    ARPG_REQUIRE(plan.ground_saturation == 5U);
    ARPG_REQUIRE(plan.room_index_overflow);
    ARPG_REQUIRE(plan.notice_drops == 10U);
    ARPG_REQUIRE(plan.binding_revision == 99U);
    ARPG_REQUIRE(plan.has_last_event);
    ARPG_REQUIRE(plan.last_event.target_ordinal == 5U);
    ARPG_REQUIRE(plan.cjk_font_ready);
    return {};
}

arpg::test::Failure fallback_font_selection_matches_actual_draw_and_shutdown_ownership() noexcept {
    const platform::HudFontSelectionPlan fallback =
        platform::make_hud_font_selection_plan(false);
    ARPG_REQUIRE(fallback.draw_mode == platform::HudFontDrawMode::fallback);
    ARPG_REQUIRE(fallback.use_default_font);
    ARPG_REQUIRE(!fallback.owns_loaded_font);

    const platform::HudFontSelectionPlan cjk =
        platform::make_hud_font_selection_plan(true);
    ARPG_REQUIRE(cjk.draw_mode == platform::HudFontDrawMode::cjk_ready);
    ARPG_REQUIRE(!cjk.use_default_font);
    ARPG_REQUIRE(cjk.owns_loaded_font);
    return {};
}

arpg::test::Failure paused_frames_freeze_hud_notice_time() noexcept {
    const dungeon::DungeonSnapshot previous = snapshot();
    dungeon::DungeonSnapshot current = previous;
    current.last_room_experience = 25U;
    platform::CombatRenderer renderer{};

    renderer.observe_hud(previous, current, saved_status(), committed_hints(4U),
        3.0F, true);
    const float paused_seconds = renderer.hud_notice_view().primary.seconds_left;
    renderer.observe_hud(previous, current, saved_status(), committed_hints(4U),
        1.25F, true);
    ARPG_REQUIRE(arpg::test::near(
        renderer.hud_notice_view().primary.seconds_left, paused_seconds));
    return {};
}

arpg::test::Failure committed_settings_hints_and_revision_are_used_after_apply() noexcept {
    const dungeon::DungeonSnapshot value = snapshot();
    platform::CombatRenderer renderer{};

    renderer.observe_hud(value, value, saved_status(), committed_hints(42U),
        0.0F, false);
    ARPG_REQUIRE(renderer.hud_binding_revision() == 42U);
    ARPG_REQUIRE(renderer.hud_model().diagnostics.combat_snapshot_missing);
    return {};
}

arpg::test::Failure observation_publishes_the_prebuilt_hud_model() noexcept {
    const dungeon::DungeonSnapshot value = snapshot();
    platform::CombatRenderer renderer{};
    renderer.observe_hud(value, value, saved_status(), committed_hints(8U),
        0.0F, false);
    const platform::HudViewModel before = renderer.hud_model();

    // Drawing is raylib-owned; this headless contract verifies the draw input
    // is fully prepared before the render phase and is not rebuilt from later hints.
    ARPG_REQUIRE(renderer.hud_observation_count() == 1U);
    ARPG_REQUIRE(renderer.hud_model().navigation.depth == before.navigation.depth);
    ARPG_REQUIRE(renderer.hud_binding_revision() == 8U);
    return {};
}

arpg::test::Failure static_text_cache_rebuilds_only_changed_fragments() noexcept {
    dungeon::DungeonSnapshot previous = snapshot();
    previous.combat.emplace();
    previous.combat->player.hp = 90;
    previous.combat->player.max_hp = 100;
    dungeon::DungeonSnapshot current = previous;
    platform::CombatRenderer renderer{};
    const platform::ControlHints hints = committed_hints(21U);

    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        previous, current, saved_status(), hints, 0.1F, false);
    const platform::HudStaticFormattingDiagnostics initial =
        renderer.hud_static_formatting_diagnostics();
    ARPG_REQUIRE(initial.objective_rebuilds == 1U);
    ARPG_REQUIRE(initial.navigation_rebuilds == 1U);
    ARPG_REQUIRE(initial.control_hint_rebuilds == 1U);

    // The three valid frame owners share one production projection cache.
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::recovery,
        current, current, recovery_status(), hints, 0.1F, true);
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::death_overlay,
        current, current, saved_status(), hints, 0.1F, false);
    ARPG_REQUIRE(renderer.hud_static_formatting_diagnostics()
        == initial);

    // Dynamic player values still refresh without rebuilding static fragments.
    dungeon::DungeonSnapshot dynamic = current;
    dynamic.combat->player.hp = 31;
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        current, dynamic, saved_status(), hints, 0.1F, false);
    ARPG_REQUIRE(renderer.hud_model().player.hp == 31);
    ARPG_REQUIRE(renderer.hud_static_formatting_diagnostics()
        == initial);

    // Objective, navigation and committed bindings invalidate independently.
    dungeon::DungeonSnapshot changed_objective = dynamic;
    changed_objective.remaining_targets = 1U;
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        dynamic, changed_objective, saved_status(), hints, 0.1F, false);
    platform::HudStaticFormattingDiagnostics expected = initial;
    ++expected.objective_rebuilds;
    ARPG_REQUIRE(renderer.hud_model().room.remaining_targets == 1U);
    ARPG_REQUIRE(renderer.hud_static_formatting_diagnostics() == expected);

    dungeon::DungeonSnapshot changed_navigation = changed_objective;
    changed_navigation.depth = 22U;
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        changed_objective, changed_navigation, saved_status(), hints, 0.1F, false);
    ++expected.navigation_rebuilds;
    ARPG_REQUIRE(renderer.hud_model().navigation.depth == 22U);
    ARPG_REQUIRE(renderer.hud_static_formatting_diagnostics() == expected);

    platform::ControlHints rebound = hints;
    rebound.revision = 22U;
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        changed_navigation, changed_navigation, saved_status(), rebound,
        0.1F, false);
    ++expected.control_hint_rebuilds;
    ARPG_REQUIRE(renderer.hud_static_formatting_diagnostics() == expected);

    // Presentation-only notices remain live, while an invalid owner is inert.
    dungeon::DungeonSnapshot rewarded = changed_navigation;
    rewarded.last_room_experience = 25U;
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        changed_navigation, rewarded, saved_status(), rebound, 0.1F, false);
    ARPG_REQUIRE(renderer.hud_notice_view().primary.kind
        == platform::HudNoticeKind::reward);
    ARPG_REQUIRE(renderer.hud_static_formatting_diagnostics() == expected);

    const auto invalid = static_cast<platform::HudPresentedFrame>(255U);
    renderer.observe_presented_hud_frame(invalid, rewarded, current,
        recovery_status(), committed_hints(999U), 0.1F, false);
    ARPG_REQUIRE(renderer.hud_static_formatting_diagnostics() == expected);
    ARPG_REQUIRE(renderer.hud_model().navigation.depth == 22U);
    return {};
}

arpg::test::Failure ground_loot_render_plan_reuses_one_view_and_orders_stages()
    noexcept {
    dungeon::DungeonSnapshot previous = snapshot();
    previous.combat.emplace();
    previous.combat->player.position = {8.0F, 0.0F, 0.0F};
    previous.combat->monsters[0].active = true;
    previous.combat->monsters[0].generation = 5U;
    previous.combat->monsters[0].position = {-4.0F, 0.0F, 0.0F};
    previous.combat->monster_count = 1U;
    dungeon::DungeonSnapshot value = previous;
    value.combat->monsters[0].position = {4.0F, 0.0F, 0.0F};
    dungeon::DungeonRenderSnapshot world{};
    world.has_active_room = true;
    world.has_combat = true;
    world.combat = *value.combat;
    world.equipment[0] = ground_item(30U, items::ItemRarity::rare);
    world.equipment[1] = ground_item(10U, items::ItemRarity::normal);
    world.equipment[2] = ground_item(20U, items::ItemRarity::magic);
    world.equipment_count = 3U;
    world.material_count = 1U;
    world.materials[0] = {40U,
        dungeon::GroundMaterialSource::monster_common,
        {0.0F, 0.0F, 0.0F}, items::MaterialId::chaos};
    constexpr platform::CameraOffset kCameraOffset{11.0F, -7.0F};
    const platform::CombatCameraView camera =
        platform::make_combat_camera_view(
            {8.0F, 0.0F, 0.0F}, 1280.0F, 720.0F);

    const platform::CombatRenderPlan plan = platform::make_combat_render_plan(
        previous, world, true, 0.5F, camera, kCameraOffset,
        settings::LootFilterMode::magic_or_better, 1280.0F, 720.0F);
    const platform::GroundLootView& room_stage_view = plan.ground_loot;
    const platform::GroundLootView& label_stage_view = plan.ground_loot;

    ARPG_REQUIRE(&room_stage_view == &plan.ground_loot);
    ARPG_REQUIRE(&label_stage_view == &plan.ground_loot);
    ARPG_REQUIRE(&room_stage_view == &label_stage_view);
    ARPG_REQUIRE(plan.ground_loot.count == 2U);
    ARPG_REQUIRE(plan.ground_loot.labels[0].ordinal == 20U);
    ARPG_REQUIRE(plan.ground_loot.labels[1].ordinal == 30U);
    ARPG_REQUIRE(plan.material_loot.count == 1U);
    const platform::ScreenProjection actor_projection =
        platform::project_combat_position({0.0F, 0.0F, 0.0F},
            camera, 1280.0F, 720.0F);
    ARPG_REQUIRE(plan.ground_loot.labels[0].anchor_x == actor_projection.x);
    ARPG_REQUIRE(plan.ground_loot.labels[0].anchor_y == actor_projection.y);
    ARPG_REQUIRE(plan.material_loot.labels[0].anchor_x == actor_projection.x);
    ARPG_REQUIRE(plan.material_loot.labels[0].anchor_y == actor_projection.y);
    const platform::LootLabelRect actor_rect{
        actor_projection.x - 75.0F * actor_projection.scale + kCameraOffset.x,
        actor_projection.y - 119.0F * actor_projection.scale + kCameraOffset.y,
        150.0F * actor_projection.scale,
        133.0F * actor_projection.scale};
    for (std::size_t index = 0U; index < plan.ground_loot.count; ++index) {
        ARPG_REQUIRE(!platform::loot_label_rects_overlap(
            plan.ground_loot.labels[index].rect, actor_rect));
        ARPG_REQUIRE(!platform::loot_label_rects_overlap(
            plan.ground_loot.labels[index].rect,
            plan.material_loot.labels[0].rect));
    }
    ARPG_REQUIRE(!platform::loot_label_rects_overlap(
        plan.material_loot.labels[0].rect, actor_rect));
    ARPG_REQUIRE(plan.stage_count == 4U);
    ARPG_REQUIRE(plan.stages[0] == platform::CombatRenderStage::room);
    ARPG_REQUIRE(plan.stages[1] == platform::CombatRenderStage::actors);
    ARPG_REQUIRE(plan.stages[2]
        == platform::CombatRenderStage::ground_loot_labels);
    ARPG_REQUIRE(plan.stages[3] == platform::CombatRenderStage::normal_hud);
    return {};
}

arpg::test::Failure ground_loot_labels_avoid_highest_monster_resource_bar()
    noexcept {
    dungeon::DungeonSnapshot previous = snapshot();
    previous.combat.emplace();
    previous.combat->player.position = {8.0F, 0.0F, 0.0F};
    previous.combat->monsters[0].active = true;
    previous.combat->monsters[0].generation = 5U;
    previous.combat->monsters[0].position = {0.0F, 0.0F, 0.0F};
    previous.combat->monster_count = 1U;

    dungeon::DungeonRenderSnapshot world{};
    world.has_active_room = true;
    world.has_combat = true;
    world.combat = *previous.combat;
    world.health_potion_count = 1U;
    world.health_potions[0] = {0U, 1U, {0.0F, 0.0F, 1.5F}};
    const platform::CombatCameraView camera =
        platform::make_combat_camera_view(
            previous.combat->player.position, 1280.0F, 720.0F);

    const platform::CombatRenderPlan plan = platform::make_combat_render_plan(
        previous, world, false, 0.0F, camera, {},
        settings::LootFilterMode::show_all, 1280.0F, 720.0F);

    ARPG_REQUIRE(plan.material_loot.count == 1U);
    const platform::ScreenProjection actor =
        platform::project_combat_position(
            world.combat.monsters[0].position, camera, 1280.0F, 720.0F);
    const platform::LootLabelRect highest_resource_bar{
        actor.x - 27.0F * actor.scale,
        actor.y - 128.0F * actor.scale - 14.0F,
        54.0F * actor.scale,
        5.0F};
    ARPG_REQUIRE(!platform::loot_label_rects_overlap(
        plan.material_loot.labels[0].rect, highest_resource_bar));
    return {};
}

arpg::test::Failure renderer_uses_draft_only_on_the_settings_screen() noexcept {
    settings::SettingsData live = settings::default_settings();
    live.loot_filter_mode = settings::LootFilterMode::magic_or_better;
    settings::SettingsData draft = live;
    draft.loot_filter_mode = settings::LootFilterMode::rare_only;

    ARPG_REQUIRE(platform::renderer_loot_filter_mode(
        platform::PauseScreen::settings, live, draft)
        == settings::LootFilterMode::rare_only);
    constexpr platform::PauseScreen kCommittedScreens[] = {
        platform::PauseScreen::closed,
        platform::PauseScreen::root,
        platform::PauseScreen::capture_binding,
        platform::PauseScreen::quit_confirm,
    };
    for (const platform::PauseScreen screen : kCommittedScreens) {
        ARPG_REQUIRE(platform::renderer_loot_filter_mode(screen, live, draft)
            == settings::LootFilterMode::magic_or_better);
    }
    return {};
}

arpg::test::Failure renderer_observes_committed_pickup_receipts_before_notices()
    noexcept {
    platform::CombatRenderer renderer{};
    const auto current = snapshot();
    const auto hints = committed_hints(1U);
    platform::DungeonRenderStatus baseline = saved_status();
    baseline.loot_pickup = {true, 7U, 70U, 3U, 24U,
        items::ItemRarity::rare, dungeon::GroundItemSource::monster_drop};
    baseline.loot_pickup.position = {3.0F, -2.0F, 0.0F};
    baseline.loot_pickup.slot = items::ItemSlot::chest;
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        current, current, baseline, hints, 0.0F, false);
    ARPG_REQUIRE(renderer.hud_notice_view().primary.kind
        != platform::HudNoticeKind::loot_pickup);

    platform::DungeonRenderStatus committed = baseline;
    committed.loot_pickup.commit_generation = 8U;
    committed.loot_pickup.item_id = 80U;
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        current, current, committed, hints, 0.0F, false);
    ARPG_REQUIRE(renderer.hud_notice_view().primary.kind
        == platform::HudNoticeKind::loot_pickup);
    ARPG_REQUIRE(std::strcmp(renderer.hud_notice_view().primary.text.bytes.data(),
        u8"已拾取：稀有 Ward Coat · i24") == 0);

    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        current, current, committed, hints, 1.0F, false);
    ARPG_REQUIRE(arpg::test::near(
        renderer.hud_notice_view().primary.seconds_left, 2.0F));
    return {};
}

arpg::test::Failure abyss_pickup_cross_room_keeps_purple_context_end_to_end()
    noexcept {
    platform::CombatRenderer renderer{};
    const auto hints = committed_hints(2U);
    dungeon::DungeonSnapshot previous = snapshot();
    platform::DungeonRenderStatus empty = saved_status();
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        previous, previous, empty, hints, 0.0F, false);

    dungeon::DungeonSnapshot next = previous;
    ++next.room_index;
    ++next.commit_generation;
    platform::DungeonRenderStatus claimed = saved_status();
    claimed.loot_pickup = {true, next.commit_generation, 0xAB155U,
        3U, 24U, items::ItemRarity::rare,
        dungeon::GroundItemSource::abyss_chest};
    claimed.loot_pickup.position = {3.0F, -2.0F, 0.0F};
    claimed.loot_pickup.slot = items::ItemSlot::chest;
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        previous, next, claimed, hints, 0.0F, false);

    const auto notice = renderer.hud_notice_view().primary;
    ARPG_REQUIRE(notice.kind == platform::HudNoticeKind::loot_pickup);
    ARPG_REQUIRE(notice.abyss);
    ARPG_REQUIRE(notice.priority == 60U);
    ARPG_REQUIRE(renderer.hud_model().context.primary_kind
        == platform::HudNoticeKind::loot_pickup);
    ARPG_REQUIRE(renderer.hud_model().context.primary_abyss);
    const platform::HudLayout layout =
        platform::make_hud_layout(1280, 720, false);
    const platform::ContextPanelPlan plan =
        platform::make_context_panel_plan(renderer.hud_model().context, layout);
    ARPG_REQUIRE(plan.primary_visible);
    ARPG_REQUIRE(plan.primary_abyss);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"observes every presented frame read only", &observation_is_once_per_presented_frame_and_read_only},
    {"production presentation seam covers all owners", &production_presentation_seam_observes_normal_recovery_and_death},
    {"presentation frame sentinel is safe", &presentation_frame_sentinel_rejects_invalid_enum_values},
    {"debug diagnostics plan covers F1 counters", &debug_diagnostics_plan_carries_all_f1_only_counters},
    {"fallback font selection owns no default font", &fallback_font_selection_matches_actual_draw_and_shutdown_ownership},
    {"paused hud notices freeze", &paused_frames_freeze_hud_notice_time},
    {"settings apply uses committed hints", &committed_settings_hints_and_revision_are_used_after_apply},
    {"observation publishes prebuilt hud model", &observation_publishes_the_prebuilt_hud_model},
    {"static HUD text cache invalidates by fragment", &static_text_cache_rebuilds_only_changed_fragments},
    {"ground loot render plan reuses view and orders stages",
        &ground_loot_render_plan_reuses_one_view_and_orders_stages},
    {"ground loot avoids highest monster resource bar",
        &ground_loot_labels_avoid_highest_monster_resource_bar},
    {"renderer draft is settings-only",
        &renderer_uses_draft_only_on_the_settings_screen},
    {"renderer observes committed pickup receipt",
        &renderer_observes_committed_pickup_receipts_before_notices},
    {"abyss pickup keeps purple context across room",
        &abyss_pickup_cross_room_keeps_purple_context_end_to_end},
};

}  // namespace

arpg::test::TestSuite hud_host_integration_suite() noexcept {
    return arpg::test::make_suite("hud_host_integration", kCases);
}
