#include "test_framework.hpp"

#include "combat/combat_world.hpp"
#include "fire_room_material_slice.hpp"
#include "material_animation.hpp"
#include "material_asset_validation.hpp"
#include "material_manifest.hpp"

#include <array>
#include <cstddef>

namespace {

using arpg::combat::MonsterId;
using arpg::platform::MonsterAnimationState;

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

arpg::test::Failure fire_monster_clips_reach_only_committed_opaque_cells() noexcept {
    struct ExpectedFrame final {
        std::uint16_t cell;
        std::uint8_t duration;
        Vector2 anchor;
    };
    struct ExpectedClip final {
        MonsterId monster;
        MonsterAnimationState state;
        const ExpectedFrame* frames;
        std::size_t count;
    };
#define EXPECTED_FRAME(cell, duration, x, y) ExpectedFrame{cell, duration, {x, y}}
    constexpr ExpectedFrame kBomberIdle[]{EXPECTED_FRAME(0U, 6U, 125.0F, 234.0F)};
    constexpr ExpectedFrame kBomberMove[]{EXPECTED_FRAME(1U, 4U, 111.0F, 235.0F)};
    constexpr ExpectedFrame kBomberTelegraph[]{EXPECTED_FRAME(2U, 6U, 141.0F, 235.0F), EXPECTED_FRAME(7U, 6U, 113.0F, 229.0F)};
    constexpr ExpectedFrame kBomberActive[]{EXPECTED_FRAME(8U, 3U, 121.0F, 222.0F), EXPECTED_FRAME(3U, 3U, 112.0F, 228.0F), EXPECTED_FRAME(9U, 4U, 109.0F, 224.0F)};
    constexpr ExpectedFrame kBomberRecovery[]{EXPECTED_FRAME(4U, 4U, 121.0F, 221.0F), EXPECTED_FRAME(10U, 6U, 128.0F, 220.0F)};
    constexpr ExpectedFrame kBomberCooldown[]{EXPECTED_FRAME(5U, 6U, 110.0F, 222.0F)};
    constexpr ExpectedFrame kBomberHurt[]{EXPECTED_FRAME(4U, 5U, 121.0F, 221.0F)};
    constexpr ExpectedFrame kBomberDeath[]{EXPECTED_FRAME(6U, 6U, 130.0F, 225.0F), EXPECTED_FRAME(11U, 12U, 116.0F, 220.0F)};
    constexpr ExpectedFrame kChargerIdle[]{EXPECTED_FRAME(0U, 6U, 125.0F, 250.0F)};
    constexpr ExpectedFrame kChargerMove[]{EXPECTED_FRAME(1U, 4U, 119.0F, 249.0F)};
    constexpr ExpectedFrame kChargerTelegraph[]{EXPECTED_FRAME(2U, 5U, 132.0F, 253.0F), EXPECTED_FRAME(8U, 5U, 144.0F, 227.0F)};
    constexpr ExpectedFrame kChargerActive[]{EXPECTED_FRAME(9U, 3U, 128.0F, 231.0F), EXPECTED_FRAME(3U, 3U, 123.0F, 250.0F), EXPECTED_FRAME(10U, 4U, 128.0F, 229.0F)};
    constexpr ExpectedFrame kChargerRecovery[]{EXPECTED_FRAME(4U, 6U, 132.0F, 249.0F)};
    constexpr ExpectedFrame kChargerCooldown[]{EXPECTED_FRAME(5U, 6U, 155.0F, 252.0F)};
    constexpr ExpectedFrame kChargerHurt[]{EXPECTED_FRAME(4U, 5U, 132.0F, 249.0F)};
    constexpr ExpectedFrame kChargerDeath[]{EXPECTED_FRAME(6U, 6U, 114.0F, 249.0F), EXPECTED_FRAME(11U, 12U, 109.0F, 229.0F)};
#undef EXPECTED_FRAME
#define EXPECTED_CLIP(monster, state, frames) ExpectedClip{MonsterId::monster, MonsterAnimationState::state, frames, std::size(frames)}
    const ExpectedClip kClips[]{
        EXPECTED_CLIP(fire_bomber, idle, kBomberIdle), EXPECTED_CLIP(fire_bomber, move, kBomberMove),
        EXPECTED_CLIP(fire_bomber, telegraph, kBomberTelegraph), EXPECTED_CLIP(fire_bomber, active, kBomberActive),
        EXPECTED_CLIP(fire_bomber, recovery, kBomberRecovery), EXPECTED_CLIP(fire_bomber, cooldown, kBomberCooldown),
        EXPECTED_CLIP(fire_bomber, hurt, kBomberHurt), EXPECTED_CLIP(fire_bomber, death, kBomberDeath),
        EXPECTED_CLIP(fire_charger, idle, kChargerIdle), EXPECTED_CLIP(fire_charger, move, kChargerMove),
        EXPECTED_CLIP(fire_charger, telegraph, kChargerTelegraph), EXPECTED_CLIP(fire_charger, active, kChargerActive),
        EXPECTED_CLIP(fire_charger, recovery, kChargerRecovery), EXPECTED_CLIP(fire_charger, cooldown, kChargerCooldown),
        EXPECTED_CLIP(fire_charger, hurt, kChargerHurt), EXPECTED_CLIP(fire_charger, death, kChargerDeath),
    };
#undef EXPECTED_CLIP
    std::array<bool, 12> bomber_cells{};
    std::array<bool, 12> charger_cells{};
    for (const ExpectedClip& expected_clip : kClips) {
        const auto* const clip = arpg::platform::monster_animation_clip(
            expected_clip.monster, expected_clip.state);
        ARPG_REQUIRE(clip != nullptr);
        ARPG_REQUIRE(clip->frame_count == expected_clip.count);
        std::uint64_t elapsed{};
        for (std::size_t index{}; index < expected_clip.count; ++index) {
            ARPG_REQUIRE(arpg::platform::monster_animation_frame_index(
                *clip, elapsed, false) == index);
            const auto frame = arpg::platform::monster_animation_frame(
                *clip, static_cast<std::uint16_t>(index));
            ARPG_REQUIRE(frame.has_value());
            const std::uint16_t cell = static_cast<std::uint16_t>(
                frame->source.y / 256.0F * 4.0F + frame->source.x / 256.0F);
            ARPG_REQUIRE(frame->source.width == 256.0F);
            ARPG_REQUIRE(frame->source.height == 256.0F);
            ARPG_REQUIRE(cell == expected_clip.frames[index].cell);
            ARPG_REQUIRE(frame->foot_anchor.x == expected_clip.frames[index].anchor.x);
            ARPG_REQUIRE(frame->foot_anchor.y == expected_clip.frames[index].anchor.y);
            (expected_clip.monster == MonsterId::fire_bomber
                    ? bomber_cells : charger_cells)[cell] = true;
            elapsed += expected_clip.frames[index].duration;
        }
    }
    for (const bool reachable : bomber_cells) ARPG_REQUIRE(reachable);
    for (std::size_t cell{}; cell < charger_cells.size(); ++cell) {
        ARPG_REQUIRE(charger_cells[cell] == (cell != 7U));
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"contains all layered fire-room props", &fire_room_has_required_layered_props},
    {"preserves two fire-room navigation routes",
        &fire_room_preserves_two_navigation_routes},
    {"projects attack-broken crate state into fire-room rendering",
        &fire_room_attack_snapshot_hides_only_hit_crate},
    {"fire monster clips reach only committed opaque cells",
        &fire_monster_clips_reach_only_committed_opaque_cells},
};

}  // namespace

arpg::test::TestSuite fire_room_material_slice_suite() noexcept {
    return arpg::test::make_suite("fire_room_material_slice", kCases);
}
