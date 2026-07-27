#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "combat_renderer.hpp"
#include "material_loot_view.hpp"
#include "material_manifest.hpp"
#include "combat_view_math.hpp"
#include "combat/room_bounds.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

namespace dungeon = arpg::dungeon;
namespace items = arpg::items;
namespace platform = arpg::platform;

constexpr float kExpectedLabelWidth = 190.0F;
constexpr float kExpectedLabelHeight = 23.0F;
constexpr float kExpectedLabelGap = 13.0F;
constexpr float kExpectedPlacementGap = 3.0F;
constexpr float kPlacementEpsilon = 0.001F;

[[nodiscard]] bool nearly_equal(float left, float right) noexcept {
    return std::fabs(left - right) <= kPlacementEpsilon;
}

[[nodiscard]] bool fits_within_bytes(
    std::size_t actual, std::size_t maximum) noexcept {
    return actual <= maximum;
}

[[nodiscard]] platform::LootLabelRect expected_original_label_rect(
    platform::ScreenProjection projection, float width, float height) noexcept {
    platform::LootLabelRect expected{
        projection.x - kExpectedLabelWidth * 0.5F,
        projection.y - kExpectedLabelGap - kExpectedLabelHeight,
        kExpectedLabelWidth,
        kExpectedLabelHeight};
    const float usable_width = (std::max)(
        platform::kGroundLootSafetyInset * 2.0F, width);
    const float usable_height = (std::max)(
        platform::kGroundLootSafetyInset * 2.0F, height);
    expected.width = (std::min)(expected.width,
        usable_width - platform::kGroundLootSafetyInset * 2.0F);
    expected.height = (std::min)(expected.height,
        usable_height - platform::kGroundLootSafetyInset * 2.0F);
    expected.x = std::clamp(expected.x, platform::kGroundLootSafetyInset,
        usable_width - platform::kGroundLootSafetyInset - expected.width);
    expected.y = std::clamp(expected.y, platform::kGroundLootSafetyInset,
        usable_height - platform::kGroundLootSafetyInset - expected.height);
    return expected;
}

arpg::test::Failure require_rect_near(
    platform::LootLabelRect actual,
    platform::LootLabelRect expected) noexcept {
    ARPG_REQUIRE(nearly_equal(actual.x, expected.x));
    ARPG_REQUIRE(nearly_equal(actual.y, expected.y));
    ARPG_REQUIRE(nearly_equal(actual.width, expected.width));
    ARPG_REQUIRE(nearly_equal(actual.height, expected.height));
    return {};
}

arpg::test::Failure labels_and_emphasis_are_player_facing() noexcept {
    ARPG_REQUIRE(platform::material_label(items::MaterialId::coupon_12)
        == "+12强化券");
    ARPG_REQUIRE(platform::material_label(items::MaterialId::reinforcement_stone)
        == "强化石");
    ARPG_REQUIRE(platform::material_is_emphasized(items::MaterialId::exalt));
    ARPG_REQUIRE(platform::material_is_emphasized(items::MaterialId::directed));
    ARPG_REQUIRE(platform::material_is_emphasized(items::MaterialId::coupon_12));
    ARPG_REQUIRE(platform::material_is_emphasized(items::MaterialId::coupon_15));
    ARPG_REQUIRE(!platform::material_is_emphasized(items::MaterialId::chaos));
    return {};
}

arpg::test::Failure material_view_ignores_equipment_filter_and_orders_ordinals() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.ground_material_count = 2U;
    snapshot.ground_materials[0] = {18U,
        dungeon::GroundMaterialSource::monster_common, {2.0F, 1.0F, 0.0F},
        items::MaterialId::transmute};
    snapshot.ground_materials[1] = {4U,
        dungeon::GroundMaterialSource::monster_coupon, {-2.0F, 1.0F, 0.0F},
        items::MaterialId::coupon_12};

    const platform::MaterialLootView view =
        platform::build_material_loot_view(snapshot, 1280.0F, 720.0F);
    ARPG_REQUIRE(view.count == 2U);
    ARPG_REQUIRE(view.labels[0].ordinal == 4U);
    ARPG_REQUIRE(view.labels[1].ordinal == 18U);
    ARPG_REQUIRE(view.labels[0].text == "+12强化券");
    ARPG_REQUIRE(view.labels[0].emphasized);
    ARPG_REQUIRE(!view.labels[1].emphasized);
    return {};
}

arpg::test::Failure pickup_feedback_aggregates_counts_by_material() noexcept {
    platform::MaterialPickupFeedbackState feedback{};
    dungeon::DungeonSnapshot snapshot{};
    static_cast<void>(feedback.observe(snapshot));

    snapshot.material_pickup_receipt.valid = true;
    snapshot.material_pickup_receipt.commit_generation = 10U;
    snapshot.material_pickup_receipt.counts[
        items::material_index(items::MaterialId::chaos)] = 2U;
    auto first = feedback.observe(snapshot);
    ARPG_REQUIRE(first.ready);
    ARPG_REQUIRE(std::strcmp(first.text.bytes.data(), "已拾取：混沌石 x2") == 0);

    snapshot.material_pickup_receipt.commit_generation = 11U;
    snapshot.material_pickup_receipt.counts.fill(0U);
    snapshot.material_pickup_receipt.counts[
        items::material_index(items::MaterialId::chaos)] = 1U;
    auto repeated = feedback.observe(snapshot);
    ARPG_REQUIRE(repeated.ready);
    ARPG_REQUIRE(std::strcmp(repeated.text.bytes.data(), "已拾取：混沌石 x3") == 0);

    snapshot.material_pickup_receipt.commit_generation = 12U;
    snapshot.material_pickup_receipt.counts.fill(0U);
    snapshot.material_pickup_receipt.counts[
        items::material_index(items::MaterialId::directed)] = 1U;
    const auto mixed = feedback.observe(snapshot);
    ARPG_REQUIRE(mixed.ready);
    ARPG_REQUIRE(std::strstr(mixed.text.bytes.data(), "混沌石 x3") != nullptr);
    ARPG_REQUIRE(std::strstr(mixed.text.bytes.data(), "定向核心 x1") != nullptr);
    return {};
}

arpg::test::Failure every_material_has_a_unique_authored_resource() noexcept {
    std::array<platform::MaterialSpriteId, items::kMaterialCount> sprites{};
    const platform::MaterialManifestDefinition manifest =
        platform::default_material_manifest();
    for (std::size_t index{}; index < sprites.size(); ++index) {
        const auto id = static_cast<items::MaterialId>(index);
        sprites[index] = platform::material_loot_sprite(id);
        ARPG_REQUIRE(sprites[index] != platform::MaterialSpriteId::missing);
        const auto* frame = platform::find_material_frame(manifest, sprites[index]);
        ARPG_REQUIRE(frame != nullptr);
        ARPG_REQUIRE(frame->atlas == platform::MaterialAtlasId::items_ui);
        ARPG_REQUIRE(frame->material_class == platform::MaterialClass::loot);
        ARPG_REQUIRE(frame->foot_anchor.x > 0.0F);
        ARPG_REQUIRE(frame->foot_anchor.y > 0.0F);
        ARPG_REQUIRE(frame->perceptual_hash != 0U);
        for (std::size_t previous{}; previous < index; ++previous) {
            ARPG_REQUIRE(sprites[index] != sprites[previous]);
        }
    }
    return {};
}

arpg::test::Failure health_potion_uses_dedicated_sprite_and_pure_red_label() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.ground_health_potion_count = 1U;
    snapshot.ground_health_potions[0] = {7U, 15U, {2.0F, 1.0F, 0.0F}};
    const platform::MaterialLootView view =
        platform::build_material_loot_view(snapshot, 1280.0F, 720.0F);
    ARPG_REQUIRE(view.count == 1U);
    ARPG_REQUIRE(view.labels[0].kind
        == platform::SecondaryLootKind::health_potion);
    ARPG_REQUIRE(view.labels[0].sprite
        == platform::MaterialSpriteId::health_potion);
    ARPG_REQUIRE(view.labels[0].text_color.r == 255U);
    ARPG_REQUIRE(view.labels[0].text_color.g == 48U);
    ARPG_REQUIRE(view.labels[0].text_color.b == 48U);
    ARPG_REQUIRE(view.labels[0].text_color.a == 255U);
    ARPG_REQUIRE(view.labels[0].emphasized);
    ARPG_REQUIRE(view.labels[0].text == "生命药");
    const auto manifest = platform::default_material_manifest();
    const auto* frame = platform::find_material_frame(
        manifest, platform::MaterialSpriteId::health_potion);
    ARPG_REQUIRE(frame != nullptr);
    ARPG_REQUIRE(frame->atlas == platform::MaterialAtlasId::items_ui);
    ARPG_REQUIRE(frame->source.x == 768.0F);
    ARPG_REQUIRE(frame->source.y == 384.0F);
    return {};
}

arpg::test::Failure overlapping_secondary_loot_labels_are_resolved() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.ground_health_potion_count = 2U;
    snapshot.ground_health_potions[0] = {2U, 5U, {0.0F, 0.0F, 0.0F}};
    snapshot.ground_health_potions[1] = {3U, 7U, {0.0F, 0.0F, 0.0F}};
    const auto view = platform::build_material_loot_view(snapshot, 1280.0F, 720.0F);
    ARPG_REQUIRE(view.count == 2U);
    ARPG_REQUIRE(!platform::loot_label_rects_overlap(
        view.labels[0].rect, view.labels[1].rect));

    dungeon::DungeonSnapshot shared{};
    shared.ground_item_count = 1U;
    shared.ground_items[0] = {3U,
        dungeon::GroundItemSource::monster_drop, 0xFFU,
        {0.0F, 0.0F, 0.0F}, 1U, 1U, 20U,
        items::ItemSlot::weapon, items::ItemRarity::rare};
    shared.ground_material_count = 1U;
    shared.ground_materials[0] = {8U,
        dungeon::GroundMaterialSource::monster_common,
        {0.0F, 0.0F, 0.0F}, items::MaterialId::chaos};
    shared.ground_health_potion_count = 1U;
    shared.ground_health_potions[0] = {9U, 9U, {0.0F, 0.0F, 0.0F}};
    platform::LootLabelObstacleSet obstacles{};
    const auto equipment = platform::build_ground_loot_view(shared,
        arpg::settings::LootFilterMode::show_all,
        1280.0F, 720.0F, obstacles);
    const auto secondary = platform::build_material_loot_view(
        shared, 1280.0F, 720.0F, obstacles);
    ARPG_REQUIRE(equipment.count == 1U);
    ARPG_REQUIRE(secondary.count == 2U);
    ARPG_REQUIRE(obstacles.count == equipment.count + secondary.count);
    for (std::size_t index = 0U; index < secondary.count; ++index) {
        ARPG_REQUIRE(!platform::loot_label_rects_overlap(
            equipment.labels[0].rect, secondary.labels[index].rect));
    }
    platform::LootLabelObstacleSet repeated_obstacles{};
    const auto repeated_equipment = platform::build_ground_loot_view(shared,
        arpg::settings::LootFilterMode::show_all,
        1280.0F, 720.0F, repeated_obstacles);
    const auto repeated_secondary = platform::build_material_loot_view(
        shared, 1280.0F, 720.0F, repeated_obstacles);
    ARPG_REQUIRE(repeated_equipment.count == equipment.count);
    ARPG_REQUIRE(repeated_secondary.count == secondary.count);
    for (std::size_t index = 0U; index < equipment.count; ++index) {
        ARPG_REQUIRE(std::memcmp(&equipment.labels[index].rect,
            &repeated_equipment.labels[index].rect,
            sizeof(platform::LootLabelRect)) == 0);
    }
    for (std::size_t index = 0U; index < secondary.count; ++index) {
        ARPG_REQUIRE(secondary.labels[index].ordinal
            == repeated_secondary.labels[index].ordinal);
        ARPG_REQUIRE(std::memcmp(&secondary.labels[index].rect,
            &repeated_secondary.labels[index].rect,
            sizeof(platform::LootLabelRect)) == 0);
    }

    platform::LootLabelObstacleSet priority_obstacles{};
    const auto priority_equipment = platform::build_ground_loot_view(shared,
        arpg::settings::LootFilterMode::show_all,
        244.0F, 48.0F, priority_obstacles);
    const auto priority_secondary = platform::build_material_loot_view(
        shared, 244.0F, 48.0F, priority_obstacles);
    ARPG_REQUIRE(priority_equipment.count == 1U);
    ARPG_REQUIRE(priority_secondary.count == 0U);
    ARPG_REQUIRE(priority_secondary.label_drop_count == 2U);

    dungeon::DungeonSnapshot spread{};
    spread.ground_material_count = static_cast<std::uint16_t>(
        items::kMaterialCount);
    for (std::size_t index = 0U; index < items::kMaterialCount; ++index) {
        spread.ground_materials[index] = {
            static_cast<std::uint16_t>(index + 1U),
            dungeon::GroundMaterialSource::monster_common,
            {-4.5F + static_cast<float>(index % 7U) * 1.5F,
                2.0F + static_cast<float>(index / 7U) * 1.6F, 0.0F},
            static_cast<items::MaterialId>(index)};
    }
    const auto spread_view = platform::build_material_loot_view(
        spread, 1280.0F, 720.0F);
    ARPG_REQUIRE(spread_view.count == items::kMaterialCount);
    for (std::size_t label_index = 0U;
         label_index < spread_view.count; ++label_index) {
        const auto& label = spread_view.labels[label_index];
        for (std::size_t anchor_index = 0U;
             anchor_index < spread_view.count; ++anchor_index) {
            const auto& anchor = spread_view.labels[anchor_index];
            if (nearly_equal(label.anchor_x, anchor.anchor_x)
                    && nearly_equal(label.anchor_y, anchor.anchor_y)) {
                continue;
            }
            const platform::LootLabelRect icon_exclusion{
                anchor.anchor_x - 20.0F, anchor.anchor_y - 20.0F,
                40.0F, 40.0F};
            ARPG_REQUIRE(!platform::loot_label_rects_overlap(
                label.rect, icon_exclusion));
        }
    }
    const std::filesystem::path project_root =
        std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
    std::ifstream renderer_source(
        project_root / "src/platform/raylib/room_renderer.cpp");
    const std::string renderer{
        std::istreambuf_iterator<char>{renderer_source},
        std::istreambuf_iterator<char>{}};
    const std::size_t room = renderer.find("void CombatRenderer::draw_room");
    const std::size_t props = renderer.find(
        "draw_environment_room_props(", room);
    const std::size_t doors = renderer.find("draw_doors(", room);
    const std::size_t hole = renderer.find("draw_hole(", room);
    const std::size_t materials = renderer.find(
        "draw_ground_materials(", room);
    const std::size_t equipment_draw = renderer.find(
        "draw_ground_items(", room);
    ARPG_REQUIRE(renderer_source.good() || renderer_source.eof());
    ARPG_REQUIRE(room != std::string::npos);
    ARPG_REQUIRE(props < doors);
    ARPG_REQUIRE(doors < hole);
    ARPG_REQUIRE(hole < materials);
    ARPG_REQUIRE(materials < equipment_draw);
    const std::size_t draw_ground_items = renderer.find(
        "void draw_ground_items(");
    const std::size_t draw_secondary = renderer.find(
        "void draw_secondary_loot_icon", draw_ground_items);
    const std::string equipment_drawer = renderer.substr(
        draw_ground_items, draw_secondary - draw_ground_items);
    ARPG_REQUIRE(equipment_drawer.find("snapshot.ground_item_count")
        != std::string::npos);
    ARPG_REQUIRE(equipment_drawer.find("ground_loot_visible(item, mode)")
        != std::string::npos);
    ARPG_REQUIRE(equipment_drawer.find("ground_loot.labels")
        == std::string::npos);
    ARPG_REQUIRE(equipment_drawer.find("ground_loot.count")
        == std::string::npos);
    return {};
}

arpg::test::Failure single_secondary_label_preserves_original_clamped_rect() noexcept {
    struct Scenario final {
        float width;
        float height;
        arpg::combat::Vec3 position;
    };
    constexpr std::array<Scenario, 4U> kScenarios{{
        {1280.0F, 720.0F,
            {0.0F, arpg::combat::room_bounds::max_y, 0.0F}},
        {1280.0F, 720.0F,
            {0.0F, arpg::combat::room_bounds::max_y, 100.0F}},
        {3440.0F, 1440.0F,
            {0.0F, arpg::combat::room_bounds::max_y, 0.0F}},
        {3840.0F, 2160.0F,
            {0.0F, arpg::combat::room_bounds::max_y, 0.0F}},
    }};

    for (const Scenario& scenario : kScenarios) {
        dungeon::DungeonSnapshot snapshot{};
        snapshot.ground_health_potion_count = 1U;
        snapshot.ground_health_potions[0] = {
            1U, 1U, scenario.position};
        const platform::ScreenProjection projection =
            platform::project_combat_position(
                scenario.position, scenario.width, scenario.height);
        const platform::LootLabelRect expected = expected_original_label_rect(
            projection, scenario.width, scenario.height);
        const auto view = platform::build_material_loot_view(
            snapshot, scenario.width, scenario.height);

        ARPG_REQUIRE(view.count == 1U);
        ARPG_REQUIRE(view.capacity_saturation_count == 0U);
        ARPG_REQUIRE(nearly_equal(view.labels[0].anchor_x, projection.x));
        ARPG_REQUIRE(nearly_equal(view.labels[0].anchor_y, projection.y));
        const auto rect_result = require_rect_near(view.labels[0].rect, expected);
        if (rect_result.expression != nullptr) return rect_result;
    }
    return {};
}

arpg::test::Failure identical_anchors_keep_first_and_prefer_upward() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.ground_health_potion_count = 2U;
    const arpg::combat::Vec3 bottom_position{
        0.0F, arpg::combat::room_bounds::max_y, 0.0F};
    snapshot.ground_health_potions[0] = {1U, 1U, bottom_position};
    snapshot.ground_health_potions[1] = {2U, 2U, bottom_position};
    auto view = platform::build_material_loot_view(snapshot, 1280.0F, 720.0F);
    ARPG_REQUIRE(view.count == 2U);
    const auto projection = platform::project_combat_position(
        bottom_position, 1280.0F, 720.0F);
    const auto original = expected_original_label_rect(
        projection, 1280.0F, 720.0F);
    auto rect_result = require_rect_near(view.labels[0].rect, original);
    if (rect_result.expression != nullptr) return rect_result;
    ARPG_REQUIRE(nearly_equal(view.labels[1].rect.x, original.x));
    ARPG_REQUIRE(view.labels[1].rect.y <= original.y
        - kExpectedLabelHeight - kExpectedPlacementGap
        + kPlacementEpsilon);
    ARPG_REQUIRE(!platform::loot_label_rects_overlap(
        view.labels[0].rect, view.labels[1].rect));

    const arpg::combat::Vec3 top_position{
        0.0F, arpg::combat::room_bounds::max_y, 100.0F};
    snapshot.ground_health_potions[0].position = top_position;
    snapshot.ground_health_potions[1].position = top_position;
    view = platform::build_material_loot_view(snapshot, 1280.0F, 720.0F);
    ARPG_REQUIRE(view.count == 2U);
    const auto top_projection = platform::project_combat_position(
        top_position, 1280.0F, 720.0F);
    const auto top_original = expected_original_label_rect(
        top_projection, 1280.0F, 720.0F);
    rect_result = require_rect_near(view.labels[0].rect, top_original);
    if (rect_result.expression != nullptr) return rect_result;
    ARPG_REQUIRE(view.labels[1].rect.y + kPlacementEpsilon
        >= top_original.y);
    ARPG_REQUIRE(!platform::loot_label_rects_overlap(
        view.labels[0].rect, view.labels[1].rect));
    return {};
}

arpg::test::Failure large_viewport_candidates_reach_bottom_near_anchor() noexcept {
    struct Viewport final {
        float width;
        float height;
    };
    constexpr std::array<Viewport, 2U> kViewports{{
        {3440.0F, 1440.0F},
        {3840.0F, 2160.0F},
    }};
    constexpr float kMaximumLocalVerticalOffset =
        (kExpectedLabelHeight + kExpectedPlacementGap) * 5.0F;

    for (const Viewport viewport : kViewports) {
        dungeon::DungeonSnapshot snapshot{};
        snapshot.ground_health_potion_count = 3U;
        const arpg::combat::Vec3 position{
            0.0F, arpg::combat::room_bounds::max_y, 0.0F};
        for (std::size_t index = 0U; index < 3U; ++index) {
            snapshot.ground_health_potions[index] = {
                static_cast<std::uint16_t>(index),
                static_cast<std::uint16_t>(index), position};
        }
        platform::MaterialLootPlacementDiagnostics diagnostics{};
        const auto view = platform::build_material_loot_view_with_diagnostics(
            snapshot, viewport.width, viewport.height, diagnostics);
        ARPG_REQUIRE(view.count == 3U);
        const auto projection = platform::project_combat_position(
            position, viewport.width, viewport.height);
        const auto original = expected_original_label_rect(
            projection, viewport.width, viewport.height);
        const auto rect_result = require_rect_near(view.labels[0].rect, original);
        if (rect_result.expression != nullptr) return rect_result;
        ARPG_REQUIRE(std::fabs(view.labels[2].rect.y - original.y)
            <= kMaximumLocalVerticalOffset);
        for (std::size_t left = 0U; left < view.count; ++left) {
            for (std::size_t right = left + 1U; right < view.count; ++right) {
                ARPG_REQUIRE(!platform::loot_label_rects_overlap(
                    view.labels[left].rect, view.labels[right].rect));
            }
        }
    }
    return {};
}

arpg::test::Failure top_clamped_secondary_labels_resolve_without_overlap() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.ground_health_potion_count = 2U;
    snapshot.ground_health_potions[0] = {2U, 5U, {0.0F, 0.0F, 0.0F}};
    snapshot.ground_health_potions[1] = {3U, 7U, {0.0F, 0.0F, 0.0F}};
    const auto view = platform::build_material_loot_view(snapshot, 1280.0F, 40.0F);
    ARPG_REQUIRE(view.count == 2U);
    ARPG_REQUIRE(!platform::loot_label_rects_overlap(
        view.labels[0].rect, view.labels[1].rect));
    return {};
}

arpg::test::Failure material_loot_view_stays_within_fixed_stack_budget() noexcept {
    constexpr std::size_t kMaximumMaterialLootViewBytes = 40U * 1024U;
    std::printf(
        "[material-loot-size] label=%zu view=%zu plan=%zu budget=%zu\n",
        sizeof(platform::MaterialLootLabel),
        sizeof(platform::MaterialLootView),
        sizeof(platform::CombatRenderPlan),
        kMaximumMaterialLootViewBytes);
    ARPG_REQUIRE(fits_within_bytes(
        sizeof(platform::MaterialLootView), kMaximumMaterialLootViewBytes));
    return {};
}

arpg::test::Failure health_potion_feedback_only_observes_new_committed_receipts() noexcept {
    platform::MaterialPickupFeedbackState feedback{};
    dungeon::DungeonSnapshot snapshot{};
    snapshot.health_potion_pickup_receipt = {true, false, 1U, 10U, 250};
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);
    snapshot.health_potion_pickup_receipt.commit_generation = 11U;
    snapshot.health_potion_pickup_receipt.restored_hp = 251;
    const auto committed = feedback.observe(snapshot);
    ARPG_REQUIRE(committed.ready);
    ARPG_REQUIRE(committed.emphasized);
    ARPG_REQUIRE(std::strcmp(
        committed.text.bytes.data(), "生命药恢复 +251 HP") == 0);
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);
    return {};
}

arpg::test::Failure simultaneous_potion_and_material_feedback_preserves_both() noexcept {
    platform::MaterialPickupFeedbackState feedback{};
    dungeon::DungeonSnapshot snapshot{};
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);
    snapshot.material_pickup_receipt.valid = true;
    snapshot.material_pickup_receipt.commit_generation = 10U;
    snapshot.material_pickup_receipt.counts[
        items::material_index(items::MaterialId::chaos)] = 1U;
    snapshot.health_potion_pickup_receipt = {true, false, 1U, 10U, 250};

    const auto potion = feedback.observe(snapshot);
    ARPG_REQUIRE(potion.ready);
    ARPG_REQUIRE(std::strcmp(
        potion.text.bytes.data(), "生命药恢复 +250 HP") == 0);
    const auto material = feedback.observe(snapshot);
    ARPG_REQUIRE(material.ready);
    ARPG_REQUIRE(std::strcmp(material.text.bytes.data(),
        "已拾取：混沌石 x1") == 0);
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);
    return {};
}

void fill_secondary_loot_pressure_snapshot(
    dungeon::DungeonSnapshot& snapshot, float z) noexcept {
    snapshot.ground_material_count = static_cast<std::uint16_t>(
        dungeon::kGroundMaterialCapacity);
    for (std::size_t index = 0U;
         index < snapshot.ground_materials.size(); ++index) {
        snapshot.ground_materials[index] = {
            static_cast<std::uint16_t>(index),
            dungeon::GroundMaterialSource::monster_common,
            {0.0F, 0.0F, z}, items::MaterialId::chaos};
    }
    snapshot.ground_materials.back().material = items::MaterialId::count;

    snapshot.ground_health_potion_count = static_cast<std::uint16_t>(
        dungeon::kGroundHealthPotionCapacity);
    for (std::size_t index = 0U;
         index < snapshot.ground_health_potions.size(); ++index) {
        snapshot.ground_health_potions[index] = {
            static_cast<std::uint16_t>(index),
            static_cast<std::uint16_t>(
                dungeon::kGroundMaterialCapacity + index),
            {0.0F, 0.0F, z}};
    }
}

arpg::test::Failure verify_secondary_loot_pressure_view(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    constexpr std::size_t kSourceCount = dungeon::kGroundMaterialCapacity
        + dungeon::kGroundHealthPotionCapacity;
    constexpr std::uint64_t kOperationUpperBound =
        static_cast<std::uint64_t>(kSourceCount) * kSourceCount;
    platform::MaterialLootPlacementDiagnostics diagnostics{};
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    const auto view = platform::build_material_loot_view_with_diagnostics(
        snapshot, 1280.0F, 720.0F, diagnostics);
    const std::uint64_t allocations_after = arpg::test::allocation_count();

    ARPG_REQUIRE(view.count > 0U);
    ARPG_REQUIRE(view.count < kSourceCount);
    ARPG_REQUIRE(view.invalid_material_count == 1U);
    ARPG_REQUIRE(view.label_drop_count > 0U);
    ARPG_REQUIRE(view.capacity_saturation_count == 0U);
    ARPG_REQUIRE(view.count + view.label_drop_count
        == kSourceCount - view.invalid_material_count);
    ARPG_REQUIRE(allocations_after == allocations_before);
    ARPG_REQUIRE(diagnostics.direct_collision_check_count > 0U);
    ARPG_REQUIRE(diagnostics.direct_collision_check_count
        <= kOperationUpperBound);
    ARPG_REQUIRE(diagnostics.candidate_probe_count > 0U);
    ARPG_REQUIRE(diagnostics.candidate_probe_count <= kOperationUpperBound);
    ARPG_REQUIRE(diagnostics.occupancy_mark_check_count > 0U);
    ARPG_REQUIRE(diagnostics.occupancy_mark_check_count
        <= kOperationUpperBound);
    std::printf(
        "[material-loot-pressure] z=%.0f retained=%zu saturated=%u "
        "direct=%llu candidates=%llu occupancy=%llu\n",
        snapshot.ground_health_potions[0].position.z,
        view.count, view.capacity_saturation_count,
        static_cast<unsigned long long>(
            diagnostics.direct_collision_check_count),
        static_cast<unsigned long long>(diagnostics.candidate_probe_count),
        static_cast<unsigned long long>(
            diagnostics.occupancy_mark_check_count));
    for (std::size_t index = 1U; index < view.count; ++index) {
        ARPG_REQUIRE(view.labels[index - 1U].ordinal
            < view.labels[index].ordinal);
    }
    for (std::size_t left = 0U; left < view.count; ++left) {
        for (std::size_t right = left + 1U; right < view.count; ++right) {
            ARPG_REQUIRE(!platform::loot_label_rects_overlap(
                view.labels[left].rect, view.labels[right].rect));
        }
    }
    return {};
}

arpg::test::Failure saturated_same_position_and_top_labels_have_bounded_work() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    fill_secondary_loot_pressure_snapshot(snapshot, 100.0F);
    const auto top = verify_secondary_loot_pressure_view(snapshot);
    if (top.expression != nullptr) return top;

    fill_secondary_loot_pressure_snapshot(snapshot, 0.0F);
    return verify_secondary_loot_pressure_view(snapshot);
}

arpg::test::Failure continuous_feedback_receipts_are_consumed_before_publish() noexcept {
    platform::MaterialPickupFeedbackState feedback{};
    dungeon::DungeonSnapshot snapshot{};
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);

    snapshot.material_pickup_receipt.valid = true;
    snapshot.material_pickup_receipt.commit_generation = 10U;
    snapshot.material_pickup_receipt.counts[
        items::material_index(items::MaterialId::chaos)] = 1U;
    snapshot.health_potion_pickup_receipt = {true, false, 1U, 10U, 250};
    auto published = feedback.observe(snapshot);
    ARPG_REQUIRE(published.ready);
    ARPG_REQUIRE(std::strcmp(
        published.text.bytes.data(), "生命药恢复 +250 HP") == 0);

    snapshot.health_potion_pickup_receipt.commit_generation = 11U;
    snapshot.health_potion_pickup_receipt.restored_hp = 251;
    published = feedback.observe(snapshot);
    ARPG_REQUIRE(published.ready);
    ARPG_REQUIRE(std::strcmp(
        published.text.bytes.data(), "生命药恢复 +251 HP") == 0);

    snapshot.health_potion_pickup_receipt.commit_generation = 12U;
    snapshot.health_potion_pickup_receipt.restored_hp = 252;
    published = feedback.observe(snapshot);
    ARPG_REQUIRE(published.ready);
    ARPG_REQUIRE(std::strcmp(
        published.text.bytes.data(), "生命药恢复 +252 HP") == 0);

    published = feedback.observe(snapshot);
    ARPG_REQUIRE(published.ready);
    ARPG_REQUIRE(std::strcmp(
        published.text.bytes.data(), "已拾取：混沌石 x1") == 0);
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);

    snapshot.material_pickup_receipt.valid = false;
    snapshot.material_pickup_receipt.commit_generation = 11U;
    snapshot.material_pickup_receipt.counts[
        items::material_index(items::MaterialId::chaos)] = 2U;
    snapshot.health_potion_pickup_receipt.valid = false;
    snapshot.health_potion_pickup_receipt.commit_generation = 13U;
    snapshot.health_potion_pickup_receipt.restored_hp = 253;
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);

    snapshot.material_pickup_receipt.valid = true;
    snapshot.health_potion_pickup_receipt.valid = true;
    snapshot.health_potion_pickup_receipt.restored_hp = 0;
    published = feedback.observe(snapshot);
    ARPG_REQUIRE(published.ready);
    ARPG_REQUIRE(std::strcmp(
        published.text.bytes.data(), "已拾取：混沌石 x3") == 0);
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);

    snapshot.material_pickup_receipt.commit_generation = 12U;
    snapshot.material_pickup_receipt.counts.fill(0U);
    snapshot.health_potion_pickup_receipt.restored_hp = 253;
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);
    snapshot.material_pickup_receipt.counts[
        items::material_index(items::MaterialId::chaos)] = 9U;
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);
    snapshot.health_potion_pickup_receipt.commit_generation = 14U;
    snapshot.health_potion_pickup_receipt.restored_hp = 254;
    published = feedback.observe(snapshot);
    ARPG_REQUIRE(published.ready);
    ARPG_REQUIRE(std::strcmp(
        published.text.bytes.data(), "生命药恢复 +254 HP") == 0);
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);

    platform::MaterialPickupFeedbackState sustained{};
    dungeon::DungeonSnapshot sustained_snapshot{};
    ARPG_REQUIRE(!sustained.observe(sustained_snapshot).ready);
    constexpr std::size_t kReceiptCount = 64U;
    std::array<bool, kReceiptCount> potion_seen{};
    std::size_t potion_seen_count{};
    bool saw_final_material_total{};
    const auto record = [&](const platform::MaterialPickupFeedback& value) {
        if (!value.ready) return true;
        if (std::strncmp(value.text.bytes.data(), "生命药恢复 +",
                sizeof("生命药恢复 +") - 1U) == 0) {
            bool matched{};
            for (std::size_t index = 0U; index < kReceiptCount; ++index) {
                std::array<char, 96U> expected{};
                static_cast<void>(std::snprintf(expected.data(), expected.size(),
                    "生命药恢复 +%zu HP", 1001U + index));
                if (std::strcmp(value.text.bytes.data(), expected.data()) != 0) {
                    continue;
                }
                if (potion_seen[index]) return false;
                potion_seen[index] = true;
                ++potion_seen_count;
                matched = true;
                break;
            }
            if (!matched) return false;
        }
        saw_final_material_total = saw_final_material_total
            || std::strcmp(value.text.bytes.data(),
                "已拾取：混沌石 x64") == 0;
        return true;
    };
    for (std::size_t index = 0U; index < kReceiptCount; ++index) {
        sustained_snapshot.material_pickup_receipt.valid = true;
        sustained_snapshot.material_pickup_receipt.commit_generation = index + 1U;
        sustained_snapshot.material_pickup_receipt.counts[
            items::material_index(items::MaterialId::chaos)] = 1U;
        sustained_snapshot.health_potion_pickup_receipt = {true, false, 1U,
            index + 1U, static_cast<int>(1001U + index)};
        ARPG_REQUIRE(record(sustained.observe(sustained_snapshot)));
    }
    for (std::size_t drain = 0U; drain < kReceiptCount + 2U; ++drain) {
        ARPG_REQUIRE(record(sustained.observe(sustained_snapshot)));
    }
    ARPG_REQUIRE(potion_seen_count == kReceiptCount);
    ARPG_REQUIRE(saw_final_material_total);
    ARPG_REQUIRE(!sustained.observe(sustained_snapshot).ready);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"Chinese labels and emphasis", &labels_and_emphasis_are_player_facing},
    {"material view ignores equipment filter", &material_view_ignores_equipment_filter_and_orders_ordinals},
    {"pickup feedback aggregates material counts", &pickup_feedback_aggregates_counts_by_material},
    {"every material uses a unique resource", &every_material_has_a_unique_authored_resource},
    {"health potion uses dedicated sprite and pure-red label", &health_potion_uses_dedicated_sprite_and_pure_red_label},
    {"overlapping secondary-loot labels are resolved", &overlapping_secondary_loot_labels_are_resolved},
    {"single secondary label preserves original clamped rect", &single_secondary_label_preserves_original_clamped_rect},
    {"identical anchors keep first and prefer upward", &identical_anchors_keep_first_and_prefer_upward},
    {"large viewport candidates reach bottom near anchor", &large_viewport_candidates_reach_bottom_near_anchor},
    {"top-clamped secondary labels resolve without overlap", &top_clamped_secondary_labels_resolve_without_overlap},
    {"material loot view stays within fixed stack budget", &material_loot_view_stays_within_fixed_stack_budget},
    {"health potion feedback observes new committed receipts", &health_potion_feedback_only_observes_new_committed_receipts},
    {"simultaneous potion and material feedback preserves both", &simultaneous_potion_and_material_feedback_preserves_both},
    {"saturated same-position and top labels have bounded work", &saturated_same_position_and_top_labels_have_bounded_work},
    {"continuous feedback receipts are consumed before publish", &continuous_feedback_receipts_are_consumed_before_publish},
};

}  // namespace

arpg::test::TestSuite material_loot_view_suite() noexcept {
    return arpg::test::make_suite("material_loot_view", kCases);
}
