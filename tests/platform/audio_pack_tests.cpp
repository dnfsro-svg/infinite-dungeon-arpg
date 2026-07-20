#include "test_framework.hpp"

#include "audio_pack.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using arpg::platform::AudioAssetId;
using arpg::platform::AudioPack;
using arpg::platform::AudioSoundApi;

constexpr std::size_t kAudioAssetCount =
    static_cast<std::size_t>(AudioAssetId::count);
constexpr std::size_t kExternalFrameCount = 64U;
constexpr std::size_t kRecordCapacity = 64U;

enum class FakeWaveMode : std::uint8_t {
    valid,
    missing,
    invalid_format,
};

struct FakeAudio final {
    std::array<FakeWaveMode, kAudioAssetCount> wave_modes{};
    std::array<std::array<std::int16_t, kExternalFrameCount>,
        kAudioAssetCount> external_samples{};
    std::size_t fail_external_sound_index{kAudioAssetCount + 1U};
    bool fail_fallback_sound{};
    std::size_t load_wave_count{};
    std::size_t load_sound_count{};
    std::size_t unload_wave_count{};
    std::size_t unload_sound_count{};
    std::size_t created_sound_count{};
    std::size_t play_count{};
    std::size_t stop_count{};
    unsigned int next_sound_handle{1U};
    std::array<const void*, kRecordCapacity> unloaded_wave_data{};
    std::array<unsigned int, kRecordCapacity> unloaded_sound_handles{};
    std::array<unsigned int, kRecordCapacity> created_sound_handles{};

    FakeAudio() noexcept {
        wave_modes.fill(FakeWaveMode::valid);
        for (auto& samples : external_samples) {
            samples.fill(2048);
        }
    }
};

FakeAudio* g_fake_audio{};

struct FakeAudioScope final {
    explicit FakeAudioScope(FakeAudio& fake) noexcept {
        g_fake_audio = &fake;
    }
    ~FakeAudioScope() noexcept {
        g_fake_audio = nullptr;
    }
};

[[nodiscard]] std::size_t external_wave_index(Wave wave) noexcept {
    if (g_fake_audio == nullptr) return kAudioAssetCount;
    for (std::size_t index{}; index < kAudioAssetCount; ++index) {
        if (wave.data == g_fake_audio->external_samples[index].data()) {
            return index;
        }
    }
    return kAudioAssetCount;
}

Wave fake_load_wave(const char*) noexcept {
    if (g_fake_audio == nullptr
        || g_fake_audio->load_wave_count >= kAudioAssetCount) {
        return {};
    }
    const std::size_t index = g_fake_audio->load_wave_count++;
    if (g_fake_audio->wave_modes[index] == FakeWaveMode::missing) {
        return {};
    }
    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(kExternalFrameCount);
    wave.sampleRate = 44100U;
    wave.sampleSize = g_fake_audio->wave_modes[index]
            == FakeWaveMode::invalid_format
        ? 8U
        : 16U;
    wave.channels = 1U;
    wave.data = g_fake_audio->external_samples[index].data();
    return wave;
}

bool fake_wave_valid(Wave wave) noexcept {
    return wave.data != nullptr && wave.frameCount != 0U;
}

Sound fake_load_sound_from_wave(Wave wave) noexcept {
    if (g_fake_audio == nullptr) return {};
    ++g_fake_audio->load_sound_count;
    const std::size_t external_index = external_wave_index(wave);
    if (external_index == g_fake_audio->fail_external_sound_index
        || (external_index == kAudioAssetCount
            && g_fake_audio->fail_fallback_sound)) {
        return {};
    }
    Sound sound{};
    sound.frameCount = g_fake_audio->next_sound_handle++;
    if (g_fake_audio->created_sound_count
        < g_fake_audio->created_sound_handles.size()) {
        g_fake_audio->created_sound_handles[
            g_fake_audio->created_sound_count++] = sound.frameCount;
    }
    return sound;
}

bool fake_sound_valid(Sound sound) noexcept {
    return sound.frameCount != 0U;
}

void fake_unload_wave(Wave wave) noexcept {
    if (g_fake_audio == nullptr
        || g_fake_audio->unload_wave_count
            >= g_fake_audio->unloaded_wave_data.size()) return;
    g_fake_audio->unloaded_wave_data[
        g_fake_audio->unload_wave_count++] = wave.data;
}

void fake_unload_sound(Sound sound) noexcept {
    if (g_fake_audio == nullptr
        || g_fake_audio->unload_sound_count
            >= g_fake_audio->unloaded_sound_handles.size()) return;
    g_fake_audio->unloaded_sound_handles[
        g_fake_audio->unload_sound_count++] = sound.frameCount;
}

void fake_play_sound(Sound sound) noexcept {
    if (g_fake_audio != nullptr && fake_sound_valid(sound)) {
        ++g_fake_audio->play_count;
    }
}

void fake_stop_sound(Sound sound) noexcept {
    if (g_fake_audio != nullptr && fake_sound_valid(sound)) {
        ++g_fake_audio->stop_count;
    }
}

[[nodiscard]] AudioSoundApi fake_audio_api() noexcept {
    return {&fake_load_wave, &fake_wave_valid, &fake_load_sound_from_wave,
        &fake_sound_valid, &fake_unload_wave, &fake_unload_sound,
        &fake_play_sound, &fake_stop_sound};
}

[[nodiscard]] std::size_t wave_unload_count(
    const FakeAudio& fake, const void* data) noexcept {
    std::size_t count{};
    for (std::size_t index{}; index < fake.unload_wave_count; ++index) {
        if (fake.unloaded_wave_data[index] == data) ++count;
    }
    return count;
}

[[nodiscard]] std::size_t sound_unload_count(
    const FakeAudio& fake, unsigned int handle) noexcept {
    std::size_t count{};
    for (std::size_t index{}; index < fake.unload_sound_count; ++index) {
        if (fake.unloaded_sound_handles[index] == handle) ++count;
    }
    return count;
}

[[nodiscard]] bool external_waves_released_once_without_fallback(
    const FakeAudio& fake) noexcept {
    if (wave_unload_count(fake, nullptr) != 0U) return false;
    for (std::size_t record{}; record < fake.unload_wave_count; ++record) {
        bool external{};
        for (std::size_t index{}; index < fake.load_wave_count; ++index) {
            if (fake.unloaded_wave_data[record]
                == fake.external_samples[index].data()) {
                external = true;
                break;
            }
        }
        if (!external) return false;
    }
    for (std::size_t index{}; index < fake.load_wave_count; ++index) {
        const std::size_t expected = fake.wave_modes[index] == FakeWaveMode::missing
            ? 0U
            : 1U;
        if (wave_unload_count(fake, fake.external_samples[index].data())
            != expected) return false;
    }
    return true;
}

[[nodiscard]] bool created_sounds_released_once(
    const FakeAudio& fake) noexcept {
    if (sound_unload_count(fake, 0U) != 0U
        || fake.unload_sound_count != fake.created_sound_count) {
        return false;
    }
    for (std::size_t index{}; index < fake.created_sound_count; ++index) {
        if (sound_unload_count(fake, fake.created_sound_handles[index]) != 1U) {
            return false;
        }
    }
    return true;
}

arpg::test::Failure audio_pack_loads_all_fourteen_external_assets() noexcept {
    FakeAudio fake{};
    FakeAudioScope scope{fake};
    AudioPack pack{fake_audio_api()};

    ARPG_REQUIRE(pack.load());
    ARPG_REQUIRE(fake.load_wave_count == kAudioAssetCount);
    ARPG_REQUIRE(fake.load_sound_count == kAudioAssetCount);
    ARPG_REQUIRE(fake.unload_wave_count == kAudioAssetCount);
    for (std::size_t index{}; index < kAudioAssetCount; ++index) {
        const auto id = static_cast<AudioAssetId>(index);
        ARPG_REQUIRE(pack.available(id));
        ARPG_REQUIRE(!pack.using_fallback(id));
        pack.play(id);
    }
    pack.stop_all();
    ARPG_REQUIRE(fake.play_count == kAudioAssetCount);
    ARPG_REQUIRE(fake.stop_count == kAudioAssetCount);
    pack.unload();
    ARPG_REQUIRE(external_waves_released_once_without_fallback(fake));
    ARPG_REQUIRE(created_sounds_released_once(fake));
    return {};
}

arpg::test::Failure audio_pack_falls_back_only_for_one_missing_asset() noexcept {
    FakeAudio fake{};
    fake.wave_modes[4] = FakeWaveMode::missing;
    FakeAudioScope scope{fake};
    AudioPack pack{fake_audio_api()};

    ARPG_REQUIRE(pack.load());
    ARPG_REQUIRE(pack.using_fallback(AudioAssetId::impact_1));
    ARPG_REQUIRE(!pack.using_fallback(AudioAssetId::impact_2));
    ARPG_REQUIRE(fake.load_sound_count == kAudioAssetCount);
    ARPG_REQUIRE(fake.unload_wave_count == kAudioAssetCount - 1U);
    pack.unload();
    ARPG_REQUIRE(external_waves_released_once_without_fallback(fake));
    ARPG_REQUIRE(created_sounds_released_once(fake));
    return {};
}

arpg::test::Failure audio_pack_falls_back_only_for_one_bad_format() noexcept {
    FakeAudio fake{};
    fake.wave_modes[9] = FakeWaveMode::invalid_format;
    FakeAudioScope scope{fake};
    AudioPack pack{fake_audio_api()};

    ARPG_REQUIRE(pack.load());
    ARPG_REQUIRE(pack.using_fallback(AudioAssetId::landing));
    ARPG_REQUIRE(!pack.using_fallback(AudioAssetId::enemy_defeat));
    ARPG_REQUIRE(fake.load_sound_count == kAudioAssetCount);
    ARPG_REQUIRE(fake.unload_wave_count == kAudioAssetCount);
    pack.unload();
    ARPG_REQUIRE(external_waves_released_once_without_fallback(fake));
    ARPG_REQUIRE(created_sounds_released_once(fake));
    return {};
}

arpg::test::Failure audio_pack_falls_back_after_external_sound_creation_fails() noexcept {
    FakeAudio fake{};
    fake.fail_external_sound_index = 12U;
    FakeAudioScope scope{fake};
    AudioPack pack{fake_audio_api()};

    ARPG_REQUIRE(pack.load());
    ARPG_REQUIRE(pack.available(AudioAssetId::warning_chain));
    ARPG_REQUIRE(pack.using_fallback(AudioAssetId::warning_chain));
    ARPG_REQUIRE(!pack.using_fallback(AudioAssetId::warning_blink));
    ARPG_REQUIRE(fake.load_sound_count == kAudioAssetCount + 1U);
    ARPG_REQUIRE(fake.unload_wave_count == kAudioAssetCount);
    pack.unload();
    ARPG_REQUIRE(external_waves_released_once_without_fallback(fake));
    ARPG_REQUIRE(created_sounds_released_once(fake));
    return {};
}

arpg::test::Failure audio_pack_is_ready_when_all_external_assets_are_missing() noexcept {
    FakeAudio fake{};
    fake.wave_modes.fill(FakeWaveMode::missing);
    FakeAudioScope scope{fake};
    AudioPack pack{fake_audio_api()};

    ARPG_REQUIRE(pack.load());
    ARPG_REQUIRE(fake.unload_wave_count == 0U);
    ARPG_REQUIRE(fake.load_sound_count == kAudioAssetCount);
    for (std::size_t index{}; index < kAudioAssetCount; ++index) {
        const auto id = static_cast<AudioAssetId>(index);
        ARPG_REQUIRE(pack.available(id));
        ARPG_REQUIRE(pack.using_fallback(id));
    }
    pack.unload();
    ARPG_REQUIRE(external_waves_released_once_without_fallback(fake));
    ARPG_REQUIRE(created_sounds_released_once(fake));
    return {};
}

arpg::test::Failure audio_pack_fails_safely_with_incomplete_api() noexcept {
    FakeAudio fake{};
    FakeAudioScope scope{fake};
    AudioSoundApi incomplete = fake_audio_api();
    incomplete.load_wave = nullptr;
    AudioPack pack{incomplete};

    ARPG_REQUIRE(!pack.load());
    pack.play(AudioAssetId::swing_light_1);
    pack.stop_all();
    pack.unload();
    ARPG_REQUIRE(fake.load_wave_count == 0U);
    ARPG_REQUIRE(fake.load_sound_count == 0U);
    ARPG_REQUIRE(fake.unload_sound_count == 0U);
    return {};
}

arpg::test::Failure audio_pack_rolls_back_when_fallback_sound_fails() noexcept {
    FakeAudio fake{};
    fake.wave_modes[2] = FakeWaveMode::missing;
    fake.fail_fallback_sound = true;
    FakeAudioScope scope{fake};
    AudioPack pack{fake_audio_api()};

    ARPG_REQUIRE(!pack.load());
    for (std::size_t index{}; index < kAudioAssetCount; ++index) {
        const auto id = static_cast<AudioAssetId>(index);
        ARPG_REQUIRE(!pack.available(id));
        ARPG_REQUIRE(!pack.using_fallback(id));
    }
    ARPG_REQUIRE(external_waves_released_once_without_fallback(fake));
    ARPG_REQUIRE(created_sounds_released_once(fake));
    const std::size_t unloaded_before_repeat = fake.unload_sound_count;
    pack.unload();
    pack.unload();
    ARPG_REQUIRE(fake.unload_sound_count == unloaded_before_repeat);
    return {};
}

arpg::test::Failure audio_pack_repeated_unload_does_not_release_twice() noexcept {
    FakeAudio fake{};
    FakeAudioScope scope{fake};
    AudioPack pack{fake_audio_api()};

    ARPG_REQUIRE(pack.load());
    pack.unload();
    pack.unload();
    ARPG_REQUIRE(fake.unload_sound_count == kAudioAssetCount);
    ARPG_REQUIRE(external_waves_released_once_without_fallback(fake));
    ARPG_REQUIRE(created_sounds_released_once(fake));
    ARPG_REQUIRE(!pack.available(AudioAssetId::swing_light_1));
    ARPG_REQUIRE(!pack.using_fallback(AudioAssetId::swing_light_1));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"loads all fourteen external assets",
        &audio_pack_loads_all_fourteen_external_assets},
    {"one missing asset uses one fallback",
        &audio_pack_falls_back_only_for_one_missing_asset},
    {"one bad format uses one fallback",
        &audio_pack_falls_back_only_for_one_bad_format},
    {"external sound failure uses fallback",
        &audio_pack_falls_back_after_external_sound_creation_fails},
    {"all missing assets still ready",
        &audio_pack_is_ready_when_all_external_assets_are_missing},
    {"incomplete api fails safely",
        &audio_pack_fails_safely_with_incomplete_api},
    {"fallback failure rolls back valid resources",
        &audio_pack_rolls_back_when_fallback_sound_fails},
    {"repeated unload is idempotent",
        &audio_pack_repeated_unload_does_not_release_twice},
};

}  // namespace

arpg::test::TestSuite audio_pack_suite() noexcept {
    return arpg::test::make_suite("audio_pack", kCases);
}
