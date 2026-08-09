#pragma once

#include "combat_view_math.hpp"
#include "dungeon_runtime.hpp"
#include "ground_loot_view.hpp"
#include "material_loot_view.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

struct LootSuctionPoint final {
    float x{};
    float y{};
};

struct LootSuctionFlight final {
    combat::Vec3 world_position{};
    LootSuctionPoint center{};
    MaterialSpriteId sprite{MaterialSpriteId::missing};
    Rgba8 color{255U, 255U, 255U, 255U};
    float elapsed_seconds{};
    bool active{};
    bool equipment{};
};

struct LootSuctionPlan final {
    std::array<LootSuctionFlight, 12U> flights{};
    LootSuctionPoint destination{};
    float destination_pulse{};
    std::size_t count{};
};

class LootSuctionState final {
public:
    void observe(const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& status) noexcept;
    void update(float frame_seconds, bool paused) noexcept;
    [[nodiscard]] LootSuctionPlan build_plan(CombatCameraView camera,
        combat::Vec3 player_position, float width, float height) const noexcept;
    void clear() noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;

private:
    std::array<LootSuctionFlight, 12U> flights_{};
    std::uint64_t equipment_generation_{};
    std::uint64_t equipment_item_id_{};
    std::uint64_t material_generation_{};
    float destination_pulse_seconds_{};
    bool attached_{};
};

}  // namespace arpg::platform
