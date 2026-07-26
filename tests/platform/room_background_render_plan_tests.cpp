#include "test_framework.hpp"

#include "dungeon/dungeon_types.hpp"
#include "material_manifest.hpp"
#include "room_background_render_plan.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace {

using arpg::dungeon::DungeonElement;
using arpg::platform::MaterialAtlasDefinition;
using arpg::platform::MaterialAtlasId;
using arpg::platform::MaterialEcology;

const MaterialAtlasDefinition* find_atlas(MaterialAtlasId id) noexcept {
    const auto manifest = arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        if (manifest.atlases[index].id == id) return &manifest.atlases[index];
    }
    return nullptr;
}

arpg::test::Failure background_manifest_preserves_old_ids_and_adds_four_pairs() noexcept {
    static_assert(static_cast<std::size_t>(MaterialAtlasId::ui_material) == 21U);
    static_assert(static_cast<std::size_t>(
        MaterialAtlasId::fire_room_background) == 22U);
    static_assert(static_cast<std::size_t>(
        MaterialAtlasId::water_room_background) == 23U);
    static_assert(static_cast<std::size_t>(
        MaterialAtlasId::lightning_room_background) == 24U);
    static_assert(static_cast<std::size_t>(
        MaterialAtlasId::chaos_room_background) == 25U);
    static_assert(static_cast<std::size_t>(MaterialAtlasId::count) == 27U);

    const auto manifest = arpg::platform::default_material_manifest();
    ARPG_REQUIRE(manifest.atlas_count
        == static_cast<std::size_t>(MaterialAtlasId::count));
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        ARPG_REQUIRE(static_cast<std::size_t>(manifest.atlases[index].id)
            == index);
    }

    struct Expected final {
        MaterialAtlasId id;
        MaterialEcology ecology;
        std::string_view color_path;
        std::string_view material_path;
    };
    constexpr std::array<Expected, 4> expected{{
        {MaterialAtlasId::fire_room_background, MaterialEcology::fire,
            "assets/stage12/fire_room_background.png",
            "assets/stage12/fire_room_background_material.png"},
        {MaterialAtlasId::water_room_background, MaterialEcology::water,
            "assets/stage12/water_room_background.png",
            "assets/stage12/water_room_background_material.png"},
        {MaterialAtlasId::lightning_room_background, MaterialEcology::lightning,
            "assets/stage12/lightning_room_background.png",
            "assets/stage12/lightning_room_background_material.png"},
        {MaterialAtlasId::chaos_room_background, MaterialEcology::chaos,
            "assets/stage12/chaos_room_background.png",
            "assets/stage12/chaos_room_background_material.png"},
    }};
    for (const Expected& item : expected) {
        const MaterialAtlasDefinition* const atlas = find_atlas(item.id);
        ARPG_REQUIRE(atlas != nullptr);
        ARPG_REQUIRE(atlas->width == 2560);
        ARPG_REQUIRE(atlas->height == 1440);
        ARPG_REQUIRE(atlas->rgba_bytes == 14'745'600U);
        ARPG_REQUIRE(atlas->ecology == item.ecology);
        ARPG_REQUIRE(std::string_view{atlas->color_path} == item.color_path);
        ARPG_REQUIRE(std::string_view{atlas->material_path} == item.material_path);
    }
    return {};
}

arpg::test::Failure background_plan_maps_every_ecology_to_its_dedicated_atlas() noexcept {
    constexpr std::array<DungeonElement, 4> ecologies{{
        DungeonElement::fire, DungeonElement::water,
        DungeonElement::lightning, DungeonElement::chaos,
    }};
    constexpr std::array<MaterialAtlasId, 4> atlases{{
        MaterialAtlasId::fire_room_background,
        MaterialAtlasId::water_room_background,
        MaterialAtlasId::lightning_room_background,
        MaterialAtlasId::chaos_room_background,
    }};
    for (std::size_t index{}; index < ecologies.size(); ++index) {
        const auto plan = arpg::platform::room_background_render_plan(
            ecologies[index]);
        ARPG_REQUIRE(plan.atlas == atlases[index]);
        ARPG_REQUIRE(arpg::test::near(plan.source.x, 0.0F));
        ARPG_REQUIRE(arpg::test::near(plan.source.y, 0.0F));
        ARPG_REQUIRE(arpg::test::near(plan.source.width, 2560.0F));
        ARPG_REQUIRE(arpg::test::near(plan.source.height, 1440.0F));
    }
    return {};
}

arpg::test::Failure background_plan_uses_native_source_and_downscale_only() noexcept {
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::room_background_scale(2560.0F, 1440.0F), 1.0F));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::room_background_scale(1920.0F, 1080.0F), 0.75F));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::room_background_scale(1280.0F, 720.0F), 0.5F));
    ARPG_REQUIRE(arpg::platform::room_background_scale(3840.0F, 2160.0F)
        <= 1.0F);

    constexpr std::array<MaterialAtlasId, 5> legacy_environment_atlases{{
        MaterialAtlasId::environment,
        MaterialAtlasId::fire_environment,
        MaterialAtlasId::water_environment,
        MaterialAtlasId::lightning_environment,
        MaterialAtlasId::chaos_environment,
    }};
    for (const DungeonElement ecology : {DungeonElement::fire,
             DungeonElement::water, DungeonElement::lightning,
             DungeonElement::chaos}) {
        const auto plan = arpg::platform::room_background_render_plan(ecology);
        for (const MaterialAtlasId legacy : legacy_environment_atlases) {
            ARPG_REQUIRE(plan.atlas != legacy);
        }
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"preserves old IDs and adds four background pairs",
        &background_manifest_preserves_old_ids_and_adds_four_pairs},
    {"maps every ecology to its dedicated background atlas",
        &background_plan_maps_every_ecology_to_its_dedicated_atlas},
    {"uses native source and downscale only",
        &background_plan_uses_native_source_and_downscale_only},
};

}  // namespace

arpg::test::TestSuite room_background_render_plan_suite() noexcept {
    return arpg::test::make_suite("room_background_render_plan", kCases);
}
