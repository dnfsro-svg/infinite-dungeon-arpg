#include "test_framework.hpp"

#include "combat/combat_world.hpp"
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

arpg::test::Failure fire_room_attack_snapshot_hides_only_hit_crate() noexcept {
    arpg::combat::CombatEncounterConfig config{};
    config.player_spawn = {-4.50F, 0.0F, 0.0F};
    config.fire_room_obstacles = true;
    arpg::combat::CombatWorld world{config};

    ARPG_REQUIRE(world.snapshot().fire_crate_count == 2U);
    ARPG_REQUIRE(arpg::platform::fire_room_crate_visible(world.snapshot(), 0U));
    ARPG_REQUIRE(arpg::platform::fire_room_crate_visible(world.snapshot(), 1U));
    ARPG_REQUIRE(world.queue_action(arpg::combat::Action::light));
    for (int tick{}; tick < 8; ++tick) world.tick({});

    const arpg::combat::CombatSnapshot broken = world.snapshot();
    ARPG_REQUIRE(!broken.fire_crates[0].intact);
    ARPG_REQUIRE(broken.fire_crates[0].broken_tick != 0U);
    ARPG_REQUIRE(broken.fire_crates[1].intact);
    ARPG_REQUIRE(!arpg::platform::fire_room_crate_visible(broken, 0U));
    ARPG_REQUIRE(arpg::platform::fire_room_crate_visible(broken, 1U));

    for (int tick{}; tick < 24; ++tick) world.tick({});
    ARPG_REQUIRE(world.queue_action(arpg::combat::Action::light));
    for (int tick{}; tick < 8; ++tick) world.tick({});
    const arpg::combat::CombatSnapshot repeated = world.snapshot();
    ARPG_REQUIRE(repeated.fire_crates[0].broken_tick == broken.fire_crates[0].broken_tick);
    ARPG_REQUIRE(!arpg::platform::fire_room_crate_visible(repeated, 0U));

    config.player_spawn = {1.80F, 0.0F, 0.0F};
    arpg::combat::CombatWorld right_crate_world{config};
    ARPG_REQUIRE(right_crate_world.queue_action(arpg::combat::Action::light));
    for (int tick{}; tick < 8; ++tick) right_crate_world.tick({});
    const arpg::combat::CombatSnapshot right_broken = right_crate_world.snapshot();
    ARPG_REQUIRE(right_broken.fire_crates[0].intact);
    ARPG_REQUIRE(!right_broken.fire_crates[1].intact);
    ARPG_REQUIRE(arpg::platform::fire_room_crate_visible(right_broken, 0U));
    ARPG_REQUIRE(!arpg::platform::fire_room_crate_visible(right_broken, 1U));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"contains all layered fire-room props", &fire_room_has_required_layered_props},
    {"preserves two fire-room navigation routes",
        &fire_room_preserves_two_navigation_routes},
    {"projects attack-broken crate state into fire-room rendering",
        &fire_room_attack_snapshot_hides_only_hit_crate},
};

}  // namespace

arpg::test::TestSuite fire_room_material_slice_suite() noexcept {
    return arpg::test::make_suite("fire_room_material_slice", kCases);
}
