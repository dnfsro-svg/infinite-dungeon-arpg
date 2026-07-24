#pragma once

#include "material_animation.hpp"

#include "combat/combat_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::platform {

struct MonsterMaterialDrawPlan final {
    bool visible{};
    bool use_material_frame{};
    MonsterAnimationState animation_state{MonsterAnimationState::idle};
    std::uint16_t frame_index{};
    std::optional<MonsterAnimationFrame> frame{};
};

class MonsterMaterialPresenter final {
public:
    [[nodiscard]] MonsterMaterialDrawPlan collect_draw_plan(
        std::size_t slot_index, const combat::MonsterSnapshot& monster,
        std::uint64_t world_tick, bool hurt) noexcept;
    void reset() noexcept;

private:
    struct SlotState final {
        bool occupied{};
        std::uint16_t generation{};
        combat::MonsterId monster{combat::MonsterId::count};
        MonsterAnimationState state{MonsterAnimationState::idle};
        std::uint64_t state_started_tick{};
    };

    std::array<SlotState, combat::kMonsterCapacity> slots_{};
};

}  // namespace arpg::platform
