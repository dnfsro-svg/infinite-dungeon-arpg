#pragma once

#include "audio_asset_types.hpp"

#include <cstddef>

namespace arpg::platform {

struct AudioManifestEntry final {
    AudioAssetId id{AudioAssetId::swing_light_1};
    const char* path{};
};

struct AudioManifestDefinition final {
    const AudioManifestEntry* entries{};
    std::size_t entry_count{};
    const AudioDecodedMetadata* decoded_metadata{};
};

namespace detail {

inline constexpr AudioManifestEntry kDefaultAudioEntries[] = {
    {AudioAssetId::swing_light_1, "assets/stage14/audio/swing-light-1.wav"},
    {AudioAssetId::swing_light_2, "assets/stage14/audio/swing-light-2.wav"},
    {AudioAssetId::swing_finisher, "assets/stage14/audio/swing-finisher.wav"},
    {AudioAssetId::swing_launcher, "assets/stage14/audio/swing-launcher.wav"},
    {AudioAssetId::impact_1, "assets/stage14/audio/impact-1.wav"},
    {AudioAssetId::impact_2, "assets/stage14/audio/impact-2.wav"},
    {AudioAssetId::impact_3, "assets/stage14/audio/impact-3.wav"},
    {AudioAssetId::impact_low, "assets/stage14/audio/impact-low.wav"},
    {AudioAssetId::player_hurt, "assets/stage14/audio/player-hurt.wav"},
    {AudioAssetId::landing, "assets/stage14/audio/landing.wav"},
    {AudioAssetId::enemy_defeat, "assets/stage14/audio/enemy-defeat.wav"},
    {AudioAssetId::warning_blink, "assets/stage14/audio/warning-blink.wav"},
    {AudioAssetId::warning_chain, "assets/stage14/audio/warning-chain.wav"},
    {AudioAssetId::warning_death, "assets/stage14/audio/warning-death.wav"},
};

}  // namespace detail

[[nodiscard]] constexpr AudioManifestDefinition default_audio_manifest() noexcept {
    return {detail::kDefaultAudioEntries,
        sizeof(detail::kDefaultAudioEntries) / sizeof(detail::kDefaultAudioEntries[0]),
        nullptr};
}

}  // namespace arpg::platform
