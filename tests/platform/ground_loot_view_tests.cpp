#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "ground_loot_view.hpp"
#include "material_manifest.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

namespace dungeon = arpg::dungeon;
namespace items = arpg::items;
namespace platform = arpg::platform;
namespace settings = arpg::settings;

[[nodiscard]] dungeon::GroundItemSnapshot ground_item(
    std::uint16_t ordinal,
    items::ItemRarity rarity,
    std::uint8_t base_id = 1U,
    dungeon::GroundItemSource source =
        dungeon::GroundItemSource::monster_drop,
    arpg::combat::Vec3 position = {}) noexcept {
    dungeon::GroundItemSnapshot item{};
    item.ordinal = ordinal;
    item.source = source;
    item.position = position;
    item.item_id = 1000U + ordinal;
    item.base_id = base_id;
    item.item_level = static_cast<std::uint8_t>(20U + ordinal % 80U);
    item.rarity = rarity;
    return item;
}

[[nodiscard]] dungeon::DungeonSnapshot& scratch_snapshot() noexcept {
    static dungeon::DungeonSnapshot snapshot{};
    snapshot = {};
    return snapshot;
}

void append(dungeon::DungeonSnapshot& snapshot,
    dungeon::GroundItemSnapshot item) noexcept {
    if (snapshot.ground_item_count < snapshot.ground_items.size()) {
        snapshot.ground_items[snapshot.ground_item_count++] = item;
    }
}

[[nodiscard]] bool same_color(platform::Rgba8 lhs,
    platform::Rgba8 rhs) noexcept {
    return lhs.r == rhs.r && lhs.g == rhs.g
        && lhs.b == rhs.b && lhs.a == rhs.a;
}

[[nodiscard]] bool overlaps(platform::LootLabelRect lhs,
    platform::LootLabelRect rhs) noexcept {
    return lhs.x < rhs.x + rhs.width && rhs.x < lhs.x + lhs.width
        && lhs.y < rhs.y + rhs.height && rhs.y < lhs.y + lhs.height;
}

arpg::test::Failure visibility_covers_three_modes_and_abyss_bypass() noexcept {
    const auto normal = ground_item(1U, items::ItemRarity::normal);
    const auto magic = ground_item(2U, items::ItemRarity::magic);
    const auto rare = ground_item(3U, items::ItemRarity::rare);
    const auto abyss = ground_item(4U, items::ItemRarity::normal, 1U,
        dungeon::GroundItemSource::abyss_chest);

    ARPG_REQUIRE(platform::ground_loot_visible(
        normal, settings::LootFilterMode::show_all));
    ARPG_REQUIRE(platform::ground_loot_visible(
        magic, settings::LootFilterMode::show_all));
    ARPG_REQUIRE(platform::ground_loot_visible(
        rare, settings::LootFilterMode::show_all));
    ARPG_REQUIRE(!platform::ground_loot_visible(
        normal, settings::LootFilterMode::magic_or_better));
    ARPG_REQUIRE(platform::ground_loot_visible(
        magic, settings::LootFilterMode::magic_or_better));
    ARPG_REQUIRE(platform::ground_loot_visible(
        rare, settings::LootFilterMode::magic_or_better));
    ARPG_REQUIRE(!platform::ground_loot_visible(
        normal, settings::LootFilterMode::rare_only));
    ARPG_REQUIRE(!platform::ground_loot_visible(
        magic, settings::LootFilterMode::rare_only));
    ARPG_REQUIRE(platform::ground_loot_visible(
        rare, settings::LootFilterMode::rare_only));
    ARPG_REQUIRE(platform::ground_loot_visible(
        abyss, settings::LootFilterMode::rare_only));
    ARPG_REQUIRE(platform::ground_loot_visible(normal,
        static_cast<settings::LootFilterMode>(0xFFU)));
    return {};
}

arpg::test::Failure builder_filters_and_orders_by_stable_ordinal() noexcept {
    auto& snapshot = scratch_snapshot();
    append(snapshot, ground_item(30U, items::ItemRarity::rare));
    append(snapshot, ground_item(10U, items::ItemRarity::normal));
    append(snapshot, ground_item(20U, items::ItemRarity::magic));

    const auto all = platform::build_ground_loot_view(snapshot,
        settings::LootFilterMode::show_all, 1280.0F, 720.0F);
    ARPG_REQUIRE(all.count == 3U);
    ARPG_REQUIRE(all.labels[0].ordinal == 10U);
    ARPG_REQUIRE(all.labels[1].ordinal == 20U);
    ARPG_REQUIRE(all.labels[2].ordinal == 30U);

    const auto magic = platform::build_ground_loot_view(snapshot,
        settings::LootFilterMode::magic_or_better, 1280.0F, 720.0F);
    ARPG_REQUIRE(magic.count == 2U);
    ARPG_REQUIRE(magic.labels[0].ordinal == 20U);
    ARPG_REQUIRE(magic.labels[1].ordinal == 30U);

    const auto rare = platform::build_ground_loot_view(snapshot,
        settings::LootFilterMode::rare_only, 1280.0F, 720.0F);
    ARPG_REQUIRE(rare.count == 1U);
    ARPG_REQUIRE(rare.labels[0].ordinal == 30U);
    return {};
}

arpg::test::Failure text_uses_catalog_chinese_rarity_ilvl_and_fallback() noexcept {
    auto& snapshot = scratch_snapshot();
    auto valid = ground_item(2U, items::ItemRarity::magic, 4U);
    valid.item_level = 37U;
    append(snapshot, valid);
    auto invalid = ground_item(5U, items::ItemRarity::rare, 0xFFU);
    invalid.item_level = 99U;
    append(snapshot, invalid);

    const auto view = platform::build_ground_loot_view(snapshot,
        settings::LootFilterMode::show_all, 1280.0F, 720.0F);
    ARPG_REQUIRE(view.count == 2U);
    ARPG_REQUIRE(std::strcmp(
        view.labels[0].text.data(), "魔法 Striker Gloves · i37") == 0);
    ARPG_REQUIRE(std::strcmp(
        view.labels[1].text.data(), "稀有 未知装备 · i99") == 0);
    ARPG_REQUIRE(view.labels[0].text.back() == '\0');
    ARPG_REQUIRE(view.labels[1].text.back() == '\0');
    ARPG_REQUIRE(view.diagnostics.invalid_base_count == 1U);
    ARPG_REQUIRE(view.diagnostics.text_truncation_count == 0U);
    return {};
}

arpg::test::Failure rarity_palette_and_abyss_marker_are_stable() noexcept {
    auto& snapshot = scratch_snapshot();
    append(snapshot, ground_item(1U, items::ItemRarity::normal));
    append(snapshot, ground_item(2U, items::ItemRarity::magic));
    append(snapshot, ground_item(3U, items::ItemRarity::rare));
    append(snapshot, ground_item(4U, items::ItemRarity::normal, 1U,
        dungeon::GroundItemSource::abyss_chest));

    const auto view = platform::build_ground_loot_view(snapshot,
        settings::LootFilterMode::show_all, 1280.0F, 720.0F);
    ARPG_REQUIRE(view.count == 4U);
    ARPG_REQUIRE(same_color(view.labels[0].text_color,
        platform::Rgba8{232U, 232U, 232U, 255U}));
    ARPG_REQUIRE(same_color(view.labels[1].text_color,
        platform::Rgba8{96U, 170U, 255U, 255U}));
    ARPG_REQUIRE(same_color(view.labels[2].text_color,
        platform::Rgba8{255U, 205U, 70U, 255U}));
    ARPG_REQUIRE(!view.labels[0].abyss);
    ARPG_REQUIRE(view.labels[3].abyss);
    ARPG_REQUIRE(same_color(view.labels[3].border_color,
        platform::Rgba8{184U, 96U, 255U, 255U}));
    return {};
}

arpg::test::Failure overlapping_anchors_resolve_upward_deterministically() noexcept {
    auto& snapshot = scratch_snapshot();
    append(snapshot, ground_item(22U, items::ItemRarity::rare));
    append(snapshot, ground_item(11U, items::ItemRarity::magic));

    const auto first = platform::build_ground_loot_view(snapshot,
        settings::LootFilterMode::show_all, 1280.0F, 720.0F);
    const auto second = platform::build_ground_loot_view(snapshot,
        settings::LootFilterMode::show_all, 1280.0F, 720.0F);
    ARPG_REQUIRE(first.count == 2U);
    ARPG_REQUIRE(!overlaps(first.labels[0].rect, first.labels[1].rect));
    ARPG_REQUIRE(first.labels[1].rect.y < first.labels[0].rect.y);
    ARPG_REQUIRE(first.labels[0].ordinal == 11U);
    ARPG_REQUIRE(first.labels[1].ordinal == 22U);
    ARPG_REQUIRE(first.labels[0].rect.x == second.labels[0].rect.x);
    ARPG_REQUIRE(first.labels[0].rect.y == second.labels[0].rect.y);
    ARPG_REQUIRE(first.labels[1].rect.x == second.labels[1].rect.x);
    ARPG_REQUIRE(first.labels[1].rect.y == second.labels[1].rect.y);
    ARPG_REQUIRE(first.diagnostics.overlap_adjustment_count > 0U);

    platform::LootLabelObstacleSet seeded{};
    const platform::LootLabelRect actor{500.0F, 250.0F, 280.0F, 220.0F};
    ARPG_REQUIRE(seeded.append(actor));
    const auto actor_safe = platform::build_ground_loot_view(snapshot,
        settings::LootFilterMode::show_all, 1280.0F, 720.0F, seeded);
    ARPG_REQUIRE(actor_safe.count == 2U);
    ARPG_REQUIRE(seeded.count == actor_safe.count + 1U);
    for (std::size_t index = 0U; index < actor_safe.count; ++index) {
        ARPG_REQUIRE(!overlaps(actor_safe.labels[index].rect, actor));
    }
    platform::LootLabelObstacleSet repeated_seed{};
    ARPG_REQUIRE(repeated_seed.append(actor));
    const auto repeated = platform::build_ground_loot_view(snapshot,
        settings::LootFilterMode::show_all, 1280.0F, 720.0F,
        repeated_seed);
    ARPG_REQUIRE(repeated.count == actor_safe.count);
    for (std::size_t index = 0U; index < repeated.count; ++index) {
        ARPG_REQUIRE(std::memcmp(&repeated.labels[index].rect,
            &actor_safe.labels[index].rect,
            sizeof(platform::LootLabelRect)) == 0);
    }
    return {};
}

arpg::test::Failure supported_resolutions_keep_all_rects_in_safety_area() noexcept {
    constexpr std::array<std::array<float, 2>, 3> kViewports{{
        {{1024.0F, 576.0F}}, {{1280.0F, 720.0F}}, {{1920.0F, 1080.0F}},
    }};
    auto& snapshot = scratch_snapshot();
    append(snapshot, ground_item(1U, items::ItemRarity::normal, 1U,
        dungeon::GroundItemSource::monster_drop, {-1000.0F, -1000.0F, 1000.0F}));
    append(snapshot, ground_item(2U, items::ItemRarity::magic, 2U,
        dungeon::GroundItemSource::monster_drop, {1000.0F, 1000.0F, -1000.0F}));

    for (const auto viewport : kViewports) {
        const auto view = platform::build_ground_loot_view(snapshot,
            settings::LootFilterMode::show_all, viewport[0], viewport[1]);
        ARPG_REQUIRE(view.count == 2U);
        for (std::size_t index = 0U; index < view.count; ++index) {
            const auto rect = view.labels[index].rect;
            ARPG_REQUIRE(rect.x >= platform::kGroundLootSafetyInset);
            ARPG_REQUIRE(rect.y >= platform::kGroundLootSafetyInset);
            ARPG_REQUIRE(rect.x + rect.width
                <= viewport[0] - platform::kGroundLootSafetyInset);
            ARPG_REQUIRE(rect.y + rect.height
                <= viewport[1] - platform::kGroundLootSafetyInset);
        }
    }
    return {};
}

arpg::test::Failure full_capacity_extreme_layout_is_bounded() noexcept {
    static_assert(platform::kLootLabelObstacleCapacity == 881U);
    platform::LootLabelObstacleSet boundary{};
    for (std::size_t index = 0U;
         index < platform::kLootLabelObstacleCapacity; ++index) {
        ARPG_REQUIRE(boundary.append({static_cast<float>(index), 0.0F,
            0.0F, 0.0F}));
    }
    ARPG_REQUIRE(boundary.count == platform::kLootLabelObstacleCapacity);
    ARPG_REQUIRE(!boundary.append({}));

    auto& snapshot = scratch_snapshot();
    snapshot.ground_item_count = static_cast<std::uint16_t>(
        dungeon::kGroundDropCapacity + 7U);
    for (std::size_t index = 0U;
         index < dungeon::kGroundDropCapacity; ++index) {
        snapshot.ground_items[index] = ground_item(
            static_cast<std::uint16_t>(dungeon::kGroundDropCapacity - index),
            items::ItemRarity::rare);
    }

    const auto view = platform::build_ground_loot_view(snapshot,
        settings::LootFilterMode::show_all, 1024.0F, 576.0F);
    ARPG_REQUIRE(view.count > 0U);
    ARPG_REQUIRE(view.count < dungeon::kGroundDropCapacity);
    ARPG_REQUIRE(view.count + view.diagnostics.label_drop_count
        == dungeon::kGroundDropCapacity);
    ARPG_REQUIRE(view.labels.size() == dungeon::kGroundDropCapacity);
    ARPG_REQUIRE(view.diagnostics.capacity_saturation_count == 7U);
    ARPG_REQUIRE(view.diagnostics.overlap_adjustment_count
        <= dungeon::kGroundDropCapacity * dungeon::kGroundDropCapacity);
    for (std::size_t index = 0U; index < view.count; ++index) {
        const auto rect = view.labels[index].rect;
        if (index > 0U) {
            ARPG_REQUIRE(view.labels[index - 1U].ordinal
                < view.labels[index].ordinal);
        }
        ARPG_REQUIRE(view.labels[index].text.back() == '\0');
        ARPG_REQUIRE(rect.x >= platform::kGroundLootSafetyInset);
        ARPG_REQUIRE(rect.y >= platform::kGroundLootSafetyInset);
        ARPG_REQUIRE(rect.x + rect.width
            <= 1024.0F - platform::kGroundLootSafetyInset);
        ARPG_REQUIRE(rect.y + rect.height
            <= 576.0F - platform::kGroundLootSafetyInset);
        for (std::size_t other = index + 1U; other < view.count; ++other) {
            ARPG_REQUIRE(!overlaps(rect, view.labels[other].rect));
        }
    }

    platform::LootLabelObstacleSet blocked{};
    ARPG_REQUIRE(blocked.append({0.0F, 0.0F, 1024.0F, 576.0F}));
    const auto dropped = platform::build_ground_loot_view(snapshot,
        settings::LootFilterMode::show_all, 1024.0F, 576.0F, blocked);
    ARPG_REQUIRE(dropped.count == 0U);
    ARPG_REQUIRE(dropped.diagnostics.label_drop_count
        == dungeon::kGroundDropCapacity);
    ARPG_REQUIRE(dropped.count + dropped.diagnostics.label_drop_count
        == dungeon::kGroundDropCapacity);
    ARPG_REQUIRE(dropped.diagnostics.capacity_saturation_count == 7U);
    return {};
}

arpg::test::Failure builder_does_not_mutate_snapshot_bytes() noexcept {
    auto& snapshot = scratch_snapshot();
    append(snapshot, ground_item(7U, items::ItemRarity::rare, 6U,
        dungeon::GroundItemSource::abyss_chest, {2.0F, 3.0F, 1.0F}));
    static dungeon::DungeonSnapshot before{};
    before = snapshot;

    static_cast<void>(platform::build_ground_loot_view(snapshot,
        settings::LootFilterMode::rare_only, 1280.0F, 720.0F));
    ARPG_REQUIRE(std::memcmp(&snapshot, &before, sizeof(snapshot)) == 0);
    return {};
}

arpg::test::Failure hundred_thousand_builds_allocate_nothing() noexcept {
    auto& snapshot = scratch_snapshot();
    append(snapshot, ground_item(3U, items::ItemRarity::normal));
    append(snapshot, ground_item(1U, items::ItemRarity::magic));
    append(snapshot, ground_item(2U, items::ItemRarity::rare, 2U,
        dungeon::GroundItemSource::abyss_chest));

    std::uint64_t checksum{};
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t iteration = 0U; iteration < 100000U; ++iteration) {
        const auto mode = iteration < 50000U
            ? settings::LootFilterMode::show_all
            : (iteration & 1U) == 0U
                ? settings::LootFilterMode::magic_or_better
                : settings::LootFilterMode::rare_only;
        const auto view = platform::build_ground_loot_view(
            snapshot, mode, 1280.0F, 720.0F);
        checksum += view.count;
        checksum += view.labels[0].ordinal;
        ARPG_REQUIRE(view.labels[0].text.back() == '\0');
    }
    const std::uint64_t after = arpg::test::allocation_count();
    ARPG_REQUIRE(after == before);
    ARPG_REQUIRE(checksum != 0U);
    std::printf("[stage11d-ground-loot] builds=100000 "
        "unchanged=50000 alternating=50000 allocations=%llu\n",
        static_cast<unsigned long long>(after - before));
    return {};
}

arpg::test::Failure equipment_slots_and_rarities_have_unique_layered_resources() noexcept {
    constexpr std::array<items::ItemSlot, 6U> kSlots{{
        items::ItemSlot::weapon, items::ItemSlot::helmet,
        items::ItemSlot::chest, items::ItemSlot::gloves,
        items::ItemSlot::boots, items::ItemSlot::accessory,
    }};
    std::array<platform::MaterialSpriteId, kSlots.size()> slot_sprites{};
    for (std::size_t index{}; index < kSlots.size(); ++index) {
        slot_sprites[index] = platform::ground_loot_item_sprite(kSlots[index]);
        ARPG_REQUIRE(slot_sprites[index] != platform::MaterialSpriteId::missing);
        for (std::size_t previous{}; previous < index; ++previous) {
            ARPG_REQUIRE(slot_sprites[index] != slot_sprites[previous]);
        }
    }

    constexpr std::array<items::ItemRarity, 3U> kRarities{{
        items::ItemRarity::normal, items::ItemRarity::magic,
        items::ItemRarity::rare,
    }};
    std::array<platform::MaterialSpriteId, 4U> rarity_sprites{};
    for (std::size_t index{}; index < kRarities.size(); ++index) {
        rarity_sprites[index] = platform::ground_loot_rarity_sprite(
            kRarities[index], false);
    }
    rarity_sprites[3] = platform::ground_loot_rarity_sprite(
        items::ItemRarity::normal, true);
    for (std::size_t index{}; index < rarity_sprites.size(); ++index) {
        ARPG_REQUIRE(rarity_sprites[index] != platform::MaterialSpriteId::missing);
        for (std::size_t previous{}; previous < index; ++previous) {
            ARPG_REQUIRE(rarity_sprites[index] != rarity_sprites[previous]);
        }
    }

    const platform::MaterialManifestDefinition manifest =
        platform::default_material_manifest();
    for (const auto sprite : slot_sprites) {
        const auto* frame = platform::find_material_frame(manifest, sprite);
        ARPG_REQUIRE(frame != nullptr);
        ARPG_REQUIRE(frame->atlas == platform::MaterialAtlasId::items_ui);
        ARPG_REQUIRE(frame->foot_anchor.x > 0.0F);
        ARPG_REQUIRE(frame->foot_anchor.y > 0.0F);
        ARPG_REQUIRE(frame->perceptual_hash != 0U);
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"three visibility modes and abyss", &visibility_covers_three_modes_and_abyss_bypass},
    {"filter and stable ordinal order", &builder_filters_and_orders_by_stable_ordinal},
    {"catalog Chinese text and invalid fallback", &text_uses_catalog_chinese_rarity_ilvl_and_fallback},
    {"rarity palette and abyss marker", &rarity_palette_and_abyss_marker_are_stable},
    {"overlap resolves upward stably", &overlapping_anchors_resolve_upward_deterministically},
    {"three resolutions stay safe", &supported_resolutions_keep_all_rects_in_safety_area},
    {"full capacity remains bounded", &full_capacity_extreme_layout_is_bounded},
    {"snapshot bytes remain unchanged", &builder_does_not_mutate_snapshot_bytes},
    {"hundred thousand builds no allocation", &hundred_thousand_builds_allocate_nothing},
    {"equipment and rarity resources are unique", &equipment_slots_and_rarities_have_unique_layered_resources},
};

}  // namespace

arpg::test::TestSuite ground_loot_view_suite() noexcept {
    return arpg::test::make_suite("ground_loot_view", kCases);
}
