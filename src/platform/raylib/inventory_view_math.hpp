#pragma once

#include "combat/combat_types.hpp"
#include "items/item_catalog.hpp"
#include "skills/active_skill_types.hpp"
#include "ui_text_layout.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace arpg::platform {

struct InventoryLayout final {
    Rectangle equipment{};
    Rectangle grid{};
    Rectangle detail{};
    float scale{1.0F};
};

enum class InventoryPage : std::uint8_t {
    equipment,
    equipment_materials = equipment,
    skill_stones,
    materials,
    count,
};

inline constexpr std::size_t kInventoryPageCount = 3U;
inline constexpr std::size_t kInventoryTooltipLineCapacity = 32U;
inline constexpr std::size_t kInventoryTooltipSegmentCapacity = 8U;
inline constexpr std::size_t kInventoryTooltipPlacementCapacity =
    kInventoryTooltipLineCapacity * kInventoryTooltipSegmentCapacity;
inline constexpr std::uint16_t kInventoryTransientStatusFrameCount = 180U;

enum class InventoryEmptyState : std::uint8_t {
    none,
    inventory_empty,
    all_equipped,
    filter_empty,
};

enum class InventoryActionKind : std::uint8_t {
    none,
    combine,
    remove_active_skill,
};

enum class InventorySavePresentationState : std::uint8_t {
    none,
    saving,
    saved,
    recovered,
    error,
};

enum class InventoryTransientStatusKind : std::uint8_t {
    none,
    saved,
    recovered,
    reinforcement_receipt,
};

struct InventoryTransientStatusInput final {
    std::uint64_t commit_generation{};
    InventorySavePresentationState save_state{
        InventorySavePresentationState::none};
    bool receipt_valid{};
    bool receipt_success{};
    bool receipt_destroyed{};
    bool receipt_coupon{};
    std::uint64_t receipt_commit_generation{};
    std::uint32_t receipt_before{};
    std::uint32_t receipt_after{};
};

struct InventoryTransientStatusState final {
    bool initialized{};
    std::uint64_t last_commit_generation{};
    std::uint64_t last_receipt_generation{};
    InventoryTransientStatusKind kind{InventoryTransientStatusKind::none};
    std::uint16_t remaining_frames{};
    bool receipt_success{};
    bool receipt_destroyed{};
    bool receipt_coupon{};
    std::uint32_t receipt_before{};
    std::uint32_t receipt_after{};
};

struct InventoryTransientStatusPresentation final {
    InventoryTransientStatusKind kind{InventoryTransientStatusKind::none};
    bool receipt_success{};
    bool receipt_destroyed{};
    bool receipt_coupon{};
    std::uint32_t receipt_before{};
    std::uint32_t receipt_after{};
};

struct InventoryTransientStatusFrameResult final {
    InventoryTransientStatusState next{};
    InventoryTransientStatusPresentation presentation{};
};

enum class InventoryStatusTone : std::uint8_t {
    normal,
    muted,
    positive,
    warning,
    negative,
};

enum class InventoryPointerLayer : std::uint8_t {
    none,
    confirmation,
    tab,
    tooltip,
    directed_material,
    material_cancel,
    material_select,
    material_target,
    page_action,
    filter,
    skill_content,
    recipe,
    equipment,
    background_clear,
};

struct InventoryShellLayout final {
    Rectangle viewport{};
    Rectangle header{};
    Rectangle content{};
    Rectangle left_panel{};
    Rectangle right_panel{};
    Rectangle filter_bar{};
    Rectangle slot_filter_bounds{};
    Rectangle rarity_filter_bounds{};
    Rectangle grid_viewport{};
    Rectangle empty_state_bounds{};
    Rectangle status_bar{};
    Rectangle status_bounds{};
    Rectangle action_bounds{};
    Rectangle hint_bounds{};
    Rectangle silhouette{};
    std::array<Rectangle, kInventoryPageCount> tabs{};
    std::array<Rectangle,
        static_cast<std::size_t>(items::ItemSlot::count)> equipment_slots{};
    float geometry_scale{};
    float grid_cell_height{};
    float grid_row_stride{};
    float body_font_size{};
    float hint_font_size{};
    bool hint_visible{};
    bool valid{};
};

struct InventoryEmptyPresentation final {
    InventoryEmptyState state{InventoryEmptyState::none};
    const char* title{};
    const char* hint{};
};

struct InventoryStatusInput final {
    InventoryPage page{InventoryPage::equipment};
    InventorySavePresentationState save_state{
        InventorySavePresentationState::none};
    bool recovery_required{};
    InventoryTransientStatusPresentation transient{};
    std::optional<items::MaterialId> selected_material{};
    std::size_t recipe_count{};
    bool recipe_ready{};
    bool reinforced_recipe_input{};
    bool active_skill_removable{};
    bool authority_enabled{};
};

struct InventoryStatusPresentation final {
    std::array<char, 128> status{};
    std::array<char, 96> hint{};
    std::array<char, 48> action{};
    InventoryStatusTone tone{InventoryStatusTone::normal};
    InventoryActionKind action_kind{InventoryActionKind::none};
    bool action_visible{};
    bool action_enabled{};
};

inline constexpr std::size_t kNoActiveSkillLoadoutSelection =
    ~std::size_t{0U};

struct ActiveSkillLoadoutSelection final {
    std::size_t selected_slot{kNoActiveSkillLoadoutSelection};
    skills::ActiveSkillId selected_inventory{skills::ActiveSkillId::none};
};

struct ActiveSkillLoadoutSlotView final {
    std::uint8_t slot_number{};
    skills::ActiveSkillId id{skills::ActiveSkillId::none};
    std::array<char, 48> name{};
    std::array<bool, skills::kSupportSlotsPerActive> support_empty{};
    bool empty{};
    bool selected{};
};

struct ActiveSkillInventoryStoneView final {
    skills::ActiveSkillId id{skills::ActiveSkillId::none};
    std::array<char, 48> name{};
    bool selected{};
};

struct ActiveSkillLoadoutView final {
    std::array<ActiveSkillLoadoutSlotView,
        skills::kActiveSkillSlotCount> slots{};
    std::array<ActiveSkillInventoryStoneView,
        skills::kActiveSkillCount> inventory{};
    std::size_t inventory_count{};
    bool save_pending{};
};

struct ActiveSkillLoadoutLayout final {
    Rectangle panel{};
    std::array<Rectangle, skills::kActiveSkillSlotCount> main_slots{};
    std::array<Rectangle, skills::kSupportSlotsPerActive> support_slots{};
    std::array<Rectangle, skills::kActiveSkillCount> inventory_slots{};
    Rectangle remove_button{};
    Rectangle equipment_page_button{};
    Rectangle skill_stones_page_button{};
    float scale{1.0F};
};

struct InventoryTextSafeLayout final {
    Rectangle page_title{};
    Rectangle equipment_panel_title{};
    Rectangle grid_panel_title{};
    Rectangle detail_panel_title{};
    Rectangle skill_panel_title{};
    Rectangle support_section_title{};
    Rectangle inventory_section_title{};
};

enum class ActiveSkillLoadoutActionKind : std::uint8_t {
    select,
    remove,
    equip,
    swap,
};

struct ActiveSkillLoadoutCommand final {
    ActiveSkillLoadoutActionKind kind{ActiveSkillLoadoutActionKind::select};
    std::uint8_t slot{0xFFU};
    std::uint8_t other_slot{0xFFU};
    skills::ActiveSkillId skill{skills::ActiveSkillId::none};
};

struct VisibleGridRange final {
    std::size_t first{};
    std::size_t count{};
};

struct InventoryFilter final {
    std::optional<items::ItemSlot> slot{};
    std::optional<items::ItemRarity> rarity{};
};

enum class InventoryClickKind : std::uint8_t {
    single,
    double_click,
};

struct InventoryClickTracker final {
    std::uint64_t last_item_id{};
    double last_click_seconds{-1.0};
};

struct RecipeSelection final {
    std::array<std::uint64_t, 3> ids{};
    std::size_t count{};
};

enum class InventoryItemOutline : std::uint8_t {
    none,
    hover,
    selected,
    recipe,
    material_target,
    disabled_target,
};

struct InventoryItemVisualInput final {
    std::uint64_t item_id{};
    std::uint64_t hovered_item_id{};
    std::uint64_t selected_item_id{};
    RecipeSelection recipe{};
    bool material_target_mode{};
    bool material_target_enabled{};
};

struct InventoryItemVisualState final {
    InventoryItemOutline outline{InventoryItemOutline::none};
    std::uint8_t recipe_ordinal{};
    bool selected_indicator{};
};

struct InventoryInputGate final {
    bool forward_actions{};
    bool forward_movement{};
    bool forward_descent{};
    bool forward_room_reset{};
    bool forward_passive_toggle{};
};

struct BuildDifference final {
    std::int64_t max_health{};
    std::int64_t max_barrier{};
    std::int64_t melee_damage{};
    std::int64_t impulse_scale{};
    std::int64_t move_speed{};
    std::int64_t attack_speed{};
    std::int64_t armor{};
    std::int64_t evasion{};
    std::array<std::int64_t, 5> flat_damage{};
    std::array<std::int64_t, 5> damage_increased{};
    std::array<std::int64_t, 4> damage_reduction{};
    std::array<std::int64_t, 4> damage_reduction_cap_bonus{};
    std::int64_t weapon_physical{};
    std::int64_t local_attack_speed_bp{};
    std::int32_t armor_reduction_bp{};
    std::int32_t evasion_rate_bp{};
};

struct InventoryViewCache final {
    static constexpr std::size_t kInvalidIndex = ~std::size_t{0U};

    std::uint64_t generation{~std::uint64_t{0U}};
    items::EquipmentState equipment{};
    InventoryFilter filter{};
    std::uint64_t selected_item_id{};
    RecipeSelection requested_recipe{};
    RecipeSelection resolved_recipe{};
    std::vector<std::size_t> filtered_indices{};
    std::size_t selected_index{kInvalidIndex};
    std::array<std::size_t, 3> recipe_indices{{
        kInvalidIndex, kInvalidIndex, kInvalidIndex}};
    bool recipe_ready{};
    std::size_t refresh_count{};
    std::size_t item_inspection_count{};
};

struct InventoryVisibleItemSource final {
    std::uint64_t item_id{};
    Rectangle bounds{};
    bool source_is_in_right_panel{};
    bool valid{};
};

enum class InventoryTooltipTone : std::uint8_t {
    primary,
    muted,
    positive,
    negative,
    rarity,
};

struct InventoryTooltipSegment final {
    std::array<char, 64> text{};
    InventoryTooltipTone tone{InventoryTooltipTone::primary};
};

struct InventoryTooltipLine final {
    std::array<InventoryTooltipSegment,
        kInventoryTooltipSegmentCapacity> segments{};
    std::size_t segment_count{};
};

struct InventoryTooltipView final {
    std::array<char, 96> title{};
    std::array<InventoryTooltipLine,
        kInventoryTooltipLineCapacity> lines{};
    std::size_t line_count{};
    bool valid{};
};

struct InventoryTooltipPlacedSegment final {
    std::uint8_t logical_line{};
    std::uint8_t segment{};
    Vector2 position{};
};

struct InventoryTooltipLayout final {
    Rectangle backing{};
    Rectangle content_bounds{};
    Rectangle retention_corridor{};
    std::array<InventoryTooltipPlacedSegment,
        kInventoryTooltipPlacementCapacity> placements{};
    std::size_t placement_count{};
    std::size_t visual_line_count{};
    Vector2 title_position{};
    float title_font_size{};
    float font_size{};
    float line_height{};
    bool blocks_pointer{};
    bool valid{};
};

struct InventoryTooltipInteractionState final {
    std::uint64_t hovered_item_id{};
    std::uint64_t frozen_item_id{};
    std::uint64_t suppressed_item_id{};
    Rectangle source{};
    Rectangle retention_corridor{};
    bool source_is_in_right_panel{};
    bool visible{};
};

struct InventoryTooltipInteractionInput final {
    std::uint64_t hover_candidate_item_id{};
    Rectangle hover_candidate_source{};
    Rectangle hover_candidate_corridor{};
    bool hover_candidate_source_is_in_right_panel{};
    Vector2 pointer{};
    bool pointer_inside_tooltip{};
    bool close_requested{};
    bool clear_persistent_selection_requested{};
    bool background_clear_requested{};
    bool source_invalidated{};
};

struct InventoryTooltipInteractionResult final {
    InventoryTooltipInteractionState state{};
    bool clear_persistent_selection{};
    bool blocks_pointer{};
};

struct InventoryPointerPriorityInput final {
    bool confirmation{};
    bool tab{};
    bool tooltip{};
    bool directed_material{};
    bool material_cancel_requested{};
    bool material_select{};
    bool material_target{};
    bool page_action{};
    bool filter{};
    bool skill_content{};
    bool recipe{};
    bool equipment{};
    bool background_clear{};
};

struct ItemAttributeLabel final {
    const char* name{};
    const char* scope{};
    const char* suffix{};
    const char* qualifier{};
    bool multiplier{};
};

struct InventoryDetailRowLayout final {
    Rectangle label_bounds{};
    Rectangle value_bounds{};
    float gap{};
    bool valid{};
};

struct InventoryControlTextStyle final {
    Color content_backing{};
    Color idle_backing{};
    Color active_backing{};
    Color disabled_backing{};
    Color remove_backing{};
};

[[nodiscard]] InventoryLayout inventory_layout(int width, int height) noexcept;
[[nodiscard]] InventoryShellLayout inventory_shell_layout(
    int width, int height) noexcept;
[[nodiscard]] int inventory_grid_columns(InventoryShellLayout layout) noexcept;
[[nodiscard]] Rectangle inventory_grid_item_rectangle(
    InventoryShellLayout layout, std::size_t filtered_position,
    float scroll_rows) noexcept;
[[nodiscard]] InventoryControlTextStyle
inventory_control_text_style() noexcept;
[[nodiscard]] std::array<char, 96> inventory_page_title(
    InventoryPage page, const char* inventory_binding_label) noexcept;
[[nodiscard]] ActiveSkillLoadoutLayout active_skill_loadout_layout(
    InventoryShellLayout shell) noexcept;
[[nodiscard]] ActiveSkillLoadoutLayout active_skill_loadout_layout(
    int width, int height) noexcept;
[[nodiscard]] InventoryTextSafeLayout inventory_text_safe_layout(
    int width, int height) noexcept;
[[nodiscard]] ActiveSkillLoadoutView make_active_skill_loadout_view(
    const skills::SkillLoadoutState& state,
    const ActiveSkillLoadoutSelection& selection,
    bool save_pending) noexcept;
[[nodiscard]] std::optional<ActiveSkillLoadoutCommand>
active_skill_loadout_command_after_click(
    const skills::SkillLoadoutState& state,
    const ActiveSkillLoadoutLayout& layout,
    Vector2 point,
    ActiveSkillLoadoutSelection& selection,
    bool save_pending) noexcept;
void advance_active_skill_loadout_selection(
    ActiveSkillLoadoutSelection& selection,
    const ActiveSkillLoadoutCommand& accepted_command) noexcept;
[[nodiscard]] VisibleGridRange visible_grid_range(
    std::size_t filtered_count, int columns, float scroll_rows,
    float viewport_height, float cell_height) noexcept;
[[nodiscard]] float clamp_inventory_scroll_rows(
    std::size_t filtered_count, int columns, float scroll_rows,
    float viewport_height, float cell_height) noexcept;
[[nodiscard]] std::vector<std::size_t> filtered_inventory_indices(
    const items::ItemOwnershipState& state, InventoryFilter filter);
[[nodiscard]] InventoryClickKind register_inventory_click(
    InventoryClickTracker& tracker, std::uint64_t item_id,
    double now_seconds) noexcept;
[[nodiscard]] bool toggle_recipe_selection(
    RecipeSelection& selection, std::uint64_t item_id) noexcept;
[[nodiscard]] Rectangle equipment_slot_rectangle(
    InventoryLayout layout, items::ItemSlot slot) noexcept;
[[nodiscard]] std::optional<items::ItemSlot> hit_test_equipped_slot(
    Vector2 point, InventoryLayout layout,
    const items::EquipmentState& equipment) noexcept;
[[nodiscard]] std::optional<items::ItemSlot> hit_test_equipped_slot(
    Vector2 point, InventoryShellLayout layout,
    const items::EquipmentState& equipment) noexcept;
[[nodiscard]] bool inventory_can_open(
    bool runtime_running, bool passive_overlay_open) noexcept;
[[nodiscard]] bool passive_overlay_can_toggle(bool inventory_open) noexcept;
[[nodiscard]] InventoryInputGate inventory_input_gate(
    bool inventory_open) noexcept;
[[nodiscard]] BuildDifference compare_player_builds(
    const combat::PlayerCombatBuild& current,
    const combat::PlayerCombatBuild& candidate) noexcept;
void refresh_inventory_view_cache(InventoryViewCache& cache,
    const items::ItemOwnershipState& state, std::uint64_t generation,
    InventoryFilter filter, std::uint64_t selected_item_id,
    RecipeSelection recipe);
[[nodiscard]] const items::ItemInstance* cached_selected_item(
    const InventoryViewCache& cache,
    const items::ItemOwnershipState& state) noexcept;
[[nodiscard]] bool cached_recipe_ready(
    const InventoryViewCache& cache) noexcept;
[[nodiscard]] InventoryVisibleItemSource inventory_visible_item_source(
    InventoryShellLayout shell, InventoryPage page,
    const items::ItemOwnershipState& state,
    const InventoryViewCache& cache,
    std::uint64_t item_id, float scroll_rows) noexcept;
[[nodiscard]] InventoryEmptyPresentation inventory_empty_presentation(
    const items::ItemOwnershipState& state,
    std::size_t filtered_count) noexcept;
[[nodiscard]] InventoryStatusPresentation inventory_status_presentation(
    const InventoryStatusInput& input) noexcept;
[[nodiscard]] InventoryTransientStatusFrameResult
inventory_transient_status_for_frame(
    InventoryTransientStatusState state,
    const InventoryTransientStatusInput& input) noexcept;
[[nodiscard]] InventoryItemVisualState inventory_item_visual_state(
    const InventoryItemVisualInput& input) noexcept;
[[nodiscard]] InventoryTooltipView inventory_tooltip_view(
    const items::ItemInstance& item,
    const std::optional<BuildDifference>& difference) noexcept;
[[nodiscard]] InventoryTooltipLayout inventory_tooltip_layout(
    InventoryShellLayout shell, Rectangle source,
    bool source_is_in_right_panel, const InventoryTooltipView& view,
    UiTextMeasureFn measure, void* measure_context) noexcept;
[[nodiscard]] InventoryTooltipInteractionResult inventory_tooltip_transition(
    InventoryTooltipInteractionState state,
    const InventoryTooltipInteractionInput& input) noexcept;
[[nodiscard]] float inventory_grid_scroll_after_wheel(
    InventoryPage page, InventoryShellLayout shell, Vector2 pointer,
    bool pointer_blocked, float wheel_delta, std::size_t filtered_count,
    float current_scroll_rows) noexcept;
[[nodiscard]] InventoryPointerLayer inventory_pointer_layer(
    const InventoryPointerPriorityInput& input) noexcept;
[[nodiscard]] ItemAttributeLabel item_attribute_label(
    items::ItemEffectKind effect, modifiers::StatId stat,
    modifiers::ModifierOperation operation, items::ItemSlot slot,
    std::uint8_t variant) noexcept;
[[nodiscard]] double item_attribute_display_value(
    std::int32_t raw_value, ItemAttributeLabel label) noexcept;
[[nodiscard]] Rectangle detail_line_rectangle(
    InventoryLayout layout, std::size_t line) noexcept;
[[nodiscard]] InventoryDetailRowLayout inventory_detail_row_layout(
    InventoryLayout layout, std::size_t line) noexcept;
[[nodiscard]] bool detail_content_fits(
    InventoryLayout layout, std::size_t line_count) noexcept;
[[nodiscard]] std::size_t detail_line_character_capacity(
    InventoryLayout layout) noexcept;
[[nodiscard]] bool detail_text_fits(InventoryLayout layout,
    std::string_view text) noexcept;

}  // namespace arpg::platform
