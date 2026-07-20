#pragma once

#include "audio_routing.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

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
    AudioSelectionState selection_{};
    AudioPlaybackBudget playback_budget_{};
    bool ready_{};
    bool owns_device_{};
};

}  // namespace arpg::platform
