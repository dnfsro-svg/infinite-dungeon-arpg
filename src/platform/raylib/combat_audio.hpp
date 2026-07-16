#pragma once

#include "combat/combat_types.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

enum class AudioCue : std::uint8_t {
    weapon = 1U << 0U,
    material = 1U << 1U,
    low = 1U << 2U,
    blink_warning = 1U << 3U,
    chain_warning = 1U << 4U,
    death_warning = 1U << 5U,
};

using AudioCueMask = std::uint8_t;

[[nodiscard]] constexpr AudioCueMask audio_cue_mask(AudioCue cue) noexcept {
    return static_cast<AudioCueMask>(cue);
}

[[nodiscard]] AudioCueMask route_audio_cues(
    const combat::CombatEvent& event) noexcept;

class WarningAudioThrottle final {
public:
    [[nodiscard]] bool allow(AudioCue cue, std::uint64_t tick) noexcept;

private:
    std::array<std::uint64_t, 3> last_ticks_{};
    std::array<bool, 3> has_last_tick_{};
};

class CombatAudio final {
public:
    CombatAudio() noexcept = default;
    ~CombatAudio() noexcept;

    CombatAudio(const CombatAudio&) = delete;
    CombatAudio& operator=(const CombatAudio&) = delete;
    CombatAudio(CombatAudio&&) = delete;
    CombatAudio& operator=(CombatAudio&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void consume_event(const combat::CombatEvent& event) noexcept;
    void stop_all() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool ready() const noexcept;

private:
    static constexpr unsigned int kSampleRate = 22050;
    static constexpr std::size_t kWeaponSampleCount = 1764;
    static constexpr std::size_t kMaterialSampleCount = 1323;
    static constexpr std::size_t kLowSampleCount = 2646;
    static constexpr std::size_t kBlinkWarningSampleCount = 882;
    static constexpr std::size_t kChainWarningSampleCount = 1102;
    static constexpr std::size_t kDeathWarningSampleCount = 1764;

    std::array<std::int16_t, kWeaponSampleCount> weapon_samples_{};
    std::array<std::int16_t, kMaterialSampleCount> material_samples_{};
    std::array<std::int16_t, kLowSampleCount> low_samples_{};
    std::array<std::int16_t, kBlinkWarningSampleCount> blink_warning_samples_{};
    std::array<std::int16_t, kChainWarningSampleCount> chain_warning_samples_{};
    std::array<std::int16_t, kDeathWarningSampleCount> death_warning_samples_{};
    Sound weapon_{};
    std::array<Sound, 3> material_{};
    Sound low_{};
    Sound blink_warning_{};
    Sound chain_warning_{};
    Sound death_warning_{};
    std::size_t material_voice_{};
    WarningAudioThrottle warning_throttle_{};
    bool ready_{};
    bool owns_device_{};
};

}  // namespace arpg::platform
