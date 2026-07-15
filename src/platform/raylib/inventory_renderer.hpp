#pragma once

#include "inventory_view_math.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace arpg::dungeon {
class DungeonSession;
struct DungeonSnapshot;
}

namespace arpg::platform {

class DungeonRuntime;
struct DungeonRenderStatus;

class InventoryRenderer final {
public:
    void open(const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot);
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool process_input(DungeonRuntime& runtime,
        const dungeon::DungeonSnapshot& snapshot);
    void draw(const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot,
        const DungeonRenderStatus& status);

private:
    void sync(const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot);
    void rebuild_filter(const items::ItemOwnershipState& state);
    void refresh_comparison(const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot);
    void prune_selection(const items::ItemOwnershipState& state) noexcept;
    [[nodiscard]] const items::ItemInstance* selected_item(
        const items::ItemOwnershipState& state) const noexcept;
    [[nodiscard]] bool recipe_ready(
        const items::ItemOwnershipState& state) const noexcept;

    bool open_{};
    InventoryFilter filter_{};
    float scroll_rows_{};
    std::uint64_t selected_item_id_{};
    RecipeSelection recipe_{};
    InventoryClickTracker click_tracker_{};
    std::vector<std::size_t> filtered_indices_{};
    std::uint64_t filtered_generation_{~std::uint64_t{0U}};
    items::EquipmentState filtered_equipment_{};
    std::uint64_t comparison_generation_{~std::uint64_t{0U}};
    std::uint64_t comparison_item_id_{};
    items::EquipmentState comparison_equipment_{};
    std::optional<combat::PlayerCombatBuild> current_build_{};
    std::optional<BuildDifference> difference_{};
};

}  // namespace arpg::platform
