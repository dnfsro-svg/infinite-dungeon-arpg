# Full Chinese Inventory and Progression Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 汉化背包、装备、材料、强化、被动树、怪物词缀和死亡归因，并让交互按钮居中、数据表对齐、中文名称与稳定内部 ID 完全分离。

**Architecture:** 业务核心继续保存英文 key 和稳定数字 ID；raylib 展示层通过 `ui_text_catalog` 映射中文。背包/材料使用纯布局函数，节点/怪物/死亡页面统一借用共享 CJK 字体。

**Tech Stack:** C++17、raylib 6.0、`ui_text_catalog`、`ui_utf8`、`SharedUiFont`、现有 inventory/passive/combat ViewModel 测试。

## Global Constraints

- 依赖 foundation 和 menus/HUD 两份计划全部完成。
- 不修改 `src/items`、`src/passives`、`src/combat` 中的稳定 ID、数值、规则或存档字段。
- 装备底材标识类型是 `std::uint8_t base_id`，不得引入不存在的 `BaseId`。
- 被动节点内部 `name` 继续保持英文 key，现有核心测试必须继续通过。
- 按钮/页签/确认框居中；属性表保持标签左、数值右。
- 怪物头顶短标限制为一至两个汉字；完整名称只出现在详情/死亡页。
- 所有中文路径使用共享字体和 `MeasureTextEx`，不得以 raylib 默认字体绘制。

---

## File Structure

- Modify: `src/platform/raylib/ui_text_catalog.hpp/.cpp` — 补齐装备、材料、被动、怪物和词缀映射。
- Modify: `src/platform/raylib/inventory_view_math.hpp/.cpp` — 中文属性标签和详情双列布局。
- Modify: `src/platform/raylib/inventory_renderer.cpp` — 背包/装备/合成/强化中文绘制。
- Modify: `src/platform/raylib/material_bag_renderer.hpp/.cpp` — 材料名、数量与强化弹窗。
- Modify: `src/platform/raylib/ground_loot_view.cpp` — 地面装备中文名和品质。
- Modify: `src/platform/raylib/passive_tree_view_math.hpp/.cpp` — 中文状态和操作文案。
- Modify: `src/platform/raylib/passive_tree_renderer.hpp/.cpp` — 共享字体、节点短名、tooltip 和布局。
- Modify: `src/platform/raylib/combat_view_math.hpp/.cpp` — 中文怪物角色和词缀短标。
- Modify: `src/platform/raylib/actor_renderer.cpp` — 共享字体绘制中文战斗标签。
- Modify: `src/platform/raylib/death_overlay_view.cpp` — 统一中文名称映射并移除英文玩家回退。
- Modify: 对应 inventory/material/passive/monster/death/hud font 测试。

---

### Task 1: 完整领域展示映射

**Files:**
- Modify: `src/platform/raylib/ui_text_catalog.hpp`
- Modify: `src/platform/raylib/ui_text_catalog.cpp`
- Modify: `tests/platform/ui_text_catalog_tests.cpp`
- Modify: `tests/platform/shared_ui_font_tests.cpp`

**Interfaces:**
- Consumes: 18 个 `base_id`、14 个 `MaterialId`、6 个 `ItemSlot`、3 个品质、64 个被动节点、8 个怪物和全部怪物词缀。
- Produces: 每个稳定值唯一的中文展示字符串，未知值统一为“未知”。

- [ ] **Step 1: 扩展目录失败测试**

用固定数组逐项断言完整映射；至少包含：

```cpp
ARPG_REQUIRE(std::strcmp(platform::ui_item_base_name(18U),
    u8"混沌之眼") == 0);
ARPG_REQUIRE(std::strcmp(platform::ui_item_slot_name(
    arpg::items::ItemSlot::weapon), u8"武器") == 0);
ARPG_REQUIRE(std::strcmp(platform::ui_item_rarity_name(
    arpg::items::ItemRarity::rare), u8"稀有") == 0);
ARPG_REQUIRE(std::strcmp(platform::ui_passive_node_name(0U),
    u8"起源") == 0);
ARPG_REQUIRE(std::strcmp(platform::ui_monster_name(
    arpg::combat::MonsterId::chaos_chaser), u8"混沌追猎者") == 0);
```

对每个词缀短标计算 UTF-8 码点数并断言 `<= 2`；目录访问器必须覆盖新字符串，统一字形计划继续不过 640。

- [ ] **Step 2: 运行并确认 RED**

Run: foundation 定向入口。

Expected: 未补齐的稳定值返回“未知”或旧英文，测试失败。

- [ ] **Step 3: 补齐静态映射**

装备采用规格中的 18 个中文名；材料采用嬗变石、增幅石、富豪石、混沌石、崇高石、剥离石、神圣石、重铸石、定向核心、强化石和四档强化券；被动节点按 64 个 ID 建立短展示名；怪物和 12 个词缀使用规格表。

- [ ] **Step 4: 运行目录/字形测试并提交**

```powershell
$env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY -ErrorAction SilentlyContinue }
git add src/platform/raylib/ui_text_catalog.hpp src/platform/raylib/ui_text_catalog.cpp tests/platform/ui_text_catalog_tests.cpp tests/platform/shared_ui_font_tests.cpp
git diff --cached --check
git commit -m "feat: localize gameplay display names"
```

---

### Task 2: 中文背包属性与详情布局

**Files:**
- Modify: `src/platform/raylib/inventory_view_math.hpp`
- Modify: `src/platform/raylib/inventory_view_math.cpp`
- Modify: `src/platform/raylib/inventory_renderer.cpp`
- Modify: `tests/platform/inventory_view_math_tests.cpp`
- Modify: `tests/platform/hud_font_tests.cpp`

**Interfaces:**
- Consumes: 中文底材/槽位/品质/属性映射和共享 `Font`。
- Produces: `InventoryDetailRowLayout`、中文详情行和按钮/页签居中绘制。

- [ ] **Step 1: 写中文属性与双列布局失败测试**

```cpp
const auto row = platform::inventory_detail_row_layout(layout, 0U);
ARPG_REQUIRE(row.label.x >= layout.detail.x);
ARPG_REQUIRE(row.label.x + row.label.width <= row.value.x);
ARPG_REQUIRE(arpg::test::near(row.value.x + row.value.width,
    layout.detail.x + layout.detail.width - layout.content_padding));
ARPG_REQUIRE(std::strcmp(platform::item_attribute_label(
    test_effect).bytes.data(), u8"火焰伤害") == 0);
```

分别在 800×450、1280×720、1920×1080 验证详情矩形不相交、不越界。更新英文 `LOCAL/GLOBAL/Pre/Suf/Base/inc/more/converted` 期望为“局部/全局/前缀/后缀/基础属性/提高/更多/转化”。

- [ ] **Step 2: 运行并确认 RED**

Expected: `InventoryDetailRowLayout` 不存在，属性标签仍为英文。

- [ ] **Step 3: 实现纯布局和中文属性模型**

头文件增加：

```cpp
struct InventoryDetailRowLayout final {
    Rectangle label{};
    Rectangle value{};
};

[[nodiscard]] InventoryDetailRowLayout inventory_detail_row_layout(
    const InventoryLayout& layout, std::size_t line) noexcept;
```

属性行输出“基础属性 [局部] 武器物理伤害 +16”“T3 前缀 [全局] 火焰伤害 +12%”等中文结构。计算逻辑仍使用原 effect/stat/operation，只有展示函数改变。

- [ ] **Step 4: 汉化 renderer 并按交互语义对齐**

替换 `EQUIPMENT/INVENTORY/ITEM DETAIL/Slot/Rarity/Combine/No items match`、保存锁、合成和强化结果。标题、页签、筛选、合成按钮居中；装备名在图标后的剩余区域居中；详情标签左、数值右。长文本用 `MeasureTextEx` 和 `ui_utf8_fit_with_ellipsis()`，常规语料要求不截断。

- [ ] **Step 5: 运行定向测试并提交**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY -ErrorAction SilentlyContinue }
git add src/platform/raylib/inventory_view_math.hpp src/platform/raylib/inventory_view_math.cpp src/platform/raylib/inventory_renderer.cpp tests/platform/inventory_view_math_tests.cpp tests/platform/hud_font_tests.cpp tests/platform/platform_test_main.cpp
git diff --cached --check
git commit -m "feat: localize inventory detail layout"
```

---

### Task 3: 材料袋、合成和强化弹窗

**Files:**
- Modify: `src/platform/raylib/material_bag_renderer.hpp`
- Modify: `src/platform/raylib/material_bag_renderer.cpp`
- Modify: `src/platform/raylib/ground_loot_view.cpp`
- Modify: `tests/platform/material_bag_renderer_tests.cpp`
- Modify: `tests/platform/ground_loot_view_tests.cpp`
- Modify: `tests/platform/stage12_material_formal_game_validation.cpp`

**Interfaces:**
- Consumes: `ui_material_name()`、`ui_item_base_name()`、`ui_item_rarity_name()`。
- Produces: 中文材料格、地面装备标签和居中危险确认框。

- [ ] **Step 1: 写现有漏翻路径失败测试**

断言 14 个材料格名称，无 `Reinf. Stone` 或 `* Core`；地面标签为“普通 暗影兜帽 · 物品等级 1”；强化弹窗包含“警告：失败将摧毁装备”“使用一颗强化石？”“确认”“取消”。

- [ ] **Step 2: 运行并确认 RED**

Expected: 当前材料特判和弹窗英文导致失败。

- [ ] **Step 3: 实现中文材料与居中弹窗**

删除 renderer 本地英文名称分支，全部调用目录。材料格的图标保持左侧，名称与 `x数量` 在余下矩形内居中；弹窗标题、说明和两个按钮分别相对自己的矩形居中，并使用警告/危险/普通高对比色。

- [ ] **Step 4: 运行测试并提交**

```powershell
$env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY -ErrorAction SilentlyContinue }
git add src/platform/raylib/material_bag_renderer.hpp src/platform/raylib/material_bag_renderer.cpp src/platform/raylib/ground_loot_view.cpp tests/platform/material_bag_renderer_tests.cpp tests/platform/ground_loot_view_tests.cpp tests/platform/stage12_material_formal_game_validation.cpp
git diff --cached --check
git commit -m "feat: localize materials and crafting dialogs"
```

---

### Task 4: 被动树中文字体、节点名和 tooltip

**Files:**
- Modify: `src/platform/raylib/passive_tree_view_math.hpp`
- Modify: `src/platform/raylib/passive_tree_view_math.cpp`
- Modify: `src/platform/raylib/passive_tree_renderer.hpp`
- Modify: `src/platform/raylib/passive_tree_renderer.cpp`
- Modify: `tests/platform/passive_tree_view_tests.cpp`
- Modify: `tests/platform/hud_font_tests.cpp`
- Modify: `tests/passives/passive_tree_catalog_tests.cpp`

**Interfaces:**
- Consumes: 64 个中文节点展示名、共享字体、现有核心规则结果。
- Produces: 中文短节点名、完整 tooltip、中文操作/保存状态和字体测量路径。

- [ ] **Step 1: 写中文和规则状态失败测试**

断言：

- 核心 `PassiveNode::name` 仍为原英文 key。
- 展示目录的 64 个节点名均为合法中文 UTF-8。
- `not_adjacent/disconnects_tree/no_points/save_pending/save_error` 分别给出准确中文。
- 可分配、已分配、锁定、保存中、错误状态同时有文字和非颜色标识。
- 节点短名相对圆心居中，tooltip 在屏幕安全区。

- [ ] **Step 2: 运行并确认 RED**

Expected: renderer 仍用 `DrawText(source.name)`，状态/类型/关闭提示仍有英文。

- [ ] **Step 3: 实现中文 View 和统一字体路径**

所有可见字符串通过 `ui_text_catalog`；所有 `DrawText/MeasureText` 改成共享 `Font` 的 `draw_crisp_ui_text/MeasureTextEx`。节点图只显示短名或短收益，完整收益/代价/操作放 tooltip。标题“被动星图”居中，点数左对齐，`P 关闭`右对齐。

- [ ] **Step 4: 更新字形和文本边界测试**

`hud_font_tests` 遍历 64 个名称和所有 tooltip 语料；`ui_text_bounds_audit` 增加被动树页面/角色，验证 800×450、1280×720、1920×1080 无越界。

- [ ] **Step 5: 运行测试并提交**

```powershell
$env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY -ErrorAction SilentlyContinue }
cmake --build --preset windows-msvc-debug --target arpg_passive_tests --parallel 2
& '.\out\build\windows-msvc-debug\bin\arpg_passive_tests.exe'
git add src/platform/raylib/passive_tree_view_math.hpp src/platform/raylib/passive_tree_view_math.cpp src/platform/raylib/passive_tree_renderer.hpp src/platform/raylib/passive_tree_renderer.cpp tests/platform/passive_tree_view_tests.cpp tests/platform/hud_font_tests.cpp tests/passives/passive_tree_catalog_tests.cpp
git diff --cached --check
git commit -m "feat: localize passive star chart"
```

---

### Task 5: 怪物、词缀和战斗标签

**Files:**
- Modify: `src/platform/raylib/combat_view_math.hpp`
- Modify: `src/platform/raylib/combat_view_math.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `tests/platform/monster_view_tests.cpp`
- Modify: `tests/platform/combat_view_math_tests.cpp`

**Interfaces:**
- Consumes: `ui_monster_name()`、完整/短词缀名、共享字体。
- Produces: 中文怪物角色、阶段、词缀徽记和“已击败”反馈。

- [ ] **Step 1: 写 8 怪物和 12 词缀失败测试**

逐个断言名称；短标严格为“强、狂、迅、甲、盾、多射、灼地、寒、连雷、蚀、闪袭、死爆”，长度一至两个汉字。保留原危险级、颜色、脉冲和词缀效果断言。

- [ ] **Step 2: 运行并确认 RED**

Expected: 当前 `BOMBER/CHARGER/MGT/MULTI/DEFEATED` 等英文断言失败。

- [ ] **Step 3: 接入展示目录和共享字体**

`MonsterVisual` 不再读取核心英文展示名；actor renderer 用共享字体绘制角色、阶段和词缀短标。头顶短标不得超过血条安全宽度，正式完整名只放死亡详情。

- [ ] **Step 4: 运行测试并提交**

```powershell
$env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY -ErrorAction SilentlyContinue }
git add src/platform/raylib/combat_view_math.hpp src/platform/raylib/combat_view_math.cpp src/platform/raylib/actor_renderer.cpp tests/platform/monster_view_tests.cpp tests/platform/combat_view_math_tests.cpp
git diff --cached --check
git commit -m "feat: localize monster combat labels"
```

---

### Task 6: 死亡结算统一中文归因

**Files:**
- Modify: `src/platform/raylib/death_overlay_view.cpp`
- Modify: `src/platform/raylib/death_overlay_renderer.hpp`
- Modify: `src/platform/raylib/death_overlay_renderer.cpp`
- Modify: `tests/platform/death_overlay_view_tests.cpp`
- Modify: `tests/platform/hud_font_tests.cpp`

**Interfaces:**
- Consumes: 怪物、词缀、元素、危险区中文映射和共享字体。
- Produces: 完整中文死亡归因；字体失败不再进入英文 ASCII 玩家页面。

- [ ] **Step 1: 写统一目录和无英文回退失败测试**

保留现有所有死亡来源覆盖，同时断言怪物/词缀/元素来自统一目录。把“字体不可用时生成 `DEATH RECAP`”的旧期望改成“返回资源不可用状态且不生成英文玩家文案”。

- [ ] **Step 2: 运行并确认 RED**

Expected: 局部英文映射或 `build_death_overlay_ascii_view()` 仍被正式 renderer 使用。

- [ ] **Step 3: 统一映射并删除玩家英文回退路径**

保留 ASCII helper 仅供内部诊断测试或完全移除其正式调用；`DeathOverlayRenderer` 在共享字体未就绪时返回不可绘制状态，由宿主字体初始化门禁阻止进入游戏。正常路径全部使用中文 View。

- [ ] **Step 4: 运行 death/hud font 测试并提交**

```powershell
$env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY -ErrorAction SilentlyContinue }
git add src/platform/raylib/death_overlay_view.cpp src/platform/raylib/death_overlay_renderer.hpp src/platform/raylib/death_overlay_renderer.cpp tests/platform/death_overlay_view_tests.cpp tests/platform/hud_font_tests.cpp
git diff --cached --check
git commit -m "feat: unify Chinese death recap labels"
```

---

## Plan Completion Gate

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_passive_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe'; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE } } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY -ErrorAction SilentlyContinue }
& '.\out\build\windows-msvc-debug\bin\arpg_passive_tests.exe'
git diff --check
```

Expected: 背包/成长定向 suite 与核心被动测试全部通过；核心英文 key、规则和 wire 未变化。随后进入 `2026-08-09-full-chinese-ui-release-validation.md`。
