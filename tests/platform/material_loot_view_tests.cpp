#include "test_framework.hpp"

#include "material_loot_view.hpp"
#include "material_manifest.hpp"

#include <cstring>

namespace {

namespace dungeon = arpg::dungeon;
namespace items = arpg::items;
namespace platform = arpg::platform;

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
    ARPG_REQUIRE(std::strcmp(view.labels[0].text.data(), "+12强化券") == 0);
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
    ARPG_REQUIRE(std::strcmp(view.labels[0].text.data(), "生命药") == 0);
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
        committed.text.bytes.data(), "生命药 +251 HP") == 0);
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
    ARPG_REQUIRE(std::strcmp(potion.text.bytes.data(), "生命药 +250 HP") == 0);
    const auto material = feedback.observe(snapshot);
    ARPG_REQUIRE(material.ready);
    ARPG_REQUIRE(std::strcmp(material.text.bytes.data(),
        "已拾取：混沌石 x1") == 0);
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"Chinese labels and emphasis", &labels_and_emphasis_are_player_facing},
    {"material view ignores equipment filter", &material_view_ignores_equipment_filter_and_orders_ordinals},
    {"pickup feedback aggregates material counts", &pickup_feedback_aggregates_counts_by_material},
    {"every material uses a unique resource", &every_material_has_a_unique_authored_resource},
    {"health potion uses dedicated sprite and pure-red label", &health_potion_uses_dedicated_sprite_and_pure_red_label},
    {"overlapping secondary-loot labels are resolved", &overlapping_secondary_loot_labels_are_resolved},
    {"top-clamped secondary labels resolve without overlap", &top_clamped_secondary_labels_resolve_without_overlap},
    {"health potion feedback observes new committed receipts", &health_potion_feedback_only_observes_new_committed_receipts},
    {"simultaneous potion and material feedback preserves both", &simultaneous_potion_and_material_feedback_preserves_both},
};

}  // namespace

arpg::test::TestSuite material_loot_view_suite() noexcept {
    return arpg::test::make_suite("material_loot_view", kCases);
}
