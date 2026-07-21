#pragma once

#include "audio_pack.hpp"
#include "audio_routing.hpp"

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
    AudioPack pack_{};
    AudioSelectionState selection_{};
    AudioPlaybackBudget playback_budget_{};
    bool ready_{};
    bool owns_device_{};
};

}  // namespace arpg::platform
