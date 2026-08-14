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

struct InventoryFrameLayout final {
    int width{};
    int height{};
    InventoryShellLayout shell{};
    ActiveSkillLoadoutLayout skills{};
    MaterialBagLayout materials{};
    bool valid{};
};

struct InventoryTooltipFrame final {
    std::uint64_t item_id{};
    InventoryTooltipView view{};
    InventoryTooltipLayout layout{};
    bool valid{};
};

class InventoryRenderer final {
public:
    void open(const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot);
    void close() noexcept;
    void show_skill_stones_page() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool process_input(DungeonRuntime& runtime,
        const dungeon::DungeonSnapshot& snapshot,
        const HostFrameInput& input);
    void draw(const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot,
        const DungeonRenderStatus& status,
        const MaterialPack& material_pack,
        const char* inventory_binding_label,
        Font hud_font, bool hud_font_ready);

private:
    void refresh_frame_layout() noexcept;
    void sync(const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot);
    void refresh_comparison(const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot,
        std::uint64_t presentation_item_id);
    [[nodiscard]] InventoryTooltipFrame build_tooltip_frame(
        const dungeon::DungeonSession& session,
        const dungeon::DungeonSnapshot& snapshot,
        std::uint64_t presentation_item_id,
        Rectangle source, bool source_is_in_right_panel,
        Font measure_font, bool measure_font_ready);
    [[nodiscard]] bool recipe_ready() const noexcept;
    void clear_material_rejection() noexcept;
    void show_material_rejection(std::uint64_t item_id) noexcept;

    bool open_{};
    InventoryPage page_{InventoryPage::equipment};
    ActiveSkillLoadoutSelection active_skill_selection_{};
    InventoryFilter filter_{};
    float scroll_rows_{};
    std::uint64_t selected_item_id_{};
    RecipeSelection recipe_{};
    InventoryClickTracker click_tracker_{};
    InventoryViewCache view_cache_{};
    MaterialBagRenderer material_bag_{};
    InventoryFrameLayout frame_layout_{};
    InventoryTooltipInteractionState tooltip_state_{};
    InventoryTransientStatusState transient_status_state_{};
    std::uint64_t rejected_material_item_id_{};
    std::uint16_t material_rejection_frames_{};
    Font tooltip_measure_font_{};
    bool tooltip_measure_font_ready_{};
    std::uint64_t comparison_generation_{~std::uint64_t{0U}};
    std::uint64_t comparison_item_id_{};
    items::EquipmentState comparison_equipment_{};
    std::optional<combat::PlayerCombatBuild> current_build_{};
    std::optional<BuildDifference> difference_{};
};

}  // namespace arpg::platform
