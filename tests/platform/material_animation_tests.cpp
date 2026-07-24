#include "test_framework.hpp"

#include "material_animation.hpp"

#include <array>

namespace {

using arpg::combat::AttackId;
using arpg::combat::MonsterAiPhase;
using arpg::combat::MonsterId;
using arpg::combat::PlayerState;
using arpg::platform::AnimationClipId;
using arpg::platform::MaterialSpriteId;
using arpg::platform::PlayerAnimationClipId;

arpg::test::Failure player_animation_clips_have_required_unique_uv_frames() noexcept {
    constexpr std::array<PlayerAnimationClipId, 11> kClips{{
        PlayerAnimationClipId::idle, PlayerAnimationClipId::move,
        PlayerAnimationClipId::jump, PlayerAnimationClipId::j1,
        PlayerAnimationClipId::j2, PlayerAnimationClipId::j3,
        PlayerAnimationClipId::launcher, PlayerAnimationClipId::hurt,
        PlayerAnimationClipId::down, PlayerAnimationClipId::get_up,
        PlayerAnimationClipId::death,
    }};
    constexpr std::array<std::uint16_t, 11> kMinimumFrames{{
        16U, 20U, 24U, 18U, 22U, 26U, 24U, 10U, 16U, 14U, 24U,
    }};
    for (std::size_t index = 0U; index < kClips.size(); ++index) {
        const auto* clip = arpg::platform::player_animation_clip(kClips[index]);
        ARPG_REQUIRE(clip != nullptr);
        ARPG_REQUIRE(clip->frame_count >= kMinimumFrames[index]);
        ARPG_REQUIRE(clip->frames_per_second == 24U);
        for (std::uint16_t frame = 0U; frame < clip->frame_count; ++frame) {
            const auto current = arpg::platform::player_animation_frame(*clip, frame);
            ARPG_REQUIRE(current.has_value());
            ARPG_REQUIRE(current->foot_anchor.x >= 0.0F);
            ARPG_REQUIRE(current->foot_anchor.y >= 0.0F);
            ARPG_REQUIRE(current->weapon_anchor.x >= 0.0F);
            ARPG_REQUIRE(current->weapon_anchor.y >= 0.0F);
            if (frame == 0U) continue;
            const auto previous = arpg::platform::player_animation_frame(*clip,
                static_cast<std::uint16_t>(frame - 1U));
            ARPG_REQUIRE(previous.has_value());
            ARPG_REQUIRE(current->source.x != previous->source.x
                || current->source.y != previous->source.y);
        }
    }
    return {};
}

arpg::test::Failure player_animation_frame_index_matches_timeline_progress() noexcept {
    const auto* const j3 = arpg::platform::player_animation_clip(
        PlayerAnimationClipId::j3);
    ARPG_REQUIRE(j3 != nullptr);
    ARPG_REQUIRE(arpg::platform::player_animation_frame_index(*j3, 0U, 28U, false)
        == 0U);
    ARPG_REQUIRE(arpg::platform::player_animation_frame_index(*j3, 14U, 28U, false)
        == 13U);
    ARPG_REQUIRE(arpg::platform::player_animation_frame_index(*j3, 28U, 28U, false)
        == 25U);
    ARPG_REQUIRE(arpg::platform::player_animation_frame_index(*j3, 30U, 28U, false)
        == 25U);
    ARPG_REQUIRE(arpg::platform::player_animation_frame_index(*j3, 28U, 28U, true)
        == 0U);
    return {};
}

arpg::test::Failure material_animation_selects_launcher_active() noexcept {
    ARPG_REQUIRE(arpg::platform::select_player_sprite(PlayerState::attack_active,
        AttackId::launcher) == MaterialSpriteId::player_launcher);
    return {};
}

arpg::test::Failure material_animation_covers_all_player_states_and_attacks() noexcept {
    constexpr std::array<AttackId, 5> kAttacks{{
        AttackId::j1, AttackId::j2, AttackId::j3, AttackId::launcher,
        AttackId::air_j,
    }};
    constexpr std::array<MaterialSpriteId, 5> kAttackSprites{{
        MaterialSpriteId::player_j1, MaterialSpriteId::player_j2,
        MaterialSpriteId::player_j3, MaterialSpriteId::player_launcher,
        MaterialSpriteId::player_air_j,
    }};
    for (std::size_t index = 0U; index < kAttacks.size(); ++index) {
        for (const PlayerState state : {PlayerState::attack_startup,
                 PlayerState::attack_active, PlayerState::attack_recovery}) {
            ARPG_REQUIRE(arpg::platform::select_player_sprite(state, kAttacks[index])
                == kAttackSprites[index]);
        }
    }
    ARPG_REQUIRE(arpg::platform::select_player_sprite(
        PlayerState::idle, AttackId::j1) == MaterialSpriteId::player_idle);
    ARPG_REQUIRE(arpg::platform::select_player_sprite(
        PlayerState::move, AttackId::j1) == MaterialSpriteId::player_move);
    ARPG_REQUIRE(arpg::platform::select_player_sprite(
        PlayerState::jump_rise, AttackId::j1) == MaterialSpriteId::player_jump_rise);
    ARPG_REQUIRE(arpg::platform::select_player_sprite(
        PlayerState::jump_fall, AttackId::j1) == MaterialSpriteId::player_jump_fall);
    ARPG_REQUIRE(arpg::platform::select_player_sprite(
        PlayerState::landing, AttackId::j1) == MaterialSpriteId::player_landing);
    ARPG_REQUIRE(arpg::platform::select_player_sprite(
        PlayerState::attack_active, AttackId::none) == MaterialSpriteId::player_idle);
    return {};
}

arpg::test::Failure material_animation_covers_monsters_and_ai_phases() noexcept {
    constexpr std::array<MonsterId, 8> kMonsters{{
        MonsterId::fire_bomber, MonsterId::fire_charger,
        MonsterId::water_bulwark, MonsterId::water_support,
        MonsterId::lightning_shooter, MonsterId::lightning_dasher,
        MonsterId::chaos_chaser, MonsterId::chaos_hazard,
    }};
    constexpr std::array<MonsterAiPhase, 7> kPhases{{
        MonsterAiPhase::idle, MonsterAiPhase::move, MonsterAiPhase::telegraph,
        MonsterAiPhase::active, MonsterAiPhase::recovery,
        MonsterAiPhase::cooldown, MonsterAiPhase::defeated,
    }};
    for (std::size_t index = 0U; index < kMonsters.size(); ++index) {
        for (const MonsterAiPhase phase : kPhases) {
            ARPG_REQUIRE(arpg::platform::select_monster_sprite(
                kMonsters[index], phase) != MaterialSpriteId::missing);
        }
    }
    ARPG_REQUIRE(arpg::platform::select_monster_sprite(
        MonsterId::count, MonsterAiPhase::idle) == MaterialSpriteId::missing);
    return {};
}


arpg::test::Failure material_animation_exposes_fixed_capacity_clips() noexcept {
    const auto* idle = arpg::platform::material_animation_clip(AnimationClipId::player_idle);
    ARPG_REQUIRE(idle != nullptr);
    ARPG_REQUIRE(idle->frame_count == 16U);
    ARPG_REQUIRE(idle->minimum_frames == 16U);
    ARPG_REQUIRE(arpg::platform::material_animation_frame_sprite(*idle, 0U)
        != MaterialSpriteId::missing);
    ARPG_REQUIRE(arpg::platform::material_animation_frame_sprite(*idle, idle->frame_count)
        == MaterialSpriteId::missing);
    const auto* attack = arpg::platform::material_animation_clip(AnimationClipId::monster_attack);
    ARPG_REQUIRE(attack != nullptr);
    ARPG_REQUIRE(attack->frame_count >= 20U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"player action clips use unique UV frames",
        &player_animation_clips_have_required_unique_uv_frames},
    {"player action frame index follows timeline progress",
        &player_animation_frame_index_matches_timeline_progress},
    {"selects launcher active", &material_animation_selects_launcher_active},
    {"covers player states and attacks",
        &material_animation_covers_all_player_states_and_attacks},
    {"covers monsters and ai phases",
        &material_animation_covers_monsters_and_ai_phases},
    {"exposes fixed capacity clips",
        &material_animation_exposes_fixed_capacity_clips},
};

}  // namespace

arpg::test::TestSuite material_animation_suite() noexcept {
    return arpg::test::make_suite("material_animation", kCases);
}
