#pragma once

#include "combat_feedback.hpp"
#include "dungeon_runtime.hpp"
#include "hud_view_model.hpp"

#include <cstdint>

namespace arpg::platform {

class DebugOverlayRenderer final {
public:
    void draw(const dungeon::DungeonSnapshot&, const DungeonRenderStatus&,
        const CombatFeedback&, bool audio_ready,
        const HudBuildDiagnostics&, std::uint32_t notice_drops,
        std::uint64_t binding_revision) const noexcept;
};

}  // namespace arpg::platform
