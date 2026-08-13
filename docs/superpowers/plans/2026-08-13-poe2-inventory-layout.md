# POE2 风格背包界面实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把现有固定三列背包重构为 POE2 主机版骨架：左侧角色/装备、右侧大型内容区、三页共享页签、按需浮动物品详情和单一底部状态/动作栏，同时完全保留当前键盘鼠标与 authority 请求语义。

**Architecture:** 先在 `inventory_view_math` 建立纯、固定容量的 shell/empty/status/tooltip/input-priority 合同，再让技能石和材料袋分别消费该合同，最后一次性切换 `InventoryRenderer` 的真实 draw/hit 路径。源代码守卫在生产切换时先保持 pristine GREEN，随后用可逆 mutants 独立强化，避免用字符串存在性代替真实活路径。

**Tech Stack:** C++17、raylib 6.0、CMake 3.25、MSVC Debug/Release presets、现有 SharedUiFont/UI material、项目自带无分配探针与 CMake source guards。

## Global Constraints

- 只实现背包壳层与“装备 / 技能石 / 材料”三个页面；不修改战斗 HUD、世界提示、暂停菜单、被动树或玩法数值。
- 采用 POE2 主机版的空间骨架和信息层级，不复制其美术、字体、图标、音效或像素布局。
- 保留单击查看、双击装备、`Ctrl + 左击`/右击三合一、单击已穿戴槽卸下、材料右击/应用、技能石选择/交换/取出及现有动态背包绑定。
- 800×450、1280×720、1920×1080 全部入屏；中文正文和提示最终字号不得低于 12px；基础交互命中最小 40×32 物理像素。
- 新增 shell/empty/status/tooltip/diff builder 使用固定容量、`noexcept`，稳态 10,000 次调用零动态分配；既有 `InventoryViewCache::filtered_indices` 的 generation/容量变化分配边界不扩大。
- 所有玩家文字来自 `ui_text_catalog` 或 UTF-8 安全格式器；不得新增 `GetFontDefault`、裸 `DrawText` 或半个 UTF-8 码点截断。
- 保持单一 SharedUiFont、单一 atlas、当前 glyph 计划精确 `639 < 640`；不得通过扩容、删除仍可达旧文案或第二字体掩盖新增文字覆盖问题。
- 素材绘制失败时，同尺寸程序化 fallback 仍绘制；语义 overlay 必须在素材/fallback 之后统一绘制。
- 不修改存档 schema、物品模型、材料规则、技能石协议或 authority accepted 后才推进 presentation selection 的约束。
- 遵守“尽量不测试”：每个任务只做必要增量 build、隔离 inventory selector 和直接受影响的精确 guard；full 与 `scripts/Test.ps1` 不执行，formal 在 Task 1–4 只编译/链接，Task 5 才运行三个作为确定性视觉 fixture producer 的目标。
- 不删除文件、不重置 Git、不覆盖无关改动；每个任务只 stage 自己列出的文件并单独提交。
- 本文 checkbox 是交接清单，不在实施提交中逐项改写；实际进度记录在任务报告和执行器 plan tracker，避免文档因追踪状态跨任务保持脏改动。
- 本规格与计划在执行 Task 1 前以独立 docs commit 提交；下文“5 个 commit”只指五个实现任务提交，最终工作树因此不会遗留未跟踪计划文件。

---

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

### Task 2: 让技能石布局消费共享壳层

**Files:**
- Modify: `src/platform/raylib/inventory_view_math.hpp`
- Modify: `src/platform/raylib/inventory_view_math.cpp`
- Modify: `src/platform/raylib/host_validation_stage17_runtime.cpp`
- Modify: `tests/platform/active_skill_loadout_view_tests.cpp`
- Modify: `tests/platform/input_latency_source_test.cmake`
- Modify: `tests/platform/stage12_material_formal_game_validation.cpp`

**Interfaces:**
- Consumes: Task 1 `InventoryShellLayout::left_panel/right_panel/action_bounds`。
- Produces:

```cpp
[[nodiscard]] ActiveSkillLoadoutLayout active_skill_loadout_layout(
    InventoryShellLayout shell) noexcept;
```

`remove_button` 必须等于 shell 的 `action_bounds`。Task 2 为保持独立编译，暂时保留 `ActiveSkillLoadoutLayout::{equipment_page_button,skill_stones_page_button}`，但新 overload 只能把它们分别复制自 `shell.tabs[equipment]` 与 `shell.tabs[skill_stones]`，不得独立计算；Task 4 在 renderer 和所有验证消费者迁完后再删除这两个临时字段。五个主槽是命中区，五个辅助槽只读且不进入 hit region。

- [ ] **Step 1: 把现有三分辨率 case 改成 shell 输入 RED**

```cpp
const auto shell = platform::inventory_shell_layout(width, height);
const auto layout = platform::active_skill_loadout_layout(shell);
ARPG_REQUIRE(layout.panel.x == shell.content.x);
ARPG_REQUIRE(layout.remove_button.x == shell.action_bounds.x);
ARPG_REQUIRE(layout.remove_button.y == shell.action_bounds.y);
ARPG_REQUIRE(layout.remove_button.width == shell.action_bounds.width);
ARPG_REQUIRE(layout.remove_button.height == shell.action_bounds.height);
for (Rectangle support : layout.support_slots) {
    ARPG_REQUIRE(inside(support, shell.left_panel));
}
for (Rectangle inventory : layout.inventory_slots) {
    ARPG_REQUIRE(inside(inventory, shell.right_panel));
}
```

保持 `pending_save_and_support_slots_never_emit_edit_commands()` 原断言，确保辅助槽不发请求；另用现有选择、装备、交换 case 证明新 shell 下右栏技能石、五个主动槽仍分别产生 `select/equip/swap`，`remove_button` 产生 `remove`，且 authority accepted 后才调用 `advance_active_skill_loadout_selection()`。

- [ ] **Step 2: 运行编译 RED**

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
if ($LASTEXITCODE -eq 0) { throw 'Task2 compile RED missing' }
```

Expected: 缺少 `active_skill_loadout_layout(InventoryShellLayout)` overload，而不是环境或测试语法失败。

- [ ] **Step 3: 实现新 overload 并更新验证消费者**

布局规则：左侧竖排五个主槽，当前槽的五个辅助槽位于左栏剩余区域；右栏排列未装备技能石。保留临时 `active_skill_loadout_layout(int,int)` wrapper，仅委托 `inventory_shell_layout(width,height)` 和新 overload，供 Task 4 前旧 renderer 编译；wrapper 不再独立计算坐标。旧 renderer 暂时读取的两个 page-button 字段也只镜像 shell tabs，不能形成第二坐标系。

把 `host_validation_stage17_runtime.cpp` 改为：

```cpp
const InventoryShellLayout shell = inventory_shell_layout(
    config.window_width, config.window_height);
const ActiveSkillLoadoutLayout layout = active_skill_loadout_layout(shell);
```

`host_validation_stage17_runtime.cpp` 中两处打开技能页的点击坐标都直接改为 `shell.tabs[static_cast<std::size_t>(InventoryPage::skill_stones)]`，不再消费临时字段。同步更新 `platform.input_latency_source` 的 exact token 和 reversible mutations；`stage12_material_formal_game_validation.cpp` 的纯 layout 检查也消费 shell，不运行窗口。

- [ ] **Step 4: 构建受影响目标并做定向 GREEN**

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_platform_tests arpg_stage12_material_formal `
    arpg_stage17_skill_stones_game_validation `
  --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Task2 focused build failed' }
powershell.exe -NoProfile -File tests/platform/full_chinese_inventory_selector.ps1 `
  -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Task2 inventory selector failed' }
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
  --parallel 1 -R '^platform\.input_latency_source$'
if ($LASTEXITCODE -ne 0) { throw 'Task2 input latency guard failed' }
```

Expected: targets compile/link；selector `120/0`；source guard `1/1`。不要运行 stage12/stage17 窗口。

- [ ] **Step 5: 审查并提交 Task 2**

```powershell
git diff --check
git add src/platform/raylib/inventory_view_math.hpp `
  src/platform/raylib/inventory_view_math.cpp `
  src/platform/raylib/host_validation_stage17_runtime.cpp `
  tests/platform/active_skill_loadout_view_tests.cpp `
  tests/platform/input_latency_source_test.cmake `
  tests/platform/stage12_material_formal_game_validation.cpp
git diff --cached --check
git commit -m "refactor(ui): place skill stones in inventory shell"
```

---

### Task 3: 让材料页绘制与命中消费同一布局

**Files:**
- Modify: `src/platform/raylib/material_bag_renderer.hpp`
- Modify: `src/platform/raylib/material_bag_renderer.cpp`
- Modify: `src/platform/raylib/ui_text_catalog.hpp`
- Modify: `src/platform/raylib/ui_text_catalog.cpp`
- Modify: `tests/platform/material_bag_renderer_tests.cpp`
- Modify: `tests/platform/ui_text_catalog_tests.cpp`
- Modify: `tests/platform/stage16_loot_reinforcement_game_validation.cpp`

**Interfaces:**
- Consumes: Task 1 `InventoryShellLayout::right_panel/geometry_scale`。
- Produces:

```cpp
[[nodiscard]] MaterialBagLayout material_bag_layout(
    Rectangle content, float scale) noexcept;

[[nodiscard]] bool MaterialBagRenderer::process_click(
    Vector2 point, const items::ItemOwnershipState& state,
    const MaterialBagLayout& layout) noexcept;
[[nodiscard]] bool MaterialBagRenderer::cycle_directed_category(
    Vector2 point, const MaterialBagLayout& layout) noexcept;
void MaterialBagRenderer::draw(
    const items::ItemOwnershipState& state, const MaterialPack& assets,
    Font font, bool font_ready, const MaterialBagLayout& layout,
    float scale) const noexcept;

[[nodiscard]] const char* ui_material_description(
    items::MaterialId id) noexcept;
```

材料说明固定分组为现有字形组成的三种文案：普通 craft 材料 `选择物品应用`；定向核心 `选择伤害 / 防御 / 速度 / 元素`；强化石和强化券 `强化装备`。

- [ ] **Step 1: 写 layout/draw-hit 同源 RED**

在 `material_bag_has_all_fourteen_slots()` 中把三分辨率调用改为：

```cpp
const auto shell = platform::inventory_shell_layout(width, height);
const auto bag = platform::material_bag_layout(
    shell.right_panel, shell.geometry_scale);
ARPG_REQUIRE(bag.contains_all_slots());
for (Rectangle slot : bag.slots) {
    ARPG_REQUIRE(inside(slot, shell.right_panel));
    ARPG_REQUIRE(slot.width >= 40.0F);
    ARPG_REQUIRE(slot.height >= 32.0F);
}
```

把 material selection case 改为调用 `process_click(center(slot), state, layout)`，定向材料 case 要求右击当前定向卡片只循环 category，不清除 selection。

- [ ] **Step 2: 写 catalog/glyph RED**

在既有 catalog case 中先取 `const UiGlyphPlan plan = ui_glyph_plan()`，再遍历 14 个 `MaterialId`：description 非空、UTF-8 有效、`ui_glyph_plan_covers_text(plan, description)`，且 catalog visitor 逐条可达。14 个稳定映射逐项进入 visitor，因此把 `visitor_covers_the_complete_static_catalog()` 与 `catalog_lookups_and_visiting_do_not_allocate()` 两处 329 都精确更新为 343；即使文本值重复也不能跳过某个 `MaterialId`。不要增加 glyph 容量；若任何字符串失败，使用本任务规定的三种现有文案，不另造同义词，并继续要求 `plan.codepoint_count == 639U`。

- [ ] **Step 3: 运行编译 RED**

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
if ($LASTEXITCODE -eq 0) { throw 'Task3 compile RED missing' }
```

Expected: 缺少 `material_bag_layout(Rectangle,float)`、新 process/draw overload 或 `ui_material_description`，而不是环境或测试语法失败。

- [ ] **Step 4: 实现同源 MaterialBagLayout 与中文说明**

`material_bag_layout(content,scale)` 对 14 个槽使用固定 2 列×7 行或在 800px 右栏证明可用的 3 列布局；所有 slots 由传入 content 一次计算。`process_click`、`cycle_directed_category` 和 `draw` 不得重新调用 width/height 版本。保留临时 width/height wrapper 只供 Task 4 前旧 renderer；wrapper 必须委托 shell + 新 overload。

- [ ] **Step 5: 更新 Stage16 的点击坐标**

先在该文件本地定义可独立编译的 `struct Resolution final { int width; int height; };` 与 `constexpr Resolution kResolution{1280,720};`。Task 3 的所有 helper 签名都接收 `Resolution`，不在函数体再写死尺寸；Task 4 只把单个常量扩为三项数组，不新发明类型。

`stage16_loot_reinforcement_game_validation.cpp` 使用：

```cpp
const auto shell = platform::inventory_shell_layout(
    resolution.width, resolution.height);
const auto bag = platform::material_bag_layout(
    shell.right_panel, shell.geometry_scale);
const Vector2 target = center(bag.slots[material_index]);
```

把该文件现有的全部旧坐标消费者一次迁完，不能只改材料点击：

- `first_inventory_cell()`：用 `inventory_grid_item_rectangle(shell, 0U, 0.0F)` 的中心；
- `inventory_cell(filtered_position)`：用同一个 shell 调 `inventory_grid_item_rectangle(shell, filtered_position, 0.0F)`，删除硬编码 2 列、24/100/58；
- `combine_button_center()`：改为 `center(shell.action_bounds)`；
- `inventory_close_clears_material_selection()`、`process_item_action()` 和强化石确认前的材料选择：都构造同一 `shell`，再把 `material_bag_layout(shell.right_panel, shell.geometry_scale)` 传给点击；
- `render_inventory_capture()` 内用于 camera/query/draw 的 width/height 改为其 `Resolution` 参数，不再写死 1280×720；
- 确认弹窗仍调用 `reinforcement_confirmation_layout(width,height)`，但 width/height 来自同一 Resolution。

这样当前 `inventory_layout(1280,720)` 的三处调用以及后续三个 `material_bag_layout(1280,720)` 消费者都在 Task 3 清零；camera/query/draw/capture 也已经通过单项 `Resolution` 独立编译，Task 4 删除旧 API 与扩成三分辨率时不会再暴露延迟错误。

- [ ] **Step 6: 构建与定向 GREEN**

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_platform_tests arpg_stage16_loot_reinforcement_game_validation `
  --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Task3 focused build failed' }
powershell.exe -NoProfile -File tests/platform/full_chinese_inventory_selector.ps1 `
  -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Task3 inventory selector failed' }
```

Expected: compile/link exit 0；selector `120/0`；catalog visitor 精确 343，glyph 精确 639。静态 `rg -n 'inventory_layout\(|material_bag_layout\(1280, 720\)' tests/platform/stage16_loot_reinforcement_game_validation.cpp` 无输出；不运行 stage16 窗口。

- [ ] **Step 7: 审查并提交 Task 3**

```powershell
git diff --check
git add src/platform/raylib/material_bag_renderer.hpp `
  src/platform/raylib/material_bag_renderer.cpp `
  src/platform/raylib/ui_text_catalog.hpp `
  src/platform/raylib/ui_text_catalog.cpp `
  tests/platform/material_bag_renderer_tests.cpp `
  tests/platform/ui_text_catalog_tests.cpp `
  tests/platform/stage16_loot_reinforcement_game_validation.cpp
git diff --cached --check
git commit -m "refactor(ui): place materials in inventory shell"
```

---

### Task 4: 切换真实 InventoryRenderer 到三页壳层

**Files:**
- Modify: `src/platform/raylib/inventory_renderer.hpp`
- Modify: `src/platform/raylib/inventory_renderer.cpp`
- Modify: `src/platform/raylib/inventory_view_math.hpp`
- Modify: `src/platform/raylib/inventory_view_math.cpp`
- Modify: `src/platform/raylib/material_bag_renderer.hpp`
- Modify: `src/platform/raylib/material_bag_renderer.cpp`
- Modify: `src/platform/raylib/host_validation_stage17.hpp`
- Modify: `src/platform/raylib/host_validation_stage17_runtime.cpp`
- Modify: `src/platform/raylib/host_validation_stage17_report.cpp`
- Modify: `tests/platform/inventory_view_math_tests.cpp`
- Modify: `tests/platform/active_skill_loadout_view_tests.cpp`
- Modify: `tests/platform/material_bag_renderer_tests.cpp`
- Modify: `tests/platform/stage12_material_formal_game_validation.cpp`
- Modify: `tests/platform/stage12_material_validator.ps1`
- Modify: `tests/platform/stage12_material_validator_self_test.ps1`
- Modify: `tests/platform/stage16_loot_reinforcement_game_validation.cpp`
- Modify: `tests/platform/stage17_skill_stones_game_validation.cpp`
- Modify: `tests/platform/stage17_skill_stones_validator.ps1`
- Modify: `tests/platform/stage17_skill_stones_validator_self_test.ps1`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_self_test.cmake`
- Modify: `tests/platform/host_input_source_test.cmake`
- Modify: `tests/platform/input_latency_source_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/inventory_renderer_source_test.cmake`
- Modify: `tests/platform/inventory_renderer_source_self_test.cmake`
- Modify: `tests/platform/ground_loot_renderer_source_test.cmake`
- Modify: `tests/platform/ground_loot_renderer_source_self_test.cmake`
- Modify: `tests/platform/full_chinese_inventory_progression_source_test.cmake`
- Modify: `tests/platform/full_chinese_inventory_progression_source_self_test.cmake`

**Interfaces:**
- Consumes: Task 1–3 的纯 view/layout 以及现有 `DungeonRuntime` 请求接口。
- Produces: 真正可达的 `equipment`、`skill_stones`、`materials` 三页；draw/hit 共享的 resize-aware frame layout 和易失 tooltip 字段：

```cpp
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

void refresh_frame_layout() noexcept;
void refresh_comparison(const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    std::uint64_t presentation_item_id);
[[nodiscard]] InventoryTooltipFrame build_tooltip_frame(
    const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    std::uint64_t presentation_item_id,
    Rectangle source, bool source_is_in_right_panel,
    Font measure_font, bool measure_font_ready);

InventoryFrameLayout frame_layout_{};
InventoryTooltipInteractionState tooltip_state_{};
InventoryTransientStatusState transient_status_state_{};
Font tooltip_measure_font_{};
bool tooltip_measure_font_ready_{};
```

`refresh_frame_layout()` 读取一次 `GetScreenWidth()/GetScreenHeight()`；尺寸未变且 cache valid 时原样复用，尺寸变化时一次构造 shell→skills→materials，并同时执行 `tooltip_state_ = {}`，因此旧 frozen/source/corridor 不跨 resize。`process_input()` 与 `draw()` 都先调用它，之后只能消费 `frame_layout_`，不得直接再次调用三个 layout builder。`open()`/`close()` 将 frame cache、`tooltip_state_`、`transient_status_state_` 和 tooltip 字体句柄 cache 全部清零；字体身份变化同样先清 `tooltip_state_` 再缓存新句柄。resize 或重新打开后的 selected fallback 只能通过新 frame 的 visible-source resolver 重解，不能复用旧矩形。这样 draw/hit 在同一客户区尺寸下使用同一份对象；窗口 resize 后两条路径都切到同一新对象。

`draw()` 收到当前 `hud_font/hud_font_ready` 后先验证 `hud_font_ready && IsFontValid(hud_font)`；有效时只复制 raylib `Font` 句柄到 `tooltip_measure_font_` 并置 ready，无效时清零并使整个 tooltip fail-closed。这不是第二字体、第二 atlas 或资源 owner。下一帧 `process_input()` 只能用该缓存 Font 和 renderer 内同一个 `measure_inventory_text()` 回调构造 hover/selected tooltip layout；首个尚未呈现的 frame 或字体失效时不产生 tooltip/backing/blocker。随后 `draw()` 用当帧 Font 重算同一个纯布局，并要求与 input 缓存字体的 `texture.id/baseSize/glyphCount` 身份一致；若字体身份变化，本帧清 tooltip state、更新 cache、只画不带 tooltip 的壳层，下一帧再恢复命中。这样 pointer blocker/backing 与最终 SharedUiFont 像素几何同源，不调用 `GetFontDefault()` 或近似字符宽度。

Task 4 从 `sync()` 删除旧二参 `refresh_comparison(session,snapshot)` 调用；`sync()` 只保留 material/view/selection/recipe reconcile。唯一 comparison 刷新入口是下述 `build_tooltip_frame()`，不得在 sync 或 draw 外再以 selected 隐式刷新。

`refresh_comparison()` 的第三参数是当前 presentation item：本帧直接 hover candidate 优先，其次可见 `frozen_item_id`，最后才是 `selected_item_id_`。它按该 id 查找真实 item，并把现有 `comparison_item_id_` cache key 与 generation/equipment 一起比较；不得再在函数内读取 `selected_item_id_` 或调用只认 selected 的 `cached_selected_item()`。无效/消失的 presentation item 清空 difference 并使 tooltip fail-closed。

`build_tooltip_frame()` 是 input/draw 的唯一 tooltip 构造入口：先以传入 item id 调 `refresh_comparison()`，再以同一 id 查 item，用该 item 与刚得到的 `difference_` 构造 view，并用传入 source、side、Font 和同一 `measure_inventory_text()` 构造 layout。

`process_input()` 在推进 transition/计算 blocker/priority 前执行严格两阶段：第一阶段取 `visible frozen → selected` 的当前 id，但 **frozen 与 selected 都必须**调用 Task 1 的 `inventory_visible_item_source(frame_layout_.shell, page_, state, view_cache_, id, scroll_rows_)` 重新验证并解析当前 source/`source_is_in_right_panel`。state 保存的旧 source 只用于同一 stable frame 内的 transition bookkeeping，不能绕过 resolver。筛选隐藏、滚出 viewport、切到技能/材料页、resize 当帧或 item 消失时传 id 0、清 hovered/frozen/visible/source/corridor presentation，但绝不清 `selected_item_id_`；恢复后 resolver 可从当前 frame 重新产生 selected source。resolver valid 后才重建 retained frame并判断 pointer 是否位于当前 backing/retention corridor；命中时保持 frozen、设置 tooltip layer/blocks=true，**不得**再 hit-test 下层槽位或网格。只有 retained frame 未阻断，第二阶段才从 shell 命中新候选 B，同时得到 candidate source/`source_is_in_right_panel`，按 `candidate B → frozen → selected` 选 presentation id 并调用 helper；transition 把 candidate 的 source/side 一起存入 next state。该布尔值逐字透传给 `inventory_tooltip_layout()`，不得取反。`pointer_inside_tooltip` 必须来自 retained frame 的 backing，不能拿 B 的候选 backing代替。`draw()` 同样先对 transition 后的 `frozen → selected` id 调 resolver，再以当前 source/side 和当帧 Font 调同一 helper；resolver invalid 时本帧清 tooltip presentation且不画。两条路径的 id、item、comparison cache key、view、difference 与布局必须一致；已选 A→首次 hover B 的 input 不能用 A 的旧 difference 计算 blocker，A tooltip 覆盖 B 时也不能穿透切换到 B。

旧 `InventoryLayout`、常驻 `detail`、`InventoryTextSafeLayout`、`InventoryDetailRowLayout`、`inventory_detail_row_layout()`、`material_bag_detail_bounds()`、`InventoryPage::equipment_materials` 兼容别名、技能页两个临时 page-button 字段和 material width/height production overload 在本任务结束时删除；验证消费者全部使用 shell API。tooltip 的三项易失字段不再由 renderer 各自改写，只保存 Task 1 的 `InventoryTooltipInteractionState` 并以 `inventory_tooltip_transition()` 推进。

- [ ] **Step 1: 先把两个 direct guard 改成真实 renderer wiring RED**

Task 1–3 已经让下列纯行为在既有 layout/filter/skill/material case 中 GREEN；Task 4 不把这些旧 GREEN 误称为 renderer RED，先只把两个 direct guard 的旧三列锚改为要求：

- `InventoryPage::equipment/skill_stones/materials` 三个 tab hit；
- `inventory_empty/all_equipped/filter_empty` 的可见规则；
- 普通选中 cyan、配方 1/3–3/3 magenta、材料目标 gold 的纯 visual state；
- 悬停 item 进入同一纯 visual state，优先级低于 selected/recipe/material target，素材成功和 fallback 路径都在素材后画同一浅钢框；
- 用 Task 1 的纯 transition 直接验证 tooltip source→corridor→backing 保持同一 item，左击关闭并清持久选择、无材料右击只关闭、有材料右击只取消材料且 tooltip 保持可见/不穿透，关闭后离开 corridor 前不闪回，background clear 不清 recipe/material/page；另构造 A tooltip 覆盖下层 B，指针进入 A backing 时 retained frame 必须 blocks=true、A 保持 frozen、B 不得成为 hover/click candidate；
- 输入序列 `confirmation > tab > tooltip > directed > cancel > material select > material target > action > filter > skill content > recipe > equipment > background clear`；
- 槽位/稀有度筛选的 draw 与 hit 都消费 `shell.slot_filter_bounds` / `shell.rarity_filter_bounds`；
- 技能页右栏选择、空槽装备、两槽交换和底部取出四条现有命令都可达；
- skill action 只在非空选中槽、非 pending、authority enabled 时为“取出”；
- material 全零仍显示 14 张 `×0` 禁用卡片；
- 用 `inventory_grid_scroll_after_wheel()` 直接验证滚轮只在装备页、鼠标位于 `frame_layout_.shell.grid_viewport` 且 tooltip/confirmation 都未阻断时改变 `scroll_rows_`；renderer source guard 另锁定该调用发生在无点击 early-return 之前并传入真实 blocker；
- `InventoryTooltipInteractionResult::clear_persistent_selection` 驱动左击非交互背景只清除 `selected_item_id_` 与 tooltip presentation，不清除 recipe/material/page。
- 每个真实 `draw()` frame 恰好一次构造 generation/receipt transient input、调用 helper 并回写 next，`process_input()` 不推进 180 帧；
- tooltip 命中与绘制复用同一已验证 SharedUiFont 句柄身份和同一 measure callback，首帧、字体失效或身份变化时 fail-closed。

两份 direct guard 还要要求 `InventoryRenderer::process_input` 和 `draw` 函数体实际调用 Task 1–3 helper、读取同一 `frame_layout_`、删除常驻 detail。此步先不改 self-test mutations，也不改生产；因此 pristine 旧 renderer 必须被 guard 拒绝。

- [ ] **Step 2: 运行 direct guard，确认真实 RED**

```powershell
foreach ($name in @(
    'platform.full_chinese_inventory_renderer_source',
    'platform.full_chinese_inventory_progression_source')) {
  ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
    --parallel 1 -R ('^' + [regex]::Escape($name) + '$')
  if ($LASTEXITCODE -eq 0) {
    throw "Task4 RED missing: old renderer accepted by $name"
  }
}
```

Expected: 两项都 exit nonzero，首因是旧 renderer 未消费三页 shell/frame layout/tooltip/status，而不是 guard 语法、文件缺失或 stale anchor。记录首因后才进入生产实现。

- [ ] **Step 3: 重写 `process_input` 的单一命中链**

每次进入 `process_input()` 时先调用 `refresh_frame_layout()`，随后只使用 `frame_layout_` 计算 hover/source/tooltip，再由这些结果构造 `InventoryPointerPriorityInput` 并调用 `inventory_pointer_layer()`。滚轮处理位于 click early-return 前。分派必须直接复用现有请求 API，按以下表执行：

| layer | 唯一处理 |
|---|---|
| `confirmation` | 沿用 `reinforcement_confirmation_layout()` 与 `MaterialBagRenderer::resolve_reinforcement_confirmation()`；确认后只调用现有 `DungeonRuntime::request_reinforcement()` |
| `tab` | 命中 `shell.tabs[index]` 后把 `page_` 设为对应 `InventoryPage`，不发送 authority 请求 |
| `tooltip` | 左击把 `close_requested=true`、`clear_persistent_selection_requested=true` 传给 transition；右击若有材料先进入 `material_cancel`，否则只传 `close_requested=true`；`blocks_pointer` 为真时本层不得穿透 |
| `directed_material` | 调用 `MaterialBagRenderer::cycle_directed_category(mouse, material_layout)` |
| `material_cancel` | 调用 `MaterialBagRenderer::clear_selection()` |
| `material_select` | 材料页左击卡片时调用 `MaterialBagRenderer::process_click(mouse, state, frame_layout_.materials)`，只改变材料 presentation selection |
| `material_target` | 按现有分支调用 `request_reinforcement()`、`request_coupon()` 或 `request_craft()`；仅 `accepted` 后 `service_pending_save()` |
| `page_action` | 装备页仅在 `recipe_ready()` 时调用 `request_recipe(recipe_.ids)`；技能页复用 `active_skill_loadout_command_after_click()` 产生的 remove 命令并调用 `request_remove_active_skill()`；材料页不产生该层 |
| `filter` | 只使用 `shell.slot_filter_bounds` / `shell.rarity_filter_bounds` 循环现有 `InventoryFilter`，随后 `sync()` 并把 `scroll_rows_` 清零 |
| `skill_content` | 对五个主动槽和右栏未装备技能石调用 `active_skill_loadout_command_after_click()`；沿用 `select/equip/swap` 三条分支，并仅在 authority `accepted` 后推进 selection |
| `recipe` | 仅在无材料目标时调用 `toggle_recipe_selection(recipe_, item.id)` |
| `equipment` | 沿用装备槽 `request_unequip()`、双击 `request_equip()` 和单击持久选择逻辑 |
| `background_clear` | 把 `background_clear_requested=true` 传给纯 transition；仅在 result 要求时清除 `selected_item_id_`，保留 recipe/material/page |
| `none` | 不处理点击 |

每一条 authority 请求仍以 `RequestResult::accepted` 为唯一 presentation 推进条件；被拒绝时保留当前页、选择、材料和 tooltip 状态。

Tooltip 内左击关闭；右击有材料先取消材料，否则关闭；关闭后 pointer 离开 retention corridor 才由纯 transition 解除 suppress。Authority request 被拒时不推进 selection、不清除材料。

每帧先对当前 frozen（否则 selected）调用 `inventory_visible_item_source()`；仅以 resolver 返回的当前 valid source/side 调 `build_tooltip_frame()`，invalid 时清 tooltip presentation 且不复用保存矩形。若 pointer 命中 retained backing/corridor，直接推进 blocker 并跳过所有下层 hit。只有未阻断时，才从同一 frame layout 命中装备槽/网格得到 `hover_candidate_item_id/source`，用候选优先的 presentation id 再调 helper，并把返回 layout 的 backing/corridor 送入 tooltip transition；不得先用 selected 的旧 view/layout 推进 blocker。绘制所有装备卡与装备槽时，把可见状态的 `frozen_item_id`（否则当前直接 hover id）传给 `InventoryItemVisualInput::hovered_item_id`。不能只在 tooltip backing 处记 hover 而漏掉来源卡片的浅钢框。

- [ ] **Step 4: 重写 draw 为 shell→page→overlay→status 顺序**

绘制顺序固定：全屏背景、header/tabs、左右主面板、左栏静态剪影、当前页面内容、素材后 selection overlays、当前空态、可见 tooltip、底部 status/action/hint、最后 confirmation modal。装备页只在 tooltip builder 有效时画详情；不再绘制常驻第三列。装备页与材料页都必须实际绘制同一六槽角色概览；技能页以技能槽替换该概览。空态必须通过 `empty_state_bounds` 居中绘制，不能只构造 view 后丢弃。所有顶栏按键使用 `inventory_binding_label`，不允许字面 `I`。

每个实际 `draw()` frame 恰好一次构造 `InventoryTransientStatusInput`：`commit_generation=snapshot.commit_generation`、`save_state` 来自 `DungeonRenderStatus::indicator`、receipt 字段完整来自 `snapshot.reinforcement_receipt`（包括 `receipt.commit_generation`）。随后恰好一次调用 `inventory_transient_status_for_frame(transient_status_state_, input)`，立即把 `result.next` 回写成员，并只把 `result.presentation` 传给 `InventoryStatusInput::transient`。`process_input()` 不推进 180 帧倒计时；关闭/重新打开只建立当前 generation 基线，不重放旧成功。错误/恢复要求仍直接来自 runtime，不能被 transient 过期清掉。

每张装备卡片和装备槽都调用 Task 1 的 `inventory_item_visual_state()`，renderer 不再自行推导 hover/cyan/magenta/gold 优先级。`InventoryStatusPresentation` 只画一条，文字与颜色都只消费其 `status/hint/action/tone`，不得再查看 receipt/input/text 重判；`action_bounds` 在装备页发送 combine、技能页发送 remove、材料页隐藏。普通 craft 只显示 save pending/saved/error，不制造 receipt。

- [ ] **Step 5: 更新确定性验证源码与资源期望**

Stage12 不承担它无法注入的装备状态：`stage12_material_formal_game_validation.cpp` 只把现有 host-config 默认 inventory showcase 的纯布局检查改为 shell/tabs/status，并继续产出 1280/1920 素材与真实字体基线。必须从该 C++ 内 1280 与 1920 两组 `inventory_ui_runtime_ok` required-resource array 同时删除 `Ui::inventory_panel_detail`；`kInventoryNonStretchResources` 可保留该枚举，因为未绘制资源不会触发 stretch 失败。默认新档装备页没有网格条目、材料卡和 tooltip；把 `inventory_required_roles` 精确改为该场景稳定可达的 `inventory_page_title`、`inventory_equipment_slot`、`inventory_status`，明确禁止 `inventory_statistics/grid_entry/material_entry/detail`。定义单一 `constexpr std::size_t kPoe2EmptyInventoryTextCount = 17U`，对应 1 个标题、3 个页签、6 个装备槽、2 个筛选按钮、2 行空态，以及底部 status/action/hint 三项；1280/1920 两条验证都要求 `measured_text_count == kPoe2EmptyInventoryTextCount`，不得继续使用两处分散的 `20U` 或放宽为“非零”。producer summary 写出同一 role mask 与精确 count；validator/self-test 锁定两分辨率一致，并用恢复 bit19、旧 roles 或 20U、删除任一新角色/改变 17U 的 mutation 证明合同。

同步修改 `stage12_material_validator.ps1`：从 inventory draw-mask 的 required bits 中删除旧 `inventory_panel_detail` bit 19，保留默认空装备页真实绘制的 equipment/tab/slot/button/label-plate；validator 同时读取 summary 的三角色 mask 与精确 `17U` count。`stage12_material_validator_self_test.ps1` 增加重新要求 bit 19、恢复旧 detail/material/grid roles、硬编码 20 次、删除新角色或把 17 改成其它非零值的 mutation。不得向 `RaylibHostConfig` 或主宿主增加只为截图服务的背包状态注入协议。

Stage16 保持 direct-render，不引入 `RaylibHostConfig`。在 `stage16_loot_reinforcement_game_validation.cpp` 定义 `Resolution` 数组，逐尺寸执行 `InitWindow(width,height)`；已有窗口时只在需要时 `SetWindowSize(width,height)`，随后所有 camera、world query、shell/material layout、点击坐标、draw 和 capture 都消费当前 Resolution。它直接构造 empty、filter-empty、all-equipped、selected、recipe、max-tooltip、tooltip-left-source、tooltip-closed、saving、saved、error、material-target、material-accepted、material-rejected、material-exhausted、receipt；`max-tooltip` 是右网格来源→左 backing，并先持久选择属性集 A、再 hover 属性不同的 B，要求来源 B 有浅钢 hover overlay 且 tooltip 标题/所有装备后变化均按 B 计算；随后把 A tooltip 放在下层 B 卡片上方，指针进入 A backing 必须保持 A frozen、blocks=true、B 无 hover/click 请求。`tooltip-left-source` 是左装备槽来源→右 backing，`tooltip-closed` 同帧无 tooltip draw。报告 `tooltip_transition_trace=pass` 只有完整走过 freeze→retained blocker 抑制下层 B→左击关闭+clear→同 corridor 不闪回→离开恢复才成立；另写 `tooltip_comparison_trace=pass`，只有 comparison cache key、标题和差值都从 B 产生且从未出现 A 的差值才成立。再输出 `status_transition_trace=pass`，只有真实依次绘制新 saved 180 帧→第181帧 context 恢复、真实 receipt 180帧→第181帧 context 恢复、同 generation sticky runtime 字段不重播，才成立。`material-target` 必须确认目标卡实际使用 gold outline，`receipt` 必须来自真实 reinforcement/coupon receipt。每张图用 `LoadImage()` 回读尺寸后才写报告；任何 1280/720 硬编码坐标均为失败。

Stage17 延伸现有 host-config 捕获链，而不是只改 test wrapper：在 `host_validation_stage17.hpp` 给 `Stage17Capture`/state 追加 skill-empty、skill-selected、skill-swapped、skill-removed；`host_validation_stage17_runtime.cpp` 在现有 open/select/equip/swap/remove 的权威步骤设置对应 capture。四张图的真实权威顺序固定为 `skill-empty → skill-removed → skill-selected → skill-swapped`：技能页首次稳定可见后置 empty pending；empty capture complete 才推进 `remove_slot_1`；权威 removal 已反映到 snapshot 后置 removed pending，complete 才推进 `select_draw_inventory`；选择 presentation 已反映后置 selected pending，complete 才推进 `equip_slot_5`；equip 后继续既有 swap，权威 swap 已反映后置 swapped pending，complete 才推进 `close_inventory`。只要 `capture_pending != none`，`inject_stage17_physical_edges()` 必须原样返回输入 snapshot，不得注入下一动作；只有 `mark_stage17_capture_complete()` 可清 pending 并执行上述推进，因此 selected 截图不能被下一帧 equip 覆盖。`host_validation_stage17_report.cpp` 给每个 capture 提供固定分辨率相关文件名并写 summary。

`stage17_skill_stones_game_validation.cpp` 的 child CLI 从 `--scenario <name> <run> <seed>` 扩为 `--scenario <name> <run> <seed> <width> <height>`。每个 Resolution 使用独立 `stage17-run/<width>x<height>/save`、`settings`、cmd 和 `production-summary-<width>x<height>.txt`，父进程逐份读取并汇总；1280 的 restart 明确复用 `stage17-run/1280x720/save`，storm 使用独立 `stage17-run/1280x720/storm-save`。`Stage17SkillStonesValidationState` 为三个 legacy capture 分别增加显式 `initial_skipped/draw_windup_skipped/draw_hit_skipped`，不得复用 captured 布尔。

非 1280 production 仍走同一权威步骤，但必须在设置 legacy `initial/draw_windup/draw_hit` pending 的同一帧、进入通用 pending pause 之前调用显式 `skip_stage17_capture_without_file()`：它原子地设置对应 skipped、完成与 `mark_stage17_capture_complete()` 相同的状态推进、清空 pending、在 summary 写 `skipped/N/A`，且不伪造 captured、不输出文件；其中 initial 必须推进到 `empty_slot_3`，draw-hit 完成路径必须推进到 `open_inventory`。各 legacy pending 条件都检查 `!captured && !skipped`，避免同一项重复进入 pending。`write_stage17_validation_summary()` 的 production pass 对每项使用 `captured || skipped`，但附加分辨率硬约束：1280×720 必须 `captured && !skipped` 且文件存在；800×450、1920×1080 必须 `!captured && skipped`、summary 为 N/A 且对应 legacy 文件不存在。这样 800/1920 不会因无 legacy path 永久暂停或被旧 pass predicate 判失败，只输出四张 POE2 skill 图。

1280 production 继续输出原三个 legacy 图和四张新图，storm/restart 继续输出原四张，所以旧七张基线名与总量不变、新增恰好 12 张且无覆写。`stage17_skill_stones_validator.ps1` 与 self-test 同时锁原 7 张和新增 12 张，逐份关联三份 production summary、captured/skipped 互斥、文件存在/禁止、唯一性和尺寸。`host_validation_sequence_guard_test.cmake` / self-test 同步锁定 skip-before-pause、`!captured && !skipped` 防重复、分辨率 pass predicate、四张新图的 capture-complete interlock、enum→runtime step→report name→capture-owner 的唯一顺序及删除/错序 mutation；`input_latency_source_test.cmake` 同步更新完整 injector normalized hash、exact tail 和 skip/pause/reorder mutations，不能因正文变化放宽该既有守卫。

POE2 场景共用精确 name 集合（`empty`、`filter-empty`、`all-equipped`、`selected`、`recipe`、`max-tooltip`、`tooltip-left-source`、`tooltip-closed`、`saving`、`saved`、`error`、`material-target`、`material-accepted`、`material-rejected`、`material-exhausted`、`receipt`、`skill-empty`、`skill-selected`、`skill-swapped`、`skill-removed`），文件名统一为 `poe2-inventory-<scenario>-<width>x<height>.png`；前 16 个由 Stage16，后 4 个由 Stage17 生产。Stage12 保留原名素材基线。Stage12/17 通过 host config，Stage16 通过 direct-render Resolution；三者在 capture 前要求 `GetScreenWidth()/GetScreenHeight()` 精确相等，导出后立即 `LoadImage()` 回读并 `UnloadImage()`。Task 4 只编译三目标、执行 PowerShell AST 语法检查和 headless source guards，不启动窗口；完整 validator/self-test 在 Task 5 生成 fixture 后执行。

`tests/platform/CMakeLists.txt` 不新增或重命名测试，只按新增工作量调整现有注册超时：Stage12 原超时保持；Stage16 real-raylib 600s；Stage17 real-raylib 600s、validator/self-test 120s/240s。fixture 依赖、`RUN_SERIAL` 与原 labels 保持不变。

`host_input_source_test.cmake` 同步把 `observe_stage17_inventory()` 的旧 remove→select、swap→close exact fragment 改为四张截图的 `capture_pending` + `mark_stage17_capture_complete()` interlock，加入删除 pause/错序推进/非1280 skip-after-pause mutation；不得只更新字符串哈希。`ground_loot_renderer_source_test.cmake` / self-test 同步把旧 `result_bounds` 和 InventoryRenderer 内直写“成功/失败/装备丢失”迁为：真实 receipt 字段进入唯一 `InventoryTransientStatusInput`、opaque status backing、180帧后 context 恢复；mutation 删除 receipt generation 去重、恢复第二条状态或使 backing 非不透明时必须拒绝。其它 ground/pickup/material/SharedUiFont 合同保持原样。

- [ ] **Step 6: 最小更新两个 direct guard，使 pristine 先 GREEN**

把旧 `InventoryDetailRowLayout` / `draw_detail_row` / 三列标题锚替换为：

- `InventoryRenderer::process_input` 同一次 shell/layout 命中；
- 三页 tabs 只来自 shell；
- 两个筛选按钮的 draw/hit 只来自 shell，技能 `select/equip/swap/remove` 全部仍有真实消费者；
- material draw/process 共享 `MaterialBagLayout`；
- 装备页空态、左栏剪影、材料页六槽概览均在真实 draw 函数体中；
- draw 中素材之后的 cyan/magenta/gold overlay；
- tooltip backing→segments→text 顺序；
- 唯一 `InventoryStatusPresentation` 和 `action_bounds`；
- `transient_status_state_` 的 open/close reset，以及每个 draw 恰好一次 generation/receipt input→helper→next 回写；
- `InventoryStatusPresentation::tone` 是状态颜色唯一语义来源，四种 receipt 文案只由 catalog format 生成；
- SharedUiFont、catalog、opaque backing、无 default font；
- 不允许 legacy `InventoryLayout`、`InventoryDetailRowLayout`、`inventory_detail_row_layout`、`material_bag_detail_bounds`、`detail_line_rectangle`、写死 `I`。
- tooltip input/draw 使用同一已验证 SharedUiFont 句柄身份、同一 measure callback；首帧/字体变化 fail-closed，不得近似测量；
- tooltip comparison target 先取 frozen、否则 selected，但两者都必须通过当前 `inventory_visible_item_source()`；comparison cache key、item lookup、view title 与 difference 必须属于同一 valid id；
- `process_input()` 与 `draw()` 都只调用 `build_tooltip_frame()`；input 必须先 retained frozen/selected blocker、后 direct candidate，禁止被 backing 覆盖的下层 hit，也禁止用 selected 的旧 difference 计算新候选 blocker；
- input/draw 两路都逐帧对 frozen 与 selected 调 visible-source resolver；筛选隐藏/切页/滚出时清 tooltip presentation、不清持久 selected，禁止复用旧 source；

同步把两份既有 self-test 的旧 `draw_detail_row`/三列 anchor 迁到新结构，保留它们当前已有的 mutation 种类并要求 self-test 仍 GREEN；本步骤不新增 Task 5 的三十二类 mutation。这样 Task 4 提交不会留下两个已注册 CTest 为红。Task 5 再先加新 mutants 取得 RED 后强化 guards。

- [ ] **Step 7: 构建、selector 和两个 direct guard GREEN**

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_platform_tests `
    arpg_stage12_material_formal `
    arpg_stage16_loot_reinforcement_game_validation `
    arpg_stage17_skill_stones_game_validation `
  --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Task4 focused build failed' }
powershell.exe -NoProfile -File tests/platform/full_chinese_inventory_selector.ps1 `
  -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Task4 inventory selector failed' }
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
  --parallel 1 -R '^platform\.full_chinese_inventory_renderer_source(_self_test)?$'
if ($LASTEXITCODE -ne 0) { throw 'Task4 inventory renderer guards failed' }
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
  --parallel 1 -R '^platform\.full_chinese_inventory_progression_source(_self_test)?$'
if ($LASTEXITCODE -ne 0) { throw 'Task4 inventory progression guards failed' }
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
  --parallel 1 -R '^platform\.host_validation_sequence(_self_test)?$'
if ($LASTEXITCODE -ne 0) { throw 'Task4 host validation sequence guards failed' }
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
  --parallel 1 -R '^platform\.full_chinese_ground_loot_renderer_source(_self_test)?$'
if ($LASTEXITCODE -ne 0) { throw 'Task4 ground loot and receipt guards failed' }
foreach ($name in @(
    'platform.host_validation_source',
    'platform.host_input_source',
    'platform.input_latency_source')) {
  ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
    --parallel 1 -R ('^' + [regex]::Escape($name) + '$')
  if ($LASTEXITCODE -ne 0) { throw "focused host guard failed: $name" }
}
foreach ($script in @(
    'tests/platform/stage12_material_validator.ps1',
    'tests/platform/stage12_material_validator_self_test.ps1',
    'tests/platform/stage17_skill_stones_validator.ps1',
    'tests/platform/stage17_skill_stones_validator_self_test.ps1')) {
  $tokens = $null
  $parserErrors = $null
  [System.Management.Automation.Language.Parser]::ParseFile(
    (Resolve-Path $script), [ref]$tokens, [ref]$parserErrors) | Out-Null
  if ($parserErrors.Count -ne 0) {
    throw "validator PowerShell syntax rejected: $script"
  }
}
```

Expected: build targets compile/link；selector `120/0`；两组 inventory direct+self-test、host sequence、ground-loot/receipt guard 各 `2/2`；三个既有 host consumer guard 各 `1/1`；四个 validator/self-test PowerShell AST 无错误。不运行三个 formal/real-raylib executables。

- [ ] **Step 8: 独立审查并提交 Task 4**

审查者重点检查：材料定向右击、tooltip blocker、authority accepted、所有旧 detail 绘制是否清除、素材成功路径 overlay 是否仍存在。修复所有 C/I 后：

```powershell
git diff --check
git add src/platform/raylib/inventory_renderer.hpp `
  src/platform/raylib/inventory_renderer.cpp `
  src/platform/raylib/inventory_view_math.hpp `
  src/platform/raylib/inventory_view_math.cpp `
  src/platform/raylib/material_bag_renderer.hpp `
  src/platform/raylib/material_bag_renderer.cpp `
  src/platform/raylib/host_validation_stage17.hpp `
  src/platform/raylib/host_validation_stage17_runtime.cpp `
  src/platform/raylib/host_validation_stage17_report.cpp `
  tests/platform/inventory_view_math_tests.cpp `
  tests/platform/active_skill_loadout_view_tests.cpp `
  tests/platform/material_bag_renderer_tests.cpp `
  tests/platform/stage12_material_formal_game_validation.cpp `
  tests/platform/stage12_material_validator.ps1 `
  tests/platform/stage12_material_validator_self_test.ps1 `
  tests/platform/stage16_loot_reinforcement_game_validation.cpp `
  tests/platform/stage17_skill_stones_game_validation.cpp `
  tests/platform/stage17_skill_stones_validator.ps1 `
  tests/platform/stage17_skill_stones_validator_self_test.ps1 `
  tests/platform/host_validation_sequence_guard_test.cmake `
  tests/platform/host_validation_sequence_guard_self_test.cmake `
  tests/platform/host_input_source_test.cmake `
  tests/platform/input_latency_source_test.cmake `
  tests/platform/CMakeLists.txt `
  tests/platform/inventory_renderer_source_test.cmake `
  tests/platform/inventory_renderer_source_self_test.cmake `
  tests/platform/ground_loot_renderer_source_test.cmake `
  tests/platform/ground_loot_renderer_source_self_test.cmake `
  tests/platform/full_chinese_inventory_progression_source_test.cmake `
  tests/platform/full_chinese_inventory_progression_source_self_test.cmake
git diff --cached --check
git commit -m "feat(ui): adopt POE2 inventory layout"
```

---

### Task 5: 强化源代码守卫并完成最小交付验证

**Files:**
- Modify: `tests/platform/inventory_renderer_source_test.cmake`
- Modify: `tests/platform/inventory_renderer_source_self_test.cmake`
- Modify: `tests/platform/full_chinese_inventory_progression_source_test.cmake`
- Modify: `tests/platform/full_chinese_inventory_progression_source_self_test.cmake`
- Modify: `docs/superpowers/specs/2026-08-13-poe2-inventory-layout-design.md`

**Interfaces:**
- Consumes: Task 4 的最终 live renderer/helper bodies。
- Produces: 两组 exact guard/self-test，可拒绝旧布局、断开消费者、dead decoy、uncalled lambda、错序 overlay、不同 draw/hit layout、default font、硬编码绑定和丢失动作入口。

- [ ] **Step 1: 先向 self-tests 加真实可逆 mutants**

每个 mutant 必须只破坏一个合同，并确保 pristine source 的锚点唯一。至少包含：

1. `legacy_three_column_detail`：在 draw 中恢复常驻 detail；
2. `draw_hit_use_different_shells`：process_input 物料命中重新按 width/height 计算；
3. `material_overlay_before_asset`：把语义框移到 material draw 前；
4. `recipe_and_selected_same_border`：配方回退为普通 cyan；
5. `status_drawn_twice`：材料和 receipt 分别直接画第二条状态；
6. `missing_combine_action`：删除 equipment `action_bounds` 请求；
7. `missing_remove_action`：删除 skill `action_bounds` 请求；
8. `hardcoded_inventory_key`：把动态 binding 换成 `I`；
9. `tooltip_tokens_in_if_false`：把真实 tooltip draw 放进 `if(false)`；
10. `tooltip_tokens_in_uncalled_lambda`：把 backing/segments/text 放进未调用 lambda；
11. `tooltip_click_through`：删掉 pointer blocker return；
12. `default_font_fallback`：SharedUiFont 无效时回退 `GetFontDefault()`；
13. `material_core_name`：改用 core English name；
14. `zero_diff_rows_return`：恢复全零比较行。
15. `filter_hit_recomputed`：筛选 hit 绕开 shell 矩形；
16. `skill_content_disconnected`：删除技能 `select/equip/swap` 活路径，仅保留取出；
17. `empty_state_not_drawn`：保留 builder 但删除 renderer 空态调用；
18. `silhouette_not_drawn`：删除装备页静态剪影；
19. `material_equipment_overview_missing`：材料页不画六槽概览。
20. `material_selection_disconnected`：材料卡片 draw 仍在，但左击不再调用 `process_click()`；
21. `background_clear_click_through`：背景左击不清除 tooltip presentation，或继续穿透到装备请求。
22. `sticky_saved_never_expires`：同 generation 的 runtime `saved` 在第 181 个 draw frame 后仍压住上下文；
23. `receipt_replayed_same_generation`：同一 `receipt.commit_generation` 过期后再次触发；
24. `transient_next_not_stored`：调用 transient helper 但不把 `result.next` 回写 renderer；
25. `tooltip_approximate_measure`：`process_input()` 用字符数或固定宽度代替 SharedUiFont 测量；
26. `tooltip_font_identity_ignored`：字体句柄身份变化后仍沿用旧 backing/blocker。
27. `hover_outline_missing`：tooltip 仍出现，但来源卡片不再通过统一 visual state 绘制浅钢 hover 框；
28. `hover_comparison_uses_selected_item`：已选 A、悬停 B 时 `refresh_comparison()` 仍以 `selected_item_id_`/A 为 cache key 或 lookup。
29. `input_tooltip_uses_stale_selected_difference`：`process_input()` 在 transition 前绕过共同 helper，以 A 的旧 difference 为首次 hover B 计算 blocker，而 draw 才切到 B。
30. `tooltip_backing_hits_underlying_item`：A tooltip 覆盖 B 时先 raw-hit B 再检测 A backing，导致指针进入 A 后 B 被 hover/点击或 A 被替换。
31. `hidden_frozen_or_selected_reuses_old_source`：frozen/selected A 被筛选隐藏、切页或滚出 viewport 后仍用旧矩形显示 tooltip，或为隐藏 tooltip 清掉持久 `selected_item_id_`；
32. `tooltip_state_survives_lifecycle_boundary`：open/close、resize 或字体身份变化后仍保留旧 frozen/suppressed/source/corridor。

- [ ] **Step 2: 运行旧 guard self-tests，确认真实 RED**

```powershell
$redFailures = 0
foreach ($name in @(
    'platform.full_chinese_inventory_renderer_source_self_test',
    'platform.full_chinese_inventory_progression_source_self_test')) {
  ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
    --parallel 1 -R ('^' + [regex]::Escape($name) + '$')
  if ($LASTEXITCODE -eq 0) {
    throw "Task5 mutation RED missing: $name unexpectedly accepted all new mutants"
  }
  $redFailures++
}
if ($redFailures -ne 2) { throw 'Task5 did not observe both mutation RED runs' }
```

Expected: 两个 self-test 都因各自至少一个新增 mutant 被旧 guard 接受而 exit nonzero；pristine source 必须仍被接受。若任一项因 no-tests、脚本语法或 pristine 被拒而失败，先修测试装配，不把该结果算作 mutation RED。

- [ ] **Step 3: 强化函数体级 guard**

对 `InventoryRenderer::process_input`、`InventoryRenderer::draw`、tooltip helper、material layout/process/draw、status/action helper 分别用 `extract_cpp_function_body`。在已去字符串/注释的 code 上拒绝：

- `if(false)` / `if constexpr(false)` / `#if 0`；
- 未调用 lambda 中的 required tokens；
- font gate 后额外无条件 return；
- backing/asset/overlay/text 次序颠倒；
- production width/height material overload；
- legacy detail API 与硬编码 `I`；
- transient generation/receipt 去重、180-frame expiry 或 next-state 回写被删除；
- tooltip input/draw 字体句柄身份、同一 measure callback 或首帧/换字体 fail-closed 被绕开。
- hover visual 优先级、`frozen_item_id` comparison target 或 item-id cache key 被断开。
- `inventory_visible_item_source()` 未在 input/draw 的 frozen 与 selected 两路逐帧活调用，筛选隐藏/切页/滚出时复用旧 source 或错误清除持久选择；
- open/close、resize、字体身份变化任一路未清 `tooltip_state_`，或左右来源布尔未按 `source_is_in_right_panel` 原样传给 layout。

保留正向 controls：注释和字符串中的 forbidden 文本、合法数组下标、实际调用 lambda 不应误报。

- [ ] **Step 4: 运行两组 direct + self-test GREEN**

```powershell
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
  --parallel 1 -R '^platform\.full_chinese_inventory_renderer_source(_self_test)?$'
if ($LASTEXITCODE -ne 0) { throw 'Task5 inventory renderer guards failed' }
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
  --parallel 1 -R '^platform\.full_chinese_inventory_progression_source(_self_test)?$'
if ($LASTEXITCODE -ne 0) { throw 'Task5 inventory progression guards failed' }
```

Expected: 两组各 `2/2 passed`，所有新增 mutants 被拒，positive controls 被接受。

- [ ] **Step 5: 做最终最小自动化 readback**

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_platform_tests `
    arpg_stage12_material_formal `
    arpg_stage16_loot_reinforcement_game_validation `
    arpg_stage17_skill_stones_game_validation `
  --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Task5 final focused build failed' }
powershell.exe -NoProfile -File tests/platform/full_chinese_inventory_selector_self_test.ps1
if ($LASTEXITCODE -ne 0) { throw 'Inventory selector self-test failed' }
powershell.exe -NoProfile -File tests/platform/full_chinese_inventory_selector.ps1 `
  -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Final inventory selector failed' }
git diff --check
if ($LASTEXITCODE -ne 0) { throw 'Working diff check failed' }
```

Expected: build exit 0；selector self-test PASS；实际 selector 唯一 `120 cases, 0 failures`；formal targets 只编译/链接。

- [ ] **Step 6: 构建 Release 并做隔离视觉验收**

```powershell
scripts/Build.ps1 -Preset windows-msvc-release
if ($LASTEXITCODE -ne 0) { throw 'POE2 inventory Release build failed' }
```

不启动默认存档的 `arpg_game`，因为其 CLI 没有 width/height 参数，不能充当三分辨率证据。Task 4 的三个 Release producer 分工明确：Stage12 只产现有 host-config 素材/默认背包基线，Stage16 用 direct-render Resolution 产装备与材料全部 POE2 场景，Stage17 用现有 host-config 捕获链产技能页场景；三者都在导出后回读 PNG 尺寸。按顺序验收：

- 800×450、1280×720、1920×1080 的新存档 empty page；
- 普通/筛选空/全部穿戴三种状态；
- 普通 cyan、三合一 magenta 1/3–3/3、材料 gold；
- 最大词缀 tooltip、左右来源、浅钢 hover overlay、`tooltip_transition_trace=pass`、`tooltip_comparison_trace=pass`，并证明已选 A 后悬停 B 的标题/差值属于 B、点击关闭且不闪回；
- material-target gold、材料定向类别、请求拒绝/接受/耗尽、真实 receipt；
- 技能石空态、交换、取出；
- 保存 pending/error/receipt 的唯一状态栏与 `status_transition_trace=pass`。

所有场景都不依赖默认存档或人工刷取。三个确定性验证器作为只读 fixture producer，只写 `out/build/windows-msvc-release` 下各自允许的证据目录：

```powershell
$releaseRoot = (Resolve-Path 'out/build/windows-msvc-release').Path
$stage12Evidence = Join-Path $releaseRoot 'stage12 material evidence'
$stage16Evidence = Join-Path $releaseRoot 'stage16 loot reinforcement evidence'
$stage17Evidence = Join-Path $releaseRoot 'stage17 skill stones evidence'
& (Join-Path $releaseRoot 'bin/arpg_stage12_material_formal.exe') $stage12Evidence
if ($LASTEXITCODE -ne 0) { throw 'Stage12 inventory visual fixture failed' }
& (Join-Path $releaseRoot 'bin/arpg_stage16_loot_reinforcement_game_validation.exe') $stage16Evidence
if ($LASTEXITCODE -ne 0) { throw 'Stage16 material visual fixture failed' }
& (Join-Path $releaseRoot 'bin/arpg_stage17_skill_stones_game_validation.exe') $stage17Evidence
if ($LASTEXITCODE -ne 0) { throw 'Stage17 skill visual fixture failed' }

$stage12Run = Join-Path $stage12Evidence 'stage12-run'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  tests/platform/stage12_material_validator.ps1 `
  -EvidenceDirectory $stage12Run
if ($LASTEXITCODE -ne 0) { throw 'Stage12 evidence validator failed' }
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  tests/platform/stage12_material_validator_self_test.ps1 `
  -EvidenceDirectory $stage12Run `
  -Validator tests/platform/stage12_material_validator.ps1 `
  -MutationRoot (Join-Path $stage12Evidence 'poe2-validator-mutations')
if ($LASTEXITCODE -ne 0) { throw 'Stage12 validator self-test failed' }

$stage17Run = Join-Path $stage17Evidence 'stage17-run'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  tests/platform/stage17_skill_stones_validator.ps1 `
  -EvidenceDirectory $stage17Run
if ($LASTEXITCODE -ne 0) { throw 'Stage17 evidence validator failed' }
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  tests/platform/stage17_skill_stones_validator_self_test.ps1 `
  -EvidenceDirectory $stage17Run `
  -Validator tests/platform/stage17_skill_stones_validator.ps1 `
  -MutationRoot (Join-Path $stage17Run 'poe2-validator-mutations')
if ($LASTEXITCODE -ne 0) { throw 'Stage17 validator self-test failed' }
```

Stage16 executable 自身对三种 Resolution、固定场景名和 PNG readback 非零失败；Stage12/17 则再由上面的正式 validator 与 mutation self-test 锁定各自报告。逐张用本地 `view_image` 打开 producer 生成的命名截图并记录绝对路径；同时读取每个 producer 的尺寸 readback 报告，必须覆盖其分工内全部场景且每张 PNG 的真实尺寸匹配文件名，才可把 Task 5 标为完成。若 producer 未生成某个场景，本任务停在未完成状态并补最小确定性场景，不得仅标记“等待”后宣称交付。验证窗口正常退出，不使用 taskkill。

- [ ] **Step 7: 独立终审、更新规格状态并提交 Task 5**

把规格头状态改为 `已实现，等待/完成 Windows 视觉验收`，依据 Step 6 的真实结果选择其中一个，不声称未执行的视觉状态已通过。独立 reviewer 对 Task 4–5 diff 给 C/I/M；修复所有 C/I 后：

```powershell
git diff --check
git add tests/platform/inventory_renderer_source_test.cmake `
  tests/platform/inventory_renderer_source_self_test.cmake `
  tests/platform/full_chinese_inventory_progression_source_test.cmake `
  tests/platform/full_chinese_inventory_progression_source_self_test.cmake `
  docs/superpowers/specs/2026-08-13-poe2-inventory-layout-design.md
git diff --cached --check
git commit -m "test(ui): guard POE2 inventory layout"
git status --short --branch
```

最终报告必须列出：执行前 docs commit + 5 个实现 commit、实际 build/selector/guard 命令与结果、glyph count、截图绝对路径与 PNG 尺寸 readback、未运行的 full/`scripts/Test.ps1`，以及任何仍待 Windows 人工验收的场景；三个 Release fixture producer 及 Stage12/17 validators 属已运行的正式视觉证据，不能列为未运行。
