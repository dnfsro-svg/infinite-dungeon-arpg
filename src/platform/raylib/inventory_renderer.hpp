#pragma once

#include "inventory_view_math.hpp"
#include "material_bag_renderer.hpp"
#include "material_pack.hpp"

#include <cstdint>
#include <optional>

namespace arpg::dungeon {
class DungeonSession;
struct DungeonSnapshot;
}

namespace arpg::platform {

class DungeonRuntime;
struct DungeonRenderStatus;
struct HostFrameInput;

class InventoryRenderer final {
public:
    void open(const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot);
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool process_input(DungeonRuntime& runtime,
        const dungeon::DungeonSnapshot& snapshot,
        const HostFrameInput& input);
    void draw(const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot,
        const DungeonRenderStatus& status,
        const MaterialPack& material_pack,
        Font hud_font, bool hud_font_ready);

private:
    void sync(const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot);
    void refresh_comparison(const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot);
    [[nodiscard]] const items::ItemInstance* selected_item(
        const items::ItemOwnershipState& state) const noexcept;
    [[nodiscard]] bool recipe_ready() const noexcept;

    bool open_{};
    InventoryPage page_{InventoryPage::equipment_materials};
    ActiveSkillLoadoutSelection active_skill_selection_{};
    InventoryFilter filter_{};
    float scroll_rows_{};
    std::uint64_t selected_item_id_{};
    RecipeSelection recipe_{};
    InventoryClickTracker click_tracker_{};
    InventoryViewCache view_cache_{};
    MaterialBagRenderer material_bag_{};
    std::uint64_t comparison_generation_{~std::uint64_t{0U}};
    std::uint64_t comparison_item_id_{};
    items::EquipmentState comparison_equipment_{};
    std::optional<combat::PlayerCombatBuild> current_build_{};
    std::optional<BuildDifference> difference_{};
};

}  // namespace arpg::platform
