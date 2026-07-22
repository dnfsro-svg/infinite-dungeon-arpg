#include "test_framework.hpp"

#include "fire_room_material_slice.hpp"
#include "material_asset_validation.hpp"
#include "material_manifest.hpp"

#include <array>
#include <cstddef>

namespace {

arpg::test::Failure fire_room_has_required_layered_props() noexcept {
    const arpg::platform::FireRoomMaterialSlice& slice =
        arpg::platform::fire_room_material_slice();
    constexpr std::array<arpg::platform::FireRoomPropId, 10> kRequired{{
        arpg::platform::FireRoomPropId::floor,
        arpg::platform::FireRoomPropId::wall,
        arpg::platform::FireRoomPropId::door,
        arpg::platform::FireRoomPropId::torch,
        arpg::platform::FireRoomPropId::chain,
        arpg::platform::FireRoomPropId::banner,
        arpg::platform::FireRoomPropId::weapon_rack,
        arpg::platform::FireRoomPropId::bone_pile,
        arpg::platform::FireRoomPropId::breakable_crate,
        arpg::platform::FireRoomPropId::solid_brazier,
    }};
    for (std::size_t index{}; index < kRequired.size(); ++index) {
        const auto& prop = slice.props[index];
        ARPG_REQUIRE(prop.id == kRequired[index]);
        ARPG_REQUIRE(prop.sprite != arpg::platform::MaterialSpriteId::missing);
        const arpg::platform::MaterialManifestDefinition manifest =
            arpg::platform::default_material_manifest();
        const arpg::platform::MaterialFrameDefinition* frame{};
        const arpg::platform::MaterialAtlasDefinition* atlas{};
        for (std::size_t frame_index{}; frame_index < manifest.frame_count;
             ++frame_index) {
            if (manifest.frames[frame_index].id == prop.sprite) {
                frame = &manifest.frames[frame_index];
                break;
            }
        }
        for (std::size_t atlas_index{}; atlas_index < manifest.atlas_count;
             ++atlas_index) {
            if (frame != nullptr && manifest.atlases[atlas_index].id == frame->atlas) {
                atlas = &manifest.atlases[atlas_index];
                break;
            }
        }
        ARPG_REQUIRE(frame != nullptr);
        ARPG_REQUIRE(atlas != nullptr);
        ARPG_REQUIRE(arpg::platform::validate_material_frame(*atlas, *frame).valid);
    }
    ARPG_REQUIRE(slice.props[static_cast<std::size_t>(
        arpg::platform::FireRoomPropId::breakable_crate)].breakable);
    ARPG_REQUIRE(slice.props[static_cast<std::size_t>(
        arpg::platform::FireRoomPropId::solid_brazier)].blocks_navigation);
    return {};
}

arpg::test::Failure fire_room_preserves_two_navigation_routes() noexcept {
    const auto& obstacles = arpg::platform::fire_room_obstacles();
    ARPG_REQUIRE(obstacles.size() == 3U);
    ARPG_REQUIRE(arpg::platform::fire_room_route_is_clear(-4.0F, -4.0F));
    ARPG_REQUIRE(arpg::platform::fire_room_route_is_clear(4.0F, 4.0F));
    ARPG_REQUIRE(!arpg::platform::fire_room_route_is_clear(0.0F, 0.0F));
    ARPG_REQUIRE(arpg::platform::fire_room_has_two_navigation_routes());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"contains all layered fire-room props", &fire_room_has_required_layered_props},
    {"preserves two fire-room navigation routes",
        &fire_room_preserves_two_navigation_routes},
};

}  // namespace

arpg::test::TestSuite fire_room_material_slice_suite() noexcept {
    return arpg::test::make_suite("fire_room_material_slice", kCases);
}
