### Task 1: 建立共享壳层与固定容量展示合同

**Files:**
- Modify: `src/platform/raylib/inventory_view_math.hpp`
- Modify: `src/platform/raylib/inventory_view_math.cpp`
- Modify: `src/platform/raylib/ui_typography.hpp`
- Modify: `src/platform/raylib/ui_text_catalog.hpp`
- Modify: `src/platform/raylib/ui_text_catalog.cpp`
- Modify: `tests/platform/inventory_view_math_tests.cpp`
- Modify: `tests/platform/ui_text_catalog_tests.cpp`
- Modify: `tests/platform/shared_ui_font_tests.cpp`

**Interfaces:**
- Consumes: `items::ItemOwnershipState`、`items::ItemInstance`、`BuildDifference`、现有 `ui_utf8_*`、`ui_viewport_scale`、`ui_text_layout.hpp` 的 `UiTextMeasureFn` 与 `Rectangle`。
- Extends `UiTypography` with `float inventory_compact_min_font_size{12.0F};` and `float inventory_hint_font_size{14.0F};`; these names are inventory-local and do not change existing typography fields.
- Produces:

```cpp
inline constexpr std::size_t kInventoryPageCount = 3U;
inline constexpr std::size_t kInventoryTooltipLineCapacity = 32U;
inline constexpr std::size_t kInventoryTooltipSegmentCapacity = 8U;
inline constexpr std::size_t kInventoryTooltipPlacementCapacity =
    kInventoryTooltipLineCapacity * kInventoryTooltipSegmentCapacity;
inline constexpr std::uint16_t kInventoryTransientStatusFrameCount = 180U;

enum class InventoryPage : std::uint8_t {
    equipment,
    equipment_materials = equipment,  // Task 4 前的编译兼容别名
    skill_stones,
    materials,
    count,
};

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

[[nodiscard]] InventoryShellLayout inventory_shell_layout(
    int width, int height) noexcept;
[[nodiscard]] int inventory_grid_columns(
    InventoryShellLayout layout) noexcept;
[[nodiscard]] Rectangle inventory_grid_item_rectangle(
    InventoryShellLayout layout, std::size_t filtered_position,
    float scroll_rows) noexcept;
[[nodiscard]] std::optional<items::ItemSlot> hit_test_equipped_slot(
    Vector2 point, InventoryShellLayout layout,
    const items::EquipmentState& equipment) noexcept;
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
[[nodiscard]] InventoryVisibleItemSource inventory_visible_item_source(
    InventoryShellLayout shell, InventoryPage page,
    const items::ItemOwnershipState& state,
    const InventoryViewCache& cache,
    std::uint64_t item_id, float scroll_rows) noexcept;
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
```

在真实头文件中把 `InventoryItemVisualInput/State` 放在既有 `RecipeSelection` 定义之后，把 `InventoryVisibleItemSource` 及其函数声明放在既有 `InventoryViewCache` 定义之后；上述合同按概念分组展示，不要求把依赖类型前移或重复定义。

现有 `inventory_page_title(InventoryPage,const char*)` 同步改成三路：页名只取 `ui_text(UiTextId::equipment_title)`、`ui_text(UiTextId::inventory_page_skill_stones)`、`ui_text(UiTextId::inventory_page_materials)`，再与动态 `inventory_binding_label` 组成固定容量 UTF-8 标题；不得保留 `page == equipment ? ... : skill` 的二路 fallback。

Task 1 同时在 `UiTextId::recovery_dual_slot_instruction` 之后、`count` 之前只追加以下 26 个 ID，保持既有 0–44 ordinal 不变。下列右侧字符串是本轮唯一批准的固定文案；renderer 不得另写同义字符串：

| 新 ID | 精确 catalog 文本 |
|---|---|
| `inventory_page_skill_stones` | `技能石` |
| `inventory_page_materials` | `材料` |
| `inventory_empty_title` | `背包无装备` |
| `inventory_empty_hint` | `击败怪物，拾取装备` |
| `inventory_all_equipped_title` | `无剩余装备` |
| `inventory_all_equipped_hint` | `左击左方装备取出` |
| `inventory_filter_empty_title` | `当前过滤无结果` |
| `inventory_filter_empty_hint` | `切换装备槽及稀有度` |
| `inventory_skill_empty` | `暂无未装备主动技能石` |
| `inventory_skill_equip_hint` | `选择技能石后点击空槽装备` |
| `inventory_skill_swap_hint` | `选择两个技能槽交换` |
| `inventory_skill_remove_hint` | `选择已装备技能后可取出` |
| `inventory_comparison_title` | `装备后变化` |
| `inventory_no_change` | `装备后无变化` |
| `inventory_material_selected_format` | `已选择：%s · 切换至装备 · 右键取消` |
| `inventory_recipe_action_format` | `三合一（%u/3）` |
| `inventory_recipe_danger` | `危险：三合一取消强化` |
| `inventory_slot_filter_format` | `装备槽：%s` |
| `inventory_rarity_filter_format` | `稀有度：%s` |
| `inventory_context_hint` | `左键选择 · 快速左击两次装备 · 右键三合一` |
| `inventory_action_remove` | `取出` |
| `inventory_filter_all` | `全部` |
| `inventory_reinforcement_success_format` | `强化结果：成功 / +%u → +%u` |
| `inventory_reinforcement_failure_format` | `强化结果：失败 / +%u → +%u` |
| `inventory_reinforcement_destroyed_format` | `强化结果：失败 / 装备丢失 / +%u` |
| `inventory_coupon_success_format` | `强化券结果：成功 / +%u → +%u` |

`equipment` 页签复用既有 `UiTextId::equipment_title`，保存状态复用既有 `save_saving/save_saved/save_recovered/save_error`。这些 26 条只复用当前 639 个码点；实现不修改 `kUiGlyphCapacity`、atlas 数或 32 MiB 预算。`inventory_status_presentation()` 只能以 `ui_utf8_format(..., ui_text(id), ...)` 生成四种 receipt 文案，不得在 view 或 renderer 内复制裸 format。

- [ ] **Step 1: 在既有 case 中写 shell、空状态和输入优先级 RED**

扩展 `layouts_are_bounded_and_non_overlapping()`，不要增加 `kCases` 元素：

```cpp
for (const auto& size : kSizes) {
    const auto shell = platform::inventory_shell_layout(size[0], size[1]);
    ARPG_REQUIRE(shell.valid);
    ARPG_REQUIRE(inside(shell.header, shell.viewport));
    ARPG_REQUIRE(inside(shell.left_panel, shell.viewport));
    ARPG_REQUIRE(inside(shell.right_panel, shell.viewport));
    ARPG_REQUIRE(inside(shell.status_bar, shell.viewport));
    ARPG_REQUIRE(separated(shell.left_panel, shell.right_panel));
    ARPG_REQUIRE(shell.right_panel.width > shell.left_panel.width);
    ARPG_REQUIRE(platform::inventory_grid_columns(shell) == 3);
    ARPG_REQUIRE(shell.body_font_size >= 12.0F);
    ARPG_REQUIRE(shell.hint_font_size >= 12.0F);
    ARPG_REQUIRE(inside(shell.slot_filter_bounds, shell.filter_bar));
    ARPG_REQUIRE(inside(shell.rarity_filter_bounds, shell.filter_bar));
    ARPG_REQUIRE(separated(shell.slot_filter_bounds,
        shell.rarity_filter_bounds));
    ARPG_REQUIRE(inside(shell.empty_state_bounds, shell.right_panel));
    ARPG_REQUIRE(inside(shell.status_bounds, shell.status_bar));
    ARPG_REQUIRE(inside(shell.action_bounds, shell.status_bar));
    ARPG_REQUIRE(separated(shell.status_bounds, shell.action_bounds));
    if (shell.hint_visible) {
        ARPG_REQUIRE(inside(shell.hint_bounds, shell.status_bar));
        ARPG_REQUIRE(separated(shell.action_bounds, shell.hint_bounds));
    } else {
        ARPG_REQUIRE(size[0] == 800 && size[1] == 450);
    }
    for (Rectangle tab : shell.tabs) {
        ARPG_REQUIRE(tab.width >= 40.0F);
        ARPG_REQUIRE(tab.height >= 32.0F);
        ARPG_REQUIRE(inside(tab, shell.viewport));
    }
    for (std::size_t left{}; left < shell.tabs.size(); ++left) {
        for (std::size_t right = left + 1U;
                right < shell.tabs.size(); ++right) {
            ARPG_REQUIRE(separated(shell.tabs[left], shell.tabs[right]));
        }
    }
    for (Rectangle slot : shell.equipment_slots) {
        ARPG_REQUIRE(inside(slot, shell.left_panel));
        ARPG_REQUIRE(slot.width >= 40.0F);
        ARPG_REQUIRE(slot.height >= 32.0F);
    }
}

platform::InventoryPointerPriorityInput layered{};
layered.tooltip = true;
layered.directed_material = true;
layered.material_cancel_requested = true;
layered.material_select = true;
layered.material_target = true;
layered.page_action = true;
layered.filter = true;
layered.skill_content = true;
layered.recipe = true;
layered.equipment = true;
layered.background_clear = true;
ARPG_REQUIRE(platform::inventory_pointer_layer(layered)
    == platform::InventoryPointerLayer::tooltip);
```

在现有 filtering/cache case 内构造空 state、全穿戴 state 和筛选为空 state，要求三种标题与提示精确为规格文本。再覆盖 `inventory_item_visual_state()`：仅悬停为浅钢 1px；普通选中为 cyan；配方成员为 magenta 且 ordinal 为 1–3；同一物品既选中又属于配方时 recipe outline 优先且 `selected_indicator=true`；材料目标模式使用 gold/disabled target，不能回退 cyan。逐项锁定 `material target > recipe > selected > hover`，hover 不得因正式素材绘制成功而消失。

同一 filtering case 调 `inventory_visible_item_source()`：当前页已穿戴 item 返回对应左栏 `equipment_slots`；当前筛选且当前 scroll viewport 内的未穿戴 item 返回右栏当前 `inventory_grid_item_rectangle()`。先让 A 成为 frozen/selected，再逐一切换到过滤掉 A、技能/材料页、把 A 滚出 viewport，resolver 均返回 `valid=false`，transition 清 hover/frozen/visible，但外部 `selected_item_id_` fixture 始终仍为 A；恢复筛选/页签/scroll 后同一 A 再次返回当前 source。再以 1280→800 resize 证明旧 source 坐标不再被接受：resize 当帧清 tooltip state，下一帧 selected 只能从 800 shell 解出新 source。未知 id 同样 fail-closed，不得复用旧矩形。

在 `ui_text_catalog_tests.cpp` 扩展既有 fixed-message/visitor case：逐项断言上表 26 个 ID 的精确文本，把 `visitor_covers_the_complete_static_catalog()` 与 `catalog_lookups_and_visiting_do_not_allocate()` 两处 `303U` 都精确改为 329，且原 ordinal 0–44 静态合同仍成立。在 `shared_ui_font_tests.cpp::complete_catalog_is_covered_by_one_shared_plan()` 中构造 `const UiGlyphPlan plan = ui_glyph_plan()`，逐项调用 `ui_glyph_plan_covers_text(plan, ui_text(id))`，并要求 `plan.codepoint_count == 639U`、`!plan.capacity_exceeded`、单 atlas/32 MiB 不变。

扩展既有 `inventory_page_titles_follow_the_current_binding()`：装备、技能石、材料三页分别得到 catalog 页名；材料页使用 `右 Shift` 时结果是有效 UTF-8、保留动态绑定且绝不落入技能石文案。极长绑定仍走现有 UTF-8 安全省略。

- [ ] **Step 2: 写 status 与 tooltip 固定容量 RED**

在现有 comparison/detail case 内追加：

```cpp
platform::InventoryStatusInput status_input{};
status_input.page = platform::InventoryPage::equipment;
status_input.recipe_count = 3U;
status_input.recipe_ready = true;
status_input.authority_enabled = true;
const auto status = platform::inventory_status_presentation(status_input);
ARPG_REQUIRE(status.action_kind
    == platform::InventoryActionKind::combine);
ARPG_REQUIRE(status.action_enabled);
ARPG_REQUIRE(status.tone == platform::InventoryStatusTone::positive);
ARPG_REQUIRE(std::string_view{status.action.data()} == u8"三合一（3/3）");

platform::InventoryTransientStatusState transient{};
platform::InventoryTransientStatusInput baseline_input{};
baseline_input.commit_generation = 10U;
baseline_input.save_state = platform::InventorySavePresentationState::saved;
baseline_input.receipt_valid = true;
baseline_input.receipt_commit_generation = 10U;
const auto baseline = platform::inventory_transient_status_for_frame(
    transient, baseline_input);
ARPG_REQUIRE(baseline.presentation.kind
    == platform::InventoryTransientStatusKind::none);
transient = baseline.next;

platform::InventoryTransientStatusInput receipt_input{};
receipt_input.commit_generation = 11U;
receipt_input.save_state = platform::InventorySavePresentationState::saved;
receipt_input.receipt_valid = true;
receipt_input.receipt_success = true;
receipt_input.receipt_commit_generation = 11U;
receipt_input.receipt_before = 2U;
receipt_input.receipt_after = 3U;
const auto new_receipt = platform::inventory_transient_status_for_frame(
    transient, receipt_input);
ARPG_REQUIRE(new_receipt.presentation.kind
    == platform::InventoryTransientStatusKind::reinforcement_receipt);

const items::ItemInstance item = make_item(
    9001U, 1U, items::ItemRarity::rare);
platform::BuildDifference difference{};
difference.max_health = 14;
const auto tooltip = platform::inventory_tooltip_view(item, difference);
ARPG_REQUIRE(tooltip.valid);
ARPG_REQUIRE(tooltip.line_count <= tooltip.lines.size());
ARPG_REQUIRE(tooltip.line_count > 0U);
```

用包含 6 条词缀和九个非零比较组的装备，给 `inventory_tooltip_layout()` 传入 deterministic `UiTextMeasureFn`，要求 224px 内容宽度下 `placements` 可通过固定容量续行完整引用所有非零 segment；零比较组不产生 logical line，且同一 segment 不得被省略或重复引用。

在同一 status case 完成精确 transient trace：首次观察 sticky `saved` 与旧 receipt 只建立 generation 基线、不显示；同 generation 重复 300 帧仍不重播；新普通 commit 只显示 saved 180 帧，第 181 帧恢复材料/配方/context；新 receipt generation 优先于同 generation saved、完整保存 receipt 数据并只显示 180 帧；随后新的非强化 commit 不重播旧 receipt。`recovered` 只有在 renderer 已初始化后观察到新的 commit generation 边沿时才短暂显示，首次打开不重放启动期旧状态。NaN 不适用；generation 回退视为新 session 基线并 fail-closed 清除 transient。

再构造强化成功、强化失败未销毁、强化失败并销毁、强化券成功四个 `InventoryTransientStatusPresentation`，逐一调用 `inventory_status_presentation()`；最终 status 必须分别精确等于上表四个 catalog format 的 `ui_utf8_format(..., ui_text(id), ...)` 结果、有效 UTF-8、保留完整 before/after 数字且 `truncated=false`。四种 tone 精确为 positive、negative、negative、positive。另按优先级逐层验证：saving=warning、error/recovery=negative、material=warning、recipe ready=positive、recipe danger=negative、普通 context=muted；renderer 不得再从输入或字符串重判 tone。

同一既有 case 再用纯状态机验证：hover item A → 指针进入 source/backing corridor 时冻结 A → `close_requested` 后 `visible=false` 且 `suppressed_item_id=A` → 指针仍在 corridor 时 A 不得闪回 → 离开 corridor 后抑制清除并允许持久选择恢复。分别以左右来源输入 `hover_candidate_source_is_in_right_panel=false/true`，要求 next state 的同名字段逐字保存并供 retained frame 恢复，绝不取反。三种点击结果必须分别断言：左击 tooltip（`close_requested + clear_persistent_selection_requested`）得到关闭、clear=true、blocks=true；右击无材料（仅 `close_requested`）得到关闭、clear=false、blocks=true；`background_clear_requested` 得到关闭、clear=true。右击有材料不调用 transition，由 Task 4 dispatcher case 证明只调用 `clear_selection()`、tooltip 仍 visible 且不得穿透到 recipe/equipment。另对 `inventory_grid_scroll_after_wheel()` 证明只有装备页、pointer 位于 `shell.grid_viewport` 且 `pointer_blocked=false` 时滚动；技能/材料页、viewport 外、wheel=0、tooltip/confirmation blocker 为真时原样返回。

- [ ] **Step 3: 运行编译 RED**

Run:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
if ($LASTEXITCODE -eq 0) { throw 'Task1 compile RED missing' }
```

Expected: MSVC 首个业务错误为 `InventoryShellLayout` / `inventory_shell_layout` / `InventoryTooltipView` 等新接口不存在，而不是测试 include 或语法错误。

- [ ] **Step 4: 实现纯 shell、状态和 tooltip builder**

实现规则必须直接编码而不是依赖 renderer 常量：

```cpp
const float geometry_scale = std::clamp(std::min(
    static_cast<float>(width) / 1280.0F,
    static_cast<float>(height) / 720.0F), 0.625F, 1.5F);
const UiTypography type = ui_typography();
layout.body_font_size = std::max(type.inventory_compact_min_font_size,
    type.kInventoryBodyFontSize * geometry_scale);
layout.hint_font_size = std::max(type.inventory_compact_min_font_size,
    type.inventory_hint_font_size * geometry_scale);
```

保留旧 `InventoryPage::equipment_materials` 名称作为 `equipment` 的临时同值别名，Task 4 才在迁完 renderer/guards 后删除；因此 Task 1 的中间提交仍可独立编译。`inventory_status_presentation()` 按 `saving > error/recovery_required > transient receipt/saved/recovered > material > recipe > context` 选择唯一状态；它同时产出 tone，四种 receipt 只用上表 catalog format 和 UTF-8 安全格式器，普通 craft 不生成专属 result。`inventory_transient_status_for_frame()` 首次调用只记录 current/receipt generation；之后只有严格增加的 commit 或 receipt generation 才锁存一次展示。新 receipt 优先于同 generation saved；结果本帧输出后把 `next.remaining_frames` 递减，精确输出 180 帧后恢复 `none`。generation 回退清 transient 并重新建立基线。`inventory_pointer_layer()` 按规格顺序返回第一个 true layer。`inventory_item_visual_state()` 固定使用 `material target > recipe > selected > hover`。Tooltip 行使用 `ui_utf8_copy` / `ui_utf8_format` 写入数组；`inventory_tooltip_layout()` 使用传入的 `UiTextMeasureFn` 计算每个 segment 的最终位置，所有分量为零的比较组跳过，非零 segment 超过 224px 内容宽度时续到下一固定视觉行。

`InventoryTooltipLayout::content_bounds/title_position/title_font_size` 与 segment placements 都由同一个测量回调产生；标题测量失败或无法在最小 12px 下进入 backing 时 fail-closed。`inventory_tooltip_transition()` 只读输入状态和几何，固定地实现 freeze/suppress/restore，不访问鼠标全局；`inventory_grid_scroll_after_wheel()` 先拒绝 tooltip/confirmation 的 `pointer_blocked`，再做 page/viewport gating，最后用 shell 的 `grid_row_stride` 和既有 clamp。`grid_cell_height` 和 `grid_row_stride` 由 shell 一次产出；`visible_grid_range()` 与 `clamp_inventory_scroll_rows()` 统一传 `grid_row_stride`。新增的 shell `hit_test_equipped_slot()` 只遍历 `equipment_slots`，Task 4 前保留旧 `InventoryLayout` overload 供旧 renderer 编译。

- [ ] **Step 5: 加稳态无分配和极端输入 GREEN 断言**

在同一既有 layout case 中先本地构造 `item = make_item(...)`、包含该 item 的 `ownership`、已预热且容量稳定的 `view_cache`、`visible_item_id`、`BuildDifference difference{}`、三种 empty state、非空 `status_input`、recipe visual input 和优先级 input；再用同一组 fixture 预热所有 builder 一次，最后执行 10,000 次：

```cpp
const std::uint64_t before = arpg::test::allocation_count();
for (std::size_t iteration{}; iteration < 10000U; ++iteration) {
    const auto shell = platform::inventory_shell_layout(800, 450);
    const auto view = platform::inventory_tooltip_view(item, difference);
    const auto tip = platform::inventory_tooltip_layout(
        shell, shell.equipment_slots[0], false, view,
        &linear_measure, nullptr);
    const auto transition = platform::inventory_tooltip_transition({}, {});
    const auto empty = platform::inventory_empty_presentation(empty_state, 0U);
    const auto transient_frame =
        platform::inventory_transient_status_for_frame(
            transient, transient_input);
    transient = transient_frame.next;
    const auto status = platform::inventory_status_presentation(status_input);
    const auto visual = platform::inventory_item_visual_state(visual_input);
    const auto source = platform::inventory_visible_item_source(
        shell, platform::InventoryPage::equipment,
        ownership, view_cache, visible_item_id, 0.0F);
    const auto pointer_layer = platform::inventory_pointer_layer(layered);
    const float scroll = platform::inventory_grid_scroll_after_wheel(
        platform::InventoryPage::equipment, shell,
        {shell.grid_viewport.x + 1.0F, shell.grid_viewport.y + 1.0F},
        false, -1.0F, 40U, 0.0F);
    ARPG_REQUIRE(shell.valid && tip.valid && view.valid
        && transient_frame.next.initialized);
    ARPG_REQUIRE(!transition.state.visible && empty.title != nullptr
        && status.status[0] != '\0'
        && visual.outline != platform::InventoryItemOutline::none
        && source.valid
        && pointer_layer != platform::InventoryPointerLayer::none
        && scroll >= 0.0F);
}
ARPG_REQUIRE(arpg::test::allocation_count() == before);
```

NaN/Inf、非正 viewport、无面积 source 返回 `valid=false`；800/1280/1920 两侧来源均产生完整入屏且不覆盖 header/status 的 tooltip。三种尺寸还要逐一验证：三列网格、两个筛选命中矩形、`status/action/hint` 入屏且互斥、800×450 隐藏 hint 后动态绑定 `右 Shift` 仍以 UTF-8 完整码点安全省略。

- [ ] **Step 6: 构建并运行唯一 inventory selector**

Run:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Task1 platform build failed' }
powershell.exe -NoProfile -File tests/platform/full_chinese_inventory_selector.ps1 `
  -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Task1 inventory selector failed' }
```

Expected: build exit 0；选择器唯一输出 `120 cases, 0 failures`；catalog visitor 精确 329，glyph 精确 `639 < 640`。

- [ ] **Step 7: 审查并提交 Task 1**

```powershell
git diff --check
git add src/platform/raylib/inventory_view_math.hpp `
  src/platform/raylib/inventory_view_math.cpp `
  src/platform/raylib/ui_typography.hpp `
  src/platform/raylib/ui_text_catalog.hpp `
  src/platform/raylib/ui_text_catalog.cpp `
  tests/platform/inventory_view_math_tests.cpp `
  tests/platform/ui_text_catalog_tests.cpp `
  tests/platform/shared_ui_font_tests.cpp
git diff --cached --check
git commit -m "feat(ui): add POE2 inventory shell contracts"
```

---

