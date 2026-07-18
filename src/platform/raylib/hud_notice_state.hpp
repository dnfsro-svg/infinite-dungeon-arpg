#pragma once

#include "hud_view_model.hpp"

#include <array>
#include <cstdint>

namespace arpg::platform {

enum class HudNoticeKind : std::uint8_t {
    none, save_error, recovery_required, abyss_abandon,
    hole_interact, exit_ready, room_clear, reward, level_up,
    passive_points, inventory, passive_tree
};

struct HudNotice final {
    HudNoticeKind kind{HudNoticeKind::none};
    std::uint8_t priority{};
    float seconds_left{};
    HudText96 text{};
};

struct HudNoticeView final {
    HudNotice primary{};
    HudNotice secondary{};
};

class HudNoticeState final {
public:
    void observe(const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& status,
        const ControlHints& hints,
        bool recovery_required) noexcept;
    void update(float frame_seconds, bool paused) noexcept;
    void clear_room_context() noexcept;
    [[nodiscard]] HudNoticeView view() const noexcept;
    [[nodiscard]] std::uint32_t dropped_count() const noexcept;

private:
    std::array<HudNotice, 4> notices_{};
    std::uint32_t dropped_count_{};
    std::uint64_t last_commit_generation_{};
    std::uint64_t last_room_index_{};
    std::uint64_t last_room_experience_{};
    std::uint8_t last_level_{};
    std::uint64_t last_room_clear_room_index_{};
    bool has_observation_{};
    bool has_room_clear_observation_{};
};

}  // namespace arpg::platform
