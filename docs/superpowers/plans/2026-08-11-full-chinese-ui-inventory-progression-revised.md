# 背包与成长界面全汉化修订实施计划

> 执行方式：逐 Task 严格 RED → GREEN → focused verification → scoped commit → 独立审查。任何 Task 未通过不得提前进入下一项。

## 目标

完成玩家可达的背包、装备详情、技能石、地面装备、拾取反馈、材料袋、强化确认、被动树和死亡结算汉化；按钮、页签和确认项在各自完整交互矩形内水平/垂直居中；正文与不透明背板对比度至少 4.5:1；属性表保持标签左、数值右。

## 当前基线与不重做范围

- 基线提交：`67e5b84`（已包含 Task7 与菜单/HUD 收尾）。
- `ui_text_catalog` 已有 18 个底材、14 个材料、6 个槽位、3 个品质、24 个物品词缀、64 个被动节点、8 个怪物、12 个怪物词缀和 2 个主动技能映射；本计划只补新语料和迁移真实消费者，不重写稳定表。
- 菜单/HUD、世界洞口/深渊提示、F1、怪物头顶词缀短标、游戏标题和启动器已完成并由独立门禁覆盖。
- 怪物头顶只允许显示 12 个词缀短标；怪物职业完整中文名只在 F1 和死亡归因显示。本计划不得重新引入职业头顶标签。
- 正式截图 validator、Release 构建、部署和多分辨率实机截图属于 `2026-08-09-full-chinese-ui-release-validation.md`；本计划只提供纯布局、色彩、固定缓冲测试和既有 formal target 的 compile/link 证据。

## 硬约束

- 不修改稳定 ID、核心数值、存档格式、wire、掉落/强化/被动规则。
- C++17、raylib 6.0；所有中文使用唯一 `SharedUiFont`，不得 `GetFontDefault`、`DrawText` 或第二次 `LoadFontEx` 绘制玩家中文。
- 玩家可见文本使用固定容量 UTF-8 缓冲；每帧更新、布局和绘制不得堆分配。
- 标题/页签/按钮/确认项居中；装备属性采用标签左、数值右；长文本只能按 UTF-8 码点缩小或省略，不能切断多字节字符。
- 背板必须 `alpha=255`；正文/辅助/警告/错误/禁用态对实际背板均需 `contrast >= 4.5`，且状态不能只靠颜色区分。
- 材料名称统一为“嬗变石”。迁移最后一个旧消费者后，从共享字形语料删除不可达“蜕变石”，不得同时保留两种写法。
- 材料拾取聚合缓冲维持 96 bytes。只追加完整条目；剩余材料放不下时输出完整尾缀“、等N种”，绝不留下半个 UTF-8 字符或半条材料。
- `kUiGlyphCapacity` 保持 640。每次新增语料后读取真实 `ui_glyph_plan().codepoint_count`；若超过 640，立即停止该 Task。先去重/删除不可达字形，未经单图集/32MiB 实测证据不得扩容。
- 为保持真实计划在 `639/640` 且不牺牲含义，玩家实际展示词汇锁定为：“装备值”“按键提示”“小节点”“重要节点”“地面危险”“来源”；`ModifierOperation::more` 不显示含糊的“更多”，而按实际乘算结果显示“最终<中文数值名> ×N.NN”（例如“最终伤害 ×1.12”）。文档其余位置的“属性/操作/类型”等仅是工程概念，不得原样进入玩家 UI。
- Windows 构建并行度最大 `--parallel 2`；不得并发运行共享 MSVC target。
- T0 的 `ui_text_layout_suite` 必须同时加入专用 selector 与默认 full suites；T1–T5 只在既有 `TestCase` 内追加断言，不新增 platform case。新增 architecture guard/self-test 作为独立 CTest，不计入 120/675；否则必须先重新审计并显式更新两个精确计数，禁止静默漂移。

## 依赖图

`T0 选择器/纯布局基线 → T1 字形语料 → T2 背包+技能石 → T3 地面掉落+材料+强化 → T4 被动树 → T5 死亡页 → T6 总门禁与视觉交接`

---

## T0：建立背包成长专用选择器与通用纯布局契约

**修改文件**

- 新增 `src/platform/raylib/ui_text_layout.hpp`
- 新增 `src/platform/raylib/ui_text_layout.cpp`
- 新增 `tests/platform/ui_text_layout_tests.cpp`
- 新增 `tests/platform/full_chinese_inventory_selector.ps1`
- 新增 `tests/platform/full_chinese_inventory_selector_self_test.ps1`
- 修改 `src/platform/raylib/CMakeLists.txt`
- 修改 `tests/platform/CMakeLists.txt`
- `tests/platform/platform_test_main.cpp`

**目标接口**

- 新环境选择器：`ARPG_FULL_CHINESE_UI_INVENTORY_ONLY`。
- 初始 suite 包含 `ui_text_catalog`、`ui_utf8`、`shared_ui_font`、`inventory_view_math`、`active_skill_loadout_view`、`ground_loot_view`、`loot_pickup_feedback`、`material_loot_view`、`material_bag_renderer`、`passive_tree_view`、`death_overlay_view`、`hud_font`；当前合计 115 cases。
- 新增 5 个 `ui_text_layout` cases 后，selector 必须唯一输出 `120 cases, 0 failures`。
- 新增固定容量 `UiCenteredTextLayout fit_centered_ui_text(...) noexcept`，输入调用方缓冲区、字体度量和完整交互矩形，输出最终 UTF-8 文本、字号、spacing 与水平/垂直居中位置。
- 新增 `UiLabelValueRowLayout ui_label_value_row_layout(...) noexcept`，输出左标签区与右值区；两区有固定 gap、互不重叠。
- `full_chinese_inventory_selector.ps1` 必须快照并移除全部 `^ARPG_.*_ONLY$` 环境变量、只设置本 selector、捕获进程输出与退出码、只接受唯一精确的 `120 cases, 0 failures`，并在成功或异常时都恢复原环境。纯 PowerShell self-test 用内存 fake runner 覆盖非零退出、错误/重复摘要和环境恢复。

**TDD**

1. 先写 selector 与 5 个 case：三分辨率居中/containment、中文省略不切 UTF-8、左标签/右值无重叠、100,000 次零分配、空缓冲/零容量/NaN/Inf/不可容纳矩形安全失败且不返回可绘制布局。
2. 未实现选择器/接口时构建必须 RED；记录首个目标缺失或 selector 未命中证据。
3. 最小实现后定向入口唯一输出 `120 cases, 0 failures`，100,000 次布局 allocation delta 为 0。
4. 不修改任何玩家可见字符串。
5. 新增 5 cases 时，默认 full expected 只从 670 调整为 675；当前 actual 预期从 671 变为 676，继续保留既有 +1 漂移，禁止追平 actual。

**验证**

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
pwsh -NoProfile -File .\tests\platform\full_chinese_inventory_selector_self_test.ps1
pwsh -NoProfile -File .\tests\platform\full_chinese_inventory_selector.ps1 -ExecutablePath .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

---

## T1：补齐本阶段唯一字形语料并锁住 640 容量

**修改文件**

- `src/platform/raylib/ui_text_catalog.cpp`
- `tests/platform/ui_text_catalog_tests.cpp`
- `tests/platform/shared_ui_font_tests.cpp`

**范围**

- 保留现有稳定映射字面量，不重命名已确认的 18/14/6/3/24/64/8/12/2 表。
- 唯一批准的映射收口：第二个主动技能从临时“风暴剑阵”改为现有游戏展示名“极·鬼剑术（暴风式）”，T2 的真实 consumer 必须读取该目录值。
- 将 T2–T5 的真实中文格式样例纳入唯一 catalog visitor，例如详情装备值、合成/强化结果、材料聚合尾缀、被动按键提示/保存状态、死亡归因，以及上述容量兼容词汇与“最终伤害 ×1.12”。
- 本 Task 暂不移除“蜕”；删除动作在 T3 迁移 `material_loot_view` 后与消费者同提交完成。

**TDD**

1. 测试先遍历本阶段批准语料，要求合法 UTF-8、目录 visitor 可达、共享计划覆盖。
2. 断言 `!plan.capacity_exceeded`、`plan.codepoint_count <= 640`、单实例字体图集预算保持既有上限。
3. 用临时第 641 个唯一字符 mutation 证明 overflow 门禁会 RED，恢复后 GREEN。

**停止条件**

- 若真实 count > 640，不实现消费者、不静默增容；报告新增/可删码点和单图集预算，再请求单独规格决策。

---

## T2：背包、装备详情、筛选、合成与技能石页面

**修改文件**

- `src/platform/raylib/inventory_view_math.hpp/.cpp`
- `src/platform/raylib/inventory_renderer.cpp`
- `tests/platform/inventory_view_math_tests.cpp`
- `tests/platform/active_skill_loadout_view_tests.cpp`
- `tests/platform/hud_font_tests.cpp`
- 新增 `tests/platform/inventory_renderer_source_test.cmake`
- 新增 `tests/platform/inventory_renderer_source_self_test.cmake`
- 修改 `tests/platform/CMakeLists.txt`，注册精确 CTest 名 `platform.full_chinese_inventory_renderer_source` 与 `platform.full_chinese_inventory_renderer_source_self_test`

**必须迁移的真实消费者**

- `inventory_page_title()` 的 `EQUIPMENT INVENTORY` 和 `Unknown`。
- `copy_skill_name()` 从核心英文 `display_name` 改为 `ui_skill_name()`。
- `stat_name()` 与 tooltip 中 `LOCAL/GLOBAL`、装备值、按键提示、前/后缀、基础/提高/转化；`more` 严格按“最终<中文数值名> ×N.NN”显示，不出现英文或“更多”。
- `inventory_renderer.cpp` 的三栏标题、生命/护盾/防御统计、槽位/品质筛选、空状态、合成、保存锁、强化结果、物品等级、完整构筑差异。
- 物品名称、槽位、品质、词缀统一从 `ui_text_catalog` 读取，不再读取核心英文 `BaseDefinition::name` 作为玩家文案。

**布局规则**

- 背包总标题、页签、筛选按钮、合成按钮、技能石“取出”按钮在完整按钮矩形中居中。
- 装备/技能石条目保留图标在左；名称在扣除图标后的余下内容区居中。
- 详情区新增纯 `InventoryDetailRowLayout`：标签列左对齐、数值列右对齐、两列不重叠；不强行把数据表居中。
- 常规批准语料不得省略；极端输入只能通过 T0 的 UTF-8 安全 fit 路径。

**RED 用例**

- 逐项扫描真实 View/renderer 输出，无 `EQUIPMENT/INVENTORY/ITEM DETAIL/Slot/Rarity/Combine/LOCAL/GLOBAL/Pre/Suf/iLvl`。
- 三分辨率按钮 X/Y 居中、详情双列无重叠、完整背板对比度 >=4.5。
- 两个主动技能名均为目录中文；空槽/辅助槽/未装备/保存中无英文。
- source guard 拒绝核心英文展示名回流、默认字体、只包文字宽度的黑底和 dead-branch 中文诱饵。

**GREEN 验证**

- 专用 selector精确 `120/120`。
- `ctest --preset windows-msvc-debug --output-on-failure --no-tests=error -R '^platform\.full_chinese_inventory_renderer_source$'` 与对应 `_self_test` 精确项均为 `1/1`。
- `ARPG_CPLAY022_SKILL_EMPTY_SLOT_ONLY=1` 与 `ARPG_CPLAY027_INVENTORY_BINDING_LABEL_ONLY=1` 保持原行为。
- build `arpg_stage17_skill_stones_game_validation --parallel 2`，只编译/链接，不运行 formal 窗口。

---

## T3：地面装备、拾取反馈、材料聚合、材料袋与强化弹窗

**修改文件**

- `src/platform/raylib/ground_loot_view.cpp`
- `src/platform/raylib/loot_pickup_feedback.cpp`
- `src/platform/raylib/material_loot_view.cpp`
- `src/platform/raylib/material_bag_renderer.cpp`
- `src/platform/raylib/inventory_renderer.cpp`（仅强化/材料交互文案）
- `src/platform/raylib/hud_renderer.cpp`（仅 `HudRenderer::draw_ground_loot` 与 `HudRenderer::draw_material_loot`）
- `src/platform/raylib/ui_text_catalog.cpp`（删除不可达“蜕”）
- `tests/platform/ground_loot_view_tests.cpp`
- `tests/platform/loot_pickup_feedback_tests.cpp`
- `tests/platform/material_loot_view_tests.cpp`
- `tests/platform/material_bag_renderer_tests.cpp`
- `tests/platform/shared_ui_font_tests.cpp`
- `tests/platform/hud_font_tests.cpp`
- 新增 `tests/platform/ground_loot_renderer_source_test.cmake`
- 新增 `tests/platform/ground_loot_renderer_source_self_test.cmake`
- 修改 `tests/platform/CMakeLists.txt`，注册精确 CTest 名 `platform.full_chinese_ground_loot_renderer_source` 与 `platform.full_chinese_ground_loot_renderer_source_self_test`

**必须迁移的真实消费者**

- 地面装备与拾取反馈的品质/底材改用 `ui_item_rarity_name/ui_item_base_name`，`iN` 改成“物品等级 N”。
- `HudRenderer::draw_ground_loot/draw_material_loot` 必须借用 `SharedUiFont`，按 T0 helper 在完整标签背板内居中；移除这两个方法的 `GetFontDefault` fallback、半透明 `Fade` 背板与固定 `x+8/y+3` 左贴布局。背板 alpha 必须为 255，实际文字色对比度 >=4.5。
- `material_label()` 改为 `ui_material_name()`；“蜕变石”从生产代码和动态字形覆盖同时消失，只剩“嬗变石”。
- 材料袋 `MATERIAL BAG/Reinf. Stone/Invalid/* Core/DANGER/CONFIRM/CANCEL` 全部中文；定向核心分类为“伤害核心/防御核心/速度核心/元素核心”。
- 强化确认与结果必须中文，危险/成功/失败同时用文案和边框表达。

**96-byte 聚合算法**

- 使用固定数组和 `ui_utf8` 追加完整 token：`已拾取：<材料> xN`。
- 在追加前预计算完整 token 字节数；放不下时统计剩余非零材料种类，尝试完整追加 `、等N种` 后停止。
- 任意结果都必须 NUL 结尾、合法 UTF-8，`truncated=false`；绝不先写半条再回退。

**RED 用例**

- 14 个材料地面标签/材料格均精确等于目录名，无“蜕”。
- 1、2、14 种材料聚合；恰好边界、差 1 byte、长数量；期望完整条目或“、等N种”，永不产生半字符。
- 地面装备、拾取 HUD、材料标签使用同一目录并保持原 ID/ordinal/颜色/吸入动画行为。
- 800/1280/1920 的材料格文本在图标余下区域居中；弹窗标题、问题、确认/取消在各自矩形居中；所有背板不透明且 >=4.5。
- 10,000 次聚合与布局零分配。

**GREEN 验证**

- 专用 selector精确 `120/120`，Foundation、CPLAY021 吸入动画入口全绿。
- `platform.full_chinese_ground_loot_renderer_source` 与对应 `_self_test` 精确 CTest 均为 `1/1`；守卫限定上述两个 HUD 方法，明确不得改动 Task7 怪物标签/F1 路径。
- 因本 Task 再次修改 T2 已受保护的 `inventory_renderer.cpp`，必须复跑 `platform.full_chinese_inventory_renderer_source` 与对应 `_self_test`，两项均使用 exact anchor、`--no-tests=error --parallel 1` 且各为 `1/1`。
- build `arpg_stage12_material_formal`、`arpg_stage16_loot_reinforcement_game_validation`，只编译/链接。

---

## T4：被动星图节点、收益/代价、tooltip 与保存反馈

**修改文件**

- `src/platform/raylib/passive_tree_view_math.hpp/.cpp`
- `src/platform/raylib/passive_tree_renderer.cpp`
- `tests/platform/passive_tree_view_tests.cpp`
- `tests/platform/hud_font_tests.cpp`
- `tests/platform/passive_tree_binding_label_source_test.cmake`
- `tests/platform/passive_tree_feedback_source_test.cmake`
- 必要时新增 renderer shared-font/布局 source guard 与 self-test

**规则边界**

- 核心 `passives::PassiveNode::name` 继续是英文稳定 key；只允许展示层调用 `ui_passive_node_name()`。
- 不改 allocate/refund/save 规则，不改变 CPLAY034 的准确断链反馈优先级。

**必须迁移**

- 节点类型：起始/连接/小节点/重要节点/核心。
- 64 个节点名、所有收益/代价中的元素/属性、标题、点数、关闭提示、保存中/就绪/错误、分配/退款/相邻/断链等操作文案。
- 移除玩家路径 `DrawText/MeasureText` 默认字体 fallback；共享字体不可用时不绘制玩家文字，由宿主共享字体初始化门禁负责阻止正常进入。

**布局与 RED 用例**

- 标题“被动星图”顶部居中；未分配点左；动态绑定“P 关闭”右。
- 节点短名以节点圆心居中；tooltip 标题/类型/收益/代价/操作按层级排布，800/1280/1920 均在安全区。
- available/allocated/pending/locked 同时有中文状态/边框或图形差异，不只靠颜色。
- 64 名称与所有 tooltip 语料合法 UTF-8、共享字形覆盖、10,000 次 View/Layout 零分配。
- guard 拒绝 `source.name`、默认字体、英文状态、错误优先级反转和 dead shared-font decoy。

**GREEN 验证**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_passive_tests --parallel 2
pwsh -NoProfile -File .\tests\platform\full_chinese_inventory_selector.ps1 -ExecutablePath .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
$passiveOutput = @(& '.\out\build\windows-msvc-debug\bin\arpg_passive_tests.exe' 2>&1)
$passiveExit = $LASTEXITCODE
$passiveSummaryCount = @($passiveOutput | Where-Object { $_ -ceq '12 cases, 0 failures' }).Count
if ($passiveExit -ne 0 -or $passiveSummaryCount -ne 1) {
    $passiveOutput | ForEach-Object { Write-Host $_ }
    throw "passive tests failed or did not emit one exact 12/12 summary (exit $passiveExit)"
}
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error --parallel 1 -R '^platform\.passive_tree_binding_label_source$'
if ($LASTEXITCODE -ne 0) { throw 'passive binding source guard failed' }
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error --parallel 1 -R '^platform\.passive_tree_feedback_source$'
if ($LASTEXITCODE -ne 0) { throw 'passive feedback source guard failed' }
```

并复跑 CPLAY015、CPLAY033、CPLAY034 精确入口；`arpg_passive_tests.exe` 保持 `12/12`。

---

## T5：死亡结算统一中文归因且移除正式英文回退

**修改文件**

- `src/platform/raylib/dungeon_view_math.hpp/.cpp`
- `src/platform/raylib/death_overlay_view.cpp/.hpp`
- `src/platform/raylib/death_overlay_renderer.cpp`
- `tests/platform/death_overlay_view_tests.cpp`
- `tests/platform/hud_font_tests.cpp`
- 新增 `tests/platform/death_overlay_renderer_source_test.cmake`
- 新增 `tests/platform/death_overlay_renderer_source_self_test.cmake`
- 修改 `tests/platform/CMakeLists.txt`，注册精确 CTest 名 `platform.full_chinese_death_overlay_renderer_source` 与 `platform.full_chinese_death_overlay_renderer_source_self_test`

**必须迁移**

- 元素、伤害类型、怪物、地面危险、怪物词缀、深渊规则、来源、最近伤害、防御、返回入口和保存提示统一中文。
- 怪物与词缀从 `ui_text_catalog` 读取；将当前 `dungeon_view_math.cpp` 匿名命名空间内的 `abyss_rule_label(abyss::AbyssRuleId)` 提升为 `dungeon_view_math.hpp` 的只读 `noexcept` 展示接口，死亡页复用该接口，不保留第二份规则名表。
- `DeathOverlayRenderer` 不得再在字体未就绪时调用 `build_death_overlay_ascii_view()`；字体不可用则 fail-closed 不绘制玩家文案。ASCII helper 若保留，只能用于技术诊断测试，source guard 必须证明 renderer 无调用。

**RED 用例**

- 穷举全部 death source/type/detail，正常 View 无英文玩家 token。
- 字体未就绪时 renderer 选择不可绘制，不出现 `DEATH RECAP/Source/Last hit/Save failed/E Continue`。
- 中文正常/深渊/未知/保存中/保存失败/继续动态绑定语料均在固定容量内、合法 UTF-8、字形覆盖。
- 800/1280/1920 两栏/全宽行不越界，背板不透明、正文/危险/错误 >=4.5。

**GREEN 验证**

- 专用 selector精确 `120/120`，Foundation、CPLAY032 全绿。
- `ctest --preset windows-msvc-debug --output-on-failure --no-tests=error -R '^platform\.full_chinese_death_overlay_renderer_source$'` 与对应 `_self_test` 精确项均为 `1/1`。
- 因本 Task 将深渊规则名提升为 `dungeon_view_math` 公共展示 API，必须运行既有 `tests/platform/full_chinese_menu_hud_completion_gate.ps1 -Preset windows-msvc-debug`；不得用不含 `dungeon_view_math_suite` 的 Foundation/CPLAY032 代替该回归。
- 不运行正式死亡窗口；留给 Release 视觉计划。

---

## T6：范围守卫、总门禁与 Release 视觉交接

**新增/修改文件**

- `tests/platform/CMakeLists.txt`
- `tests/platform/platform_test_main.cpp`
- 新增 `full_chinese_inventory_progression_source_test.cmake` 与 reversible self-test
- 新增 `full_chinese_inventory_progression_gate.ps1` 与纯 PowerShell self-test
- 更新本计划进度记录；不得修改 Release validator 的正式截图所有权

**source guard 必须证明**

- 玩家可达生产文件中不存在本计划列出的英文文字；技术日志、include、资源路径和物理键名使用逐文件精确 allowlist，不允许宽泛正则。
- 背包/材料/被动/死亡真实 renderer 消费目录中文、共享字体、居中/双列布局和不透明高对比背板；required token 不得藏在注释、`#if 0`、lambda 或常量 false 分支。
- 旧 `BaseDefinition::name/display_name/source.name` 不再进入玩家 UI；不得回退默认字体。
- 守卫 self-test 至少含：活动英文回流、中文 dead decoy、默认字体、文字宽背板、非居中按钮、低对比/透明背板、截断 UTF-8、材料“蜕”、死亡 ASCII 正式回流。

**聚合门禁**

- 清空并恢复所有 `ARPG_*_ONLY`，只启用目标 selector；验证唯一精确 case summary。
- 每个 CTest 名称逐项 exact anchor + `--no-tests=error --parallel 1`。
- build（全部 `--parallel 2`）：`arpg_platform_tests`、`arpg_passive_tests`、`arpg_stage12_material_formal`、`arpg_stage16_loot_reinforcement_game_validation`、`arpg_stage17_skill_stones_game_validation`；formal 仅 compile/link。
- 运行：专用 selector、Foundation、passives.units、所有新 guards/self-tests、相关 CPLAY 精确入口。
- 运行完整 `arpg_platform_tests.exe`；只允许明确记录的既有 ecology alpha-bounds 和 preserved +1 count 基线，任何第三项失败都阻断。
- 按 Windows 交付约定运行 `scripts/Test.ps1 -Preset windows-msvc-debug`；若耗时或环境失败，必须报告精确阶段，不能用 focused 结果替代完整自动化。
- `git diff --check` 与 `git diff --cached --check` 均须通过。

**交给 Release 视觉计划的验收页**

- 800×450、1280×720、1920×1080：背包空/有物品、筛选、详情长属性、技能石、地面装备/材料、材料聚合、“等N种”、材料袋、强化危险弹窗、被动 tooltip/保存错误、死亡正常/深渊/未知/保存错误。
- 每页同时验证：无英文、无方块、无截断/重叠、按钮/页签居中、属性表双列、背板不透明、对比度可读。

## 完成定义

- T0–T6 每项均有有效 RED、最小 GREEN、fresh verification、窄提交和独立审查。
- 专用聚合门禁 exit 0；完整平台仅保留已记录的两项既有失败。
- Release 计划完成 Windows Release 构建、部署和三分辨率真实截图后，才能向用户声明“全部汉化、布局合理、颜色对比合格”已完成。
