#include "test_framework.hpp"

#include "audio_asset_validation.hpp"
#include "audio_manifest.hpp"

#include <array>
#include <cstddef>

namespace {

using arpg::platform::AudioAssetId;
using arpg::platform::AudioDecodedMetadata;
using arpg::platform::AudioManifestDefinition;
using arpg::platform::AudioManifestEntry;
using arpg::platform::AudioValidationError;

constexpr AudioDecodedMetadata kValidMetadata{44100U, 44100U, 16U, 1U, 0.5F};

arpg::test::Failure audio_manifest_accepts_all_fourteen_unique_ids() noexcept {
    const AudioManifestDefinition manifest = arpg::platform::default_audio_manifest();
    ARPG_REQUIRE(manifest.entry_count == 14U);
    ARPG_REQUIRE(arpg::platform::validate_audio_manifest(manifest).valid);
    return {};
}

arpg::test::Failure audio_manifest_rejects_duplicate_ids() noexcept {
    std::array<AudioManifestEntry, 14> entries{};
    const AudioManifestDefinition default_manifest =
        arpg::platform::default_audio_manifest();
    for (std::size_t index = 0U; index < entries.size(); ++index) {
        entries[index] = default_manifest.entries[index];
    }
    entries.back().id = entries.front().id;
    const AudioManifestDefinition manifest{entries.data(), entries.size(), nullptr};
    const auto result = arpg::platform::validate_audio_manifest(manifest);
    ARPG_REQUIRE(!result.valid);
    ARPG_REQUIRE(result.error == AudioValidationError::duplicate_id);
    return {};
}

arpg::test::Failure audio_manifest_rejects_absolute_paths() noexcept {
    const std::array<AudioManifestEntry, 14> entries{{
        {AudioAssetId::swing_light_1, "C:/assets/stage14/audio/swing-light-1.wav"},
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
    }};
    const auto result = arpg::platform::validate_audio_manifest(
        {entries.data(), entries.size(), nullptr});
    ARPG_REQUIRE(!result.valid);
    ARPG_REQUIRE(result.error == AudioValidationError::invalid_path);
    return {};
}

arpg::test::Failure audio_manifest_rejects_path_escape_and_empty_segments() noexcept {
    const AudioManifestDefinition default_manifest =
        arpg::platform::default_audio_manifest();
    std::array<AudioManifestEntry, 14> entries{};
    for (std::size_t index = 0U; index < entries.size(); ++index) {
        entries[index] = default_manifest.entries[index];
    }
    entries.front().path = "assets/stage14/audio/../warning-death.wav";
    ARPG_REQUIRE(!arpg::platform::validate_audio_manifest(
        {entries.data(), entries.size(), nullptr}).valid);
    entries.front().path = "assets/stage14/audio\\warning-death.wav";
    ARPG_REQUIRE(!arpg::platform::validate_audio_manifest(
        {entries.data(), entries.size(), nullptr}).valid);
    entries.front().path = "assets/stage14/audio//warning-death.wav";
    ARPG_REQUIRE(!arpg::platform::validate_audio_manifest(
        {entries.data(), entries.size(), nullptr}).valid);
    entries.front().path = "a";
    ARPG_REQUIRE(!arpg::platform::validate_audio_manifest(
        {entries.data(), entries.size(), nullptr}).valid);
    entries.front().path = "/assets/stage14/audio/warning-death.wav";
    ARPG_REQUIRE(!arpg::platform::validate_audio_manifest(
        {entries.data(), entries.size(), nullptr}).valid);
    entries.front().path = "assets/stage14/audio/./warning-death.wav";
    ARPG_REQUIRE(!arpg::platform::validate_audio_manifest(
        {entries.data(), entries.size(), nullptr}).valid);
    return {};
}

arpg::test::Failure audio_metadata_rejects_wrong_pcm_format() noexcept {
    const AudioManifestEntry entry{AudioAssetId::swing_light_1,
        "assets/stage14/audio/swing-light-1.wav"};
    ARPG_REQUIRE(!arpg::platform::validate_audio_metadata(
        entry, {44100U, 48000U, 16U, 1U, 0.5F}).valid);
    ARPG_REQUIRE(!arpg::platform::validate_audio_metadata(
        entry, {44100U, 44100U, 8U, 1U, 0.5F}).valid);
    ARPG_REQUIRE(!arpg::platform::validate_audio_metadata(
        entry, {44100U, 44100U, 16U, 2U, 0.5F}).valid);
    return {};
}

arpg::test::Failure audio_metadata_rejects_zero_or_overlong_duration() noexcept {
    const AudioManifestEntry entry{AudioAssetId::swing_light_1,
        "assets/stage14/audio/swing-light-1.wav"};
    ARPG_REQUIRE(!arpg::platform::validate_audio_metadata(
        entry, {0U, 44100U, 16U, 1U, 0.5F}).valid);
    ARPG_REQUIRE(!arpg::platform::validate_audio_metadata(
        entry, {66151U, 44100U, 16U, 1U, 0.5F}).valid);
    return {};
}

arpg::test::Failure audio_metadata_rejects_silence_and_clipping() noexcept {
    const AudioManifestEntry entry{AudioAssetId::swing_light_1,
        "assets/stage14/audio/swing-light-1.wav"};
    ARPG_REQUIRE(!arpg::platform::validate_audio_metadata(
        entry, {44100U, 44100U, 16U, 1U, 0.0F}).valid);
    ARPG_REQUIRE(!arpg::platform::validate_audio_metadata(
        entry, {44100U, 44100U, 16U, 1U, 1.0F}).valid);
    return {};
}

arpg::test::Failure audio_manifest_rejects_total_pcm_over_eight_mib() noexcept {
    std::array<AudioDecodedMetadata, 63> within_budget{};
    for (AudioDecodedMetadata& value : within_budget) {
        value = {66150U, 44100U, 16U, 1U, 0.5F};
    }
    ARPG_REQUIRE(arpg::platform::validate_audio_pcm_budget(
        within_budget.data(), within_budget.size()).valid);

    std::array<AudioDecodedMetadata, 64> metadata{};
    for (AudioDecodedMetadata& value : metadata) {
        value = {66150U, 44100U, 16U, 1U, 0.5F};
    }
    const auto result = arpg::platform::validate_audio_pcm_budget(
        metadata.data(), metadata.size());
    ARPG_REQUIRE(!result.valid);
    ARPG_REQUIRE(result.error == AudioValidationError::pcm_budget_exceeded);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"accepts all fourteen unique ids", &audio_manifest_accepts_all_fourteen_unique_ids},
    {"rejects duplicate ids", &audio_manifest_rejects_duplicate_ids},
    {"rejects absolute paths", &audio_manifest_rejects_absolute_paths},
    {"rejects path escape and empty segments",
        &audio_manifest_rejects_path_escape_and_empty_segments},
    {"rejects wrong PCM format", &audio_metadata_rejects_wrong_pcm_format},
    {"rejects zero or overlong duration",
        &audio_metadata_rejects_zero_or_overlong_duration},
    {"rejects silence and clipping", &audio_metadata_rejects_silence_and_clipping},
    {"rejects total PCM over eight MiB",
        &audio_manifest_rejects_total_pcm_over_eight_mib},
};

}  // namespace

arpg::test::TestSuite audio_asset_validation_suite() noexcept {
    return arpg::test::make_suite("audio_asset_validation", kCases);
}
