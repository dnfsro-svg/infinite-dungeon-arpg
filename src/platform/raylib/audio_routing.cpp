#include "audio_routing.hpp"

#include <array>
#include <cstddef>

namespace arpg::platform {
namespace {

constexpr std::size_t kInvalidIndex = 9U;
constexpr std::size_t kWarningBlinkIndex = 6U;
constexpr std::size_t kWarningChainIndex = 7U;
constexpr std::size_t kWarningDeathIndex = 8U;

[[nodiscard]] constexpr AudioPlan plan_for(AudioCue cue) noexcept {
    return AudioPlan{audio_cue_mask(cue)};
}

[[nodiscard]] constexpr std::size_t cue_category_index(AudioCue cue) noexcept {
    switch (cue) {
    case AudioCue::swing_light:
    case AudioCue::swing_finisher:
    case AudioCue::swing_launcher:
        return 0U;
    case AudioCue::impact: return 1U;
    case AudioCue::impact_low: return 2U;
    case AudioCue::player_hurt: return 3U;
    case AudioCue::landing: return 4U;
    case AudioCue::enemy_defeat: return 5U;
    case AudioCue::warning_blink: return kWarningBlinkIndex;
    case AudioCue::warning_chain: return kWarningChainIndex;
    case AudioCue::warning_death: return kWarningDeathIndex;
    }
    return kInvalidIndex;
}

[[nodiscard]] constexpr std::size_t warning_index(AudioCue cue) noexcept {
    switch (cue) {
    case AudioCue::warning_blink: return 0U;
    case AudioCue::warning_chain: return 1U;
    case AudioCue::warning_death: return 2U;
    default: return 3U;
    }
}

[[nodiscard]] constexpr std::uint8_t cue_limit(
    std::size_t category) noexcept {
    constexpr std::array<std::uint8_t, 9> kLimits{{
        1U, 3U, 1U, 1U, 1U, 2U, 1U, 1U, 1U,
    }};
    return category < kLimits.size() ? kLimits[category] : 0U;
}

}  // namespace

AudioPlan route_audio_plan(const combat::CombatEvent& event) noexcept {
    switch (event.kind) {
    case combat::CombatEventKind::swing:
        switch (event.attack) {
        case combat::AttackId::j1:
        case combat::AttackId::j2:
        case combat::AttackId::air_j:
            return plan_for(AudioCue::swing_light);
        case combat::AttackId::j3:
            return plan_for(AudioCue::swing_finisher);
        case combat::AttackId::launcher:
            return plan_for(AudioCue::swing_launcher);
        case combat::AttackId::none:
            return {};
        }
        return {};
    case combat::CombatEventKind::hit:
        return plan_for(AudioCue::impact);
    case combat::CombatEventKind::impact_summary:
        return event.feedback == combat::FeedbackLevel::heavy
            ? plan_for(AudioCue::impact_low)
            : AudioPlan{};
    case combat::CombatEventKind::player_hit:
        return plan_for(AudioCue::player_hurt);
    case combat::CombatEventKind::landing:
        return plan_for(AudioCue::landing);
    case combat::CombatEventKind::defeated:
        return plan_for(AudioCue::enemy_defeat);
    case combat::CombatEventKind::affix_blink_warning:
        return plan_for(AudioCue::warning_blink);
    case combat::CombatEventKind::affix_chain_warning:
        return plan_for(AudioCue::warning_chain);
    case combat::CombatEventKind::affix_death_warning:
        return plan_for(AudioCue::warning_death);
    default:
        return {};
    }
}

AudioAssetId AudioSelectionState::select(AudioCue cue) noexcept {
    switch (cue) {
    case AudioCue::swing_light: {
        const AudioAssetId selected = swing_light_variant_ == 0U
            ? AudioAssetId::swing_light_1
            : AudioAssetId::swing_light_2;
        swing_light_variant_ = static_cast<std::uint8_t>(
            (swing_light_variant_ + 1U) % 2U);
        return selected;
    }
    case AudioCue::swing_finisher: return AudioAssetId::swing_finisher;
    case AudioCue::swing_launcher: return AudioAssetId::swing_launcher;
    case AudioCue::impact: {
        constexpr std::array<AudioAssetId, 3> kImpactVariants{{
            AudioAssetId::impact_1, AudioAssetId::impact_2,
            AudioAssetId::impact_3,
        }};
        const AudioAssetId selected = kImpactVariants[impact_variant_];
        impact_variant_ = static_cast<std::uint8_t>(
            (impact_variant_ + 1U) % kImpactVariants.size());
        return selected;
    }
    case AudioCue::impact_low: return AudioAssetId::impact_low;
    case AudioCue::player_hurt: return AudioAssetId::player_hurt;
    case AudioCue::landing: return AudioAssetId::landing;
    case AudioCue::enemy_defeat: return AudioAssetId::enemy_defeat;
    case AudioCue::warning_blink: return AudioAssetId::warning_blink;
    case AudioCue::warning_chain: return AudioAssetId::warning_chain;
    case AudioCue::warning_death: return AudioAssetId::warning_death;
    }
    return AudioAssetId::count;
}

void AudioSelectionState::reset() noexcept {
    swing_light_variant_ = 0U;
    impact_variant_ = 0U;
}

bool AudioPlaybackBudget::allow(AudioCue cue, std::uint64_t tick) noexcept {
    const std::size_t category = cue_category_index(cue);
    if (category >= counts_.size()) {
        return false;
    }
    if (!has_counted_tick_ || tick != counted_tick_) {
        counts_.fill(0U);
        counted_tick_ = tick;
        has_counted_tick_ = true;
    }
    if (counts_[category] >= cue_limit(category)) {
        return false;
    }

    const std::size_t warning = warning_index(cue);
    if (warning < last_warning_ticks_.size()) {
        if (has_last_warning_tick_[warning] && tick >= last_warning_ticks_[warning]
            && tick - last_warning_ticks_[warning] < 12U) {
            return false;
        }
        last_warning_ticks_[warning] = tick;
        has_last_warning_tick_[warning] = true;
    }

    ++counts_[category];
    return true;
}

void AudioPlaybackBudget::reset() noexcept {
    counts_.fill(0U);
    last_warning_ticks_.fill(0U);
    has_last_warning_tick_.fill(false);
    counted_tick_ = 0U;
    has_counted_tick_ = false;
}

}  // namespace arpg::platform
