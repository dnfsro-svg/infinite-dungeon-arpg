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
};

using AudioCueMask = std::uint8_t;

[[nodiscard]] constexpr AudioCueMask audio_cue_mask(AudioCue cue) noexcept {
    return static_cast<AudioCueMask>(cue);
}

[[nodiscard]] AudioCueMask route_audio_cues(
    const combat::CombatEvent& event) noexcept;

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

    std::array<std::int16_t, kWeaponSampleCount> weapon_samples_{};
    std::array<std::int16_t, kMaterialSampleCount> material_samples_{};
    std::array<std::int16_t, kLowSampleCount> low_samples_{};
    Sound weapon_{};
    std::array<Sound, 3> material_{};
    Sound low_{};
    std::size_t material_voice_{};
    bool ready_{};
    bool owns_device_{};
};

}  // namespace arpg::platform
