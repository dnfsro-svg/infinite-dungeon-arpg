#include "test_framework.hpp"

#include "combat_audio.hpp"
#include "combat_view_math.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "dungeon_view_math.hpp"

#include <array>
#include <cstring>
#include <cstdint>

namespace {

using arpg::combat::HazardSnapshot;
using arpg::combat::MonsterAiPhase;
using arpg::combat::MonsterAffixDanger;
using arpg::combat::MonsterAffixId;
using arpg::combat::MonsterAffixInstance;
using arpg::combat::MonsterAffixTier;
using arpg::combat::MonsterAffixWarning;
using arpg::combat::MonsterId;
using arpg::combat::MonsterSnapshot;
using arpg::combat::PlayerSnapshot;
using arpg::combat::ProjectileSnapshot;
using arpg::combat::Vec3;
using arpg::dungeon::DungeonElement;
using arpg::platform::HazardVisualMode;
using arpg::platform::MonsterWarningMode;
using arpg::platform::MonsterVisual;

arpg::test::Failure monster_labels_keep_a_readable_size_and_outline() noexcept {
    const auto style = arpg::platform::monster_label_text_style(0.55F);
    ARPG_REQUIRE(style.role_font_size >= 14);
    ARPG_REQUIRE(style.phase_font_size >= 12);
    ARPG_REQUIRE(style.affix_font_size >= 11);
    ARPG_REQUIRE(style.outline_pixels >= 2);
    return {};
}

bool same_color(arpg::platform::Rgba8 lhs, arpg::platform::Rgba8 rhs) noexcept {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

arpg::test::Failure all_monster_roles_have_unique_labels_and_ecology_accent() noexcept {
    constexpr std::array<MonsterId, 8> kIds{{
        MonsterId::fire_bomber,
        MonsterId::fire_charger,
        MonsterId::water_bulwark,
        MonsterId::water_support,
        MonsterId::lightning_shooter,
        MonsterId::lightning_dasher,
        MonsterId::chaos_chaser,
        MonsterId::chaos_hazard,
    }};
    std::array<const char*, kIds.size()> labels{};
    for (std::size_t index = 0; index < kIds.size(); ++index) {
        const MonsterVisual visual = arpg::platform::monster_visual(
            kIds[index], MonsterAiPhase::move, DungeonElement::lightning);
        ARPG_REQUIRE(visual.role_label != nullptr);
        ARPG_REQUIRE(same_color(visual.accent,
            arpg::platform::monster_ecology_color(DungeonElement::lightning)));
        labels[index] = visual.role_label;
        for (std::size_t prior = 0; prior < index; ++prior) {
            ARPG_REQUIRE(std::strcmp(labels[index], labels[prior]) != 0);
        }
    }
    return {};
}

arpg::test::Failure priority_phases_expose_warning_visuals() noexcept {
    constexpr std::array<MonsterId, 4> kPriorityIds{{
        MonsterId::fire_bomber,
        MonsterId::fire_charger,
        MonsterId::lightning_dasher,
        MonsterId::chaos_hazard,
    }};
    for (const MonsterId id : kPriorityIds) {
        ARPG_REQUIRE(arpg::platform::monster_visual(
            id, MonsterAiPhase::telegraph, DungeonElement::fire).warning_mode
            == MonsterWarningMode::telegraph);
        ARPG_REQUIRE(arpg::platform::monster_visual(
            id, MonsterAiPhase::active, DungeonElement::fire).warning_mode
            == MonsterWarningMode::active);
    }
    ARPG_REQUIRE(arpg::platform::monster_visual(
        MonsterId::water_bulwark, MonsterAiPhase::move,
        DungeonElement::water).warning_mode == MonsterWarningMode::none);
    ARPG_REQUIRE(arpg::platform::monster_visual(
        MonsterId::water_bulwark, MonsterAiPhase::telegraph,
        DungeonElement::water).warning_mode == MonsterWarningMode::telegraph);
    ARPG_REQUIRE(arpg::platform::monster_visual(
        MonsterId::lightning_shooter, MonsterAiPhase::active,
        DungeonElement::lightning).warning_mode == MonsterWarningMode::active);
    ARPG_REQUIRE(!arpg::platform::monster_visual(
        MonsterId::water_support, MonsterAiPhase::telegraph,
        DungeonElement::water).priority_warning);
    return {};
}

arpg::test::Failure inactive_monsters_are_hidden_and_player_hp_is_clamped() noexcept {
    MonsterSnapshot monster{};
    ARPG_REQUIRE(!arpg::platform::monster_visible(monster));
    monster.active = true;
    ARPG_REQUIRE(arpg::platform::monster_visible(monster));
    monster.ai_phase = MonsterAiPhase::defeated;
    ARPG_REQUIRE(!arpg::platform::monster_visible(monster));

    PlayerSnapshot player{};
    player.max_hp = 100;
    player.hp = -10;
    ARPG_REQUIRE(arpg::test::near(arpg::platform::player_hp_ratio(player), 0.0, 1.0e-4));
    player.hp = 250;
    ARPG_REQUIRE(arpg::test::near(arpg::platform::player_hp_ratio(player), 1.0, 1.0e-4));
    player.max_hp = 0;
    ARPG_REQUIRE(arpg::test::near(arpg::platform::player_hp_ratio(player), 0.0, 1.0e-4));
    return {};
}

arpg::test::Failure hazard_modes_and_projected_effects_are_explicit() noexcept {
    HazardSnapshot hazard{};
    ARPG_REQUIRE(arpg::platform::hazard_visual_mode(hazard)
        == HazardVisualMode::hidden);
    hazard.active = true;
    hazard.center = Vec3{1.5F, -0.5F, 0.0F};
    hazard.telegraph_ticks = 12;
    ARPG_REQUIRE(arpg::platform::hazard_visual_mode(hazard)
        == HazardVisualMode::telegraph);
    hazard.telegraph_ticks = 0;
    hazard.active_ticks = 18;
    ARPG_REQUIRE(arpg::platform::hazard_visual_mode(hazard)
        == HazardVisualMode::active);

    ProjectileSnapshot projectile{};
    projectile.position = Vec3{2.0F, 1.0F, 0.5F};
    const auto projected_projectile = arpg::platform::project_projectile_position(
        projectile, 1280.0F, 720.0F);
    const auto direct = arpg::platform::project_combat_position(
        projectile.position, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(projected_projectile.x, direct.x, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(projected_projectile.y, direct.y, 1.0e-4));
    const auto projected_hazard = arpg::platform::project_hazard_center(
        hazard, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(projected_hazard.x,
        arpg::platform::project_combat_position(hazard.center, 1280.0F, 720.0F).x,
        1.0e-4));
    return {};
}

arpg::test::Failure monster_attack_audio_emits_one_low_layer() noexcept {
    arpg::combat::CombatEvent event{};
    event.kind = arpg::combat::CombatEventKind::player_hit;
    ARPG_REQUIRE(arpg::platform::route_audio_cues(event)
        == arpg::platform::audio_cue_mask(arpg::platform::AudioCue::low));
    event.kind = arpg::combat::CombatEventKind::player_hurt_started;
    ARPG_REQUIRE(arpg::platform::route_audio_cues(event) == 0);
    return {};
}

arpg::test::Failure affix_badges_consume_catalog_names_tiers_and_danger() noexcept {
    constexpr std::array<MonsterAffixId, 12> kAffixes{{
        MonsterAffixId::mighty, MonsterAffixId::frenzy,
        MonsterAffixId::swift, MonsterAffixId::armored,
        MonsterAffixId::shielding, MonsterAffixId::multishot,
        MonsterAffixId::burning_ground, MonsterAffixId::chilling,
        MonsterAffixId::chain_lightning, MonsterAffixId::chaos_corrosion,
        MonsterAffixId::blink_assault, MonsterAffixId::death_blast,
    }};
    constexpr std::array<const char*, 12> kNames{{
        "MGT", "FRZ", "SWF", "ARM", "SHD", "MULTI", "BURN", "CHILL",
        "CHAIN", "CORR", "BLINK", "DEATH",
    }};
    for (std::size_t index = 0U; index < kAffixes.size(); ++index) {
        const auto badge = arpg::platform::monster_affix_badge(
            {kAffixes[index], MonsterAffixTier::m1});
        ARPG_REQUIRE(std::strcmp(badge.short_name, kNames[index]) == 0);
        ARPG_REQUIRE(std::strcmp(badge.tier_text, "M1") == 0);
        ARPG_REQUIRE(badge.danger
            == arpg::combat::monster_affix_definition(kAffixes[index])->danger);
    }
    const auto tier_two = arpg::platform::monster_affix_badge(
        {MonsterAffixId::mighty, MonsterAffixTier::m2});
    const auto tier_three = arpg::platform::monster_affix_badge(
        {MonsterAffixId::death_blast, MonsterAffixTier::m3});
    ARPG_REQUIRE(std::strcmp(tier_two.tier_text, "M2") == 0);
    ARPG_REQUIRE(std::strcmp(tier_three.tier_text, "M3") == 0);
    ARPG_REQUIRE(tier_three.danger == MonsterAffixDanger::high);
    return {};
}

arpg::test::Failure affix_presentation_has_category_colors_and_high_danger_pulse() noexcept {
    const auto base = arpg::platform::monster_affix_badge(
        {MonsterAffixId::mighty, MonsterAffixTier::m1});
    const auto defense = arpg::platform::monster_affix_badge(
        {MonsterAffixId::armored, MonsterAffixTier::m1});
    const auto fire = arpg::platform::monster_affix_badge(
        {MonsterAffixId::burning_ground, MonsterAffixTier::m1});
    const auto water = arpg::platform::monster_affix_badge(
        {MonsterAffixId::chilling, MonsterAffixTier::m1});
    const auto lightning = arpg::platform::monster_affix_badge(
        {MonsterAffixId::chain_lightning, MonsterAffixTier::m1});
    const auto chaos = arpg::platform::monster_affix_badge(
        {MonsterAffixId::chaos_corrosion, MonsterAffixTier::m1});
    ARPG_REQUIRE(!same_color(base.color, defense.color));
    ARPG_REQUIRE(!same_color(base.color, fire.color));
    ARPG_REQUIRE(!same_color(base.color, water.color));
    ARPG_REQUIRE(!same_color(base.color, lightning.color));
    ARPG_REQUIRE(!same_color(base.color, chaos.color));
    ARPG_REQUIRE(!same_color(defense.color, fire.color));
    ARPG_REQUIRE(!same_color(fire.color, water.color));
    ARPG_REQUIRE(!same_color(water.color, lightning.color));
    ARPG_REQUIRE(!same_color(lightning.color, chaos.color));

    const auto high = arpg::platform::monster_affix_outline(
        {MonsterAffixId::death_blast, MonsterAffixTier::m3}, 0U);
    const auto high_later = arpg::platform::monster_affix_outline(
        {MonsterAffixId::death_blast, MonsterAffixTier::m3}, 15U);
    const auto low = arpg::platform::monster_affix_outline(
        {MonsterAffixId::mighty, MonsterAffixTier::m1}, 0U);
    ARPG_REQUIRE(high.color.r > high.color.g);
    ARPG_REQUIRE(high.color.r > high.color.b);
    ARPG_REQUIRE(high.alpha != high_later.alpha);
    ARPG_REQUIRE(low.alpha == 255U);
    return {};
}

arpg::test::Failure blink_affix_warning_uses_snapshot_ticks_not_ai_phase() noexcept {
    MonsterSnapshot warning{};
    warning.affix_warning = MonsterAffixWarning::blink;
    warning.affix_warning_ticks = 30U;
    warning.ai_phase = MonsterAiPhase::move;
    ARPG_REQUIRE(arpg::platform::blink_affix_warning_visible(warning));
    ARPG_REQUIRE(arpg::platform::blink_affix_warning_actor_radius(warning) > 0.0F);
    ARPG_REQUIRE(arpg::platform::blink_affix_warning_ground_radius(warning) > 0.0F);

    const float actor_radius = arpg::platform::blink_affix_warning_actor_radius(warning);
    const float ground_radius = arpg::platform::blink_affix_warning_ground_radius(warning);
    warning.ai_phase = MonsterAiPhase::recovery;
    ARPG_REQUIRE(arpg::platform::blink_affix_warning_visible(warning));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::blink_affix_warning_actor_radius(warning), actor_radius, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::blink_affix_warning_ground_radius(warning), ground_radius, 1.0e-4));

    warning.affix_warning_ticks = 0U;
    ARPG_REQUIRE(!arpg::platform::blink_affix_warning_visible(warning));
    ARPG_REQUIRE(arpg::platform::blink_affix_warning_actor_radius(warning) == 0.0F);
    ARPG_REQUIRE(arpg::platform::blink_affix_warning_ground_radius(warning) == 0.0F);
    return {};
}

arpg::test::Failure hazards_and_affix_warning_audio_are_distinct_and_throttled() noexcept {
    const auto native = arpg::platform::hazard_color(arpg::combat::HazardKind::native);
    const auto burning = arpg::platform::hazard_color(arpg::combat::HazardKind::burning);
    const auto chain = arpg::platform::hazard_color(arpg::combat::HazardKind::chain_lightning);
    const auto death = arpg::platform::hazard_color(arpg::combat::HazardKind::death_blast);
    ARPG_REQUIRE(!same_color(native, burning));
    ARPG_REQUIRE(!same_color(native, chain));
    ARPG_REQUIRE(!same_color(native, death));
    ARPG_REQUIRE(!same_color(burning, chain));
    ARPG_REQUIRE(!same_color(chain, death));

    arpg::combat::CombatEvent blink{};
    blink.kind = arpg::combat::CombatEventKind::affix_blink_warning;
    arpg::combat::CombatEvent chain_event{};
    chain_event.kind = arpg::combat::CombatEventKind::affix_chain_warning;
    arpg::combat::CombatEvent death_event{};
    death_event.kind = arpg::combat::CombatEventKind::affix_death_warning;
    ARPG_REQUIRE(arpg::platform::route_audio_cues(blink)
        == arpg::platform::audio_cue_mask(arpg::platform::AudioCue::blink_warning));
    ARPG_REQUIRE(arpg::platform::route_audio_cues(chain_event)
        == arpg::platform::audio_cue_mask(arpg::platform::AudioCue::chain_warning));
    ARPG_REQUIRE(arpg::platform::route_audio_cues(death_event)
        == arpg::platform::audio_cue_mask(arpg::platform::AudioCue::death_warning));

    arpg::platform::WarningAudioThrottle throttle{};
    ARPG_REQUIRE(throttle.allow(arpg::platform::AudioCue::blink_warning, 24U));
    ARPG_REQUIRE(!throttle.allow(arpg::platform::AudioCue::blink_warning, 24U));
    ARPG_REQUIRE(!throttle.allow(arpg::platform::AudioCue::blink_warning, 28U));
    ARPG_REQUIRE(!throttle.allow(arpg::platform::AudioCue::blink_warning, 35U));
    ARPG_REQUIRE(throttle.allow(arpg::platform::AudioCue::blink_warning, 36U));
    ARPG_REQUIRE(throttle.allow(arpg::platform::AudioCue::chain_warning, 24U));
    ARPG_REQUIRE(throttle.allow(arpg::platform::AudioCue::blink_warning, 2U));

    arpg::platform::WarningAudioThrottle near_wrap{};
    constexpr std::uint64_t kMax = UINT64_MAX;
    ARPG_REQUIRE(near_wrap.allow(arpg::platform::AudioCue::blink_warning, kMax - 5U));
    ARPG_REQUIRE(!near_wrap.allow(arpg::platform::AudioCue::blink_warning, kMax - 1U));
    ARPG_REQUIRE(near_wrap.allow(arpg::platform::AudioCue::blink_warning, 2U));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"monster labels retain readable text treatment", &monster_labels_keep_a_readable_size_and_outline},
    {"unique monster labels and ecology accent",
     &all_monster_roles_have_unique_labels_and_ecology_accent},
    {"priority monster warnings", &priority_phases_expose_warning_visuals},
    {"inactive slots and player HP clamp",
     &inactive_monsters_are_hidden_and_player_hp_is_clamped},
    {"hazard modes and effect projection",
     &hazard_modes_and_projected_effects_are_explicit},
    {"monster attack audio aggregation", &monster_attack_audio_emits_one_low_layer},
    {"affix badges consume catalog", &affix_badges_consume_catalog_names_tiers_and_danger},
    {"affix presentation categories and pulse", &affix_presentation_has_category_colors_and_high_danger_pulse},
    {"blink affix warning consumes snapshot ticks", &blink_affix_warning_uses_snapshot_ticks_not_ai_phase},
    {"hazard colors and affix warning audio", &hazards_and_affix_warning_audio_are_distinct_and_throttled},
};

}  // namespace

arpg::test::TestSuite monster_view_suite() noexcept {
    return arpg::test::make_suite("monster_view", kCases);
}
