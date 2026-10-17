# 全中文 UI / 背包与成长系统实施前只读审计

审计日期：2026-08-10  
工作树：`E:\\game\\.worktrees\\loot-suction-animation`  
分支：`codex/full-chinese-ui`  
审计快照：`9e4215b2c50ff2ce6238ba0c38756c03f4bcce82`（`feat: localize game and launcher titles`）

## 1. 边界与结论

本次仅做静态审计，没有修改 tracked 文件，没有构建、运行测试或提交。本文件位于 `.superpowers/` 忽略目录，不属于产品源码。

结论：现有计划方向正确，但不能按原文直接执行。当前 HEAD 已经完成了稳定 ID 的大部分中文展示映射，并已完成怪物/词缀主要消费者接线；与此同时，计划遗漏了多个真正向玩家显示名称的消费者，Task 2 的示例接口与现有源码不匹配，Task 4/6 又依赖计划中没有定义的映射和审计接口。若按原任务边界实现，会出现“测试绿但仍有英文”、重复映射互相漂移、UTF-8 被截断、被动树小节点文字放不下，以及两个计划同时改同一审计文件的风险。

最高优先级问题：

1. 静态字形目录按当前 `ui_text_catalog.cpp` 的文本集合估算已经使用 **633 / 640** 个唯一码点，仅余 **7** 个；新增中文文案前必须先做码点增量门禁。
2. `ground_loot_view.cpp`、`loot_pickup_feedback.cpp`、`material_loot_view.cpp`、技能石视图/渲染器不在原计划的完整文件清单中，却是玩家实际看到名称和提示的路径。
3. 背包、材料袋、被动树、死亡界面仍存在 `GetFontDefault()` / `DrawText()` 或 ASCII 回退；这与“字体加载失败阻止进入游戏、绝不回退英文”的规格冲突。
4. 原计划中的 `ARPG_FULL_CHINESE_UI_INVENTORY_ONLY` 选择器当前不存在；Task 3—6 的验证命令不能直接成立。
5. 当前没有被动树正式截图入口；完整三分辨率视觉验收只能由 release-validation 计划完成，不能把单元测试或 1280×720 截图称为最终视觉通过。

## 2. 当前已经落地、因此计划已经陈旧的部分

### 2.1 稳定 ID 展示目录已存在

`src/platform/raylib/ui_text_catalog.hpp` / `.cpp` 当前已经提供：

- `ui_item_base_name(std::uint8_t)`：18 个物品基础类型；
- `ui_material_name(items::MaterialId)`：14 种材料；
- `ui_item_slot_name(items::ItemSlot)`：6 个槽位；
- `ui_item_rarity_name(items::ItemRarity)`：3 个稀有度；
- `ui_item_affix_name(std::uint16_t)`：24 个装备词缀（1—12、101—112）；
- `ui_passive_node_name(passives::PassiveNodeId)`：64 个被动节点；
- `ui_monster_name(combat::MonsterId)`：8 种怪物；
- `ui_monster_affix_name(...)` / `ui_monster_affix_short_name(...)`：12 种怪物词缀；
- `ui_setting_action_name(...)`、`ui_skill_name(...)`；
- `visit_ui_text_catalog(...)`：共享字形收集入口。

`tests/platform/ui_text_catalog_tests.cpp` 已覆盖枚举穷举、未知值中文回退、怪物词缀短标签 UTF-8/码点数不超过 2、无分配，以及目录访问数量 `285U`。因此原 Task 1 的“先写 RED、再实现所有映射”在当前 HEAD 不再会红，也可能覆盖已落地实现。Task 1 应改为“核验现有目录并仅补缺失映射”。

### 2.2 怪物/词缀主要接线已完成

`src/platform/raylib/combat_view_math.cpp` 已消费 `ui_monster_name` 和 `ui_monster_affix_short_name`；`src/platform/raylib/actor_renderer.cpp` 已用固定 `std::array<char, 64>` 与 `ui_utf8_format` 构造词缀文字，并使用共享字体。`tests/platform/monster_view_tests.cpp`、`tests/platform/combat_view_math_tests.cpp` 已按目录检查怪物名、短词缀和阶段文字。

所以原 Task 5 预期的 `BOMBER` / `CHARGER` 等 RED 断言也已陈旧。应把 Task 5 改成验证任务；只有在产品规格明确要求“血条旁也必须展示怪物职业短名”时，才补独立短名 API。

## 3. 展示映射接口：应该保留与应该补充的边界

核心层稳定 ID 和英文内部 `name` 不应改动。所有玩家可见中文继续集中在 platform 展示目录，避免死亡、材料掉落、拾取反馈各自维护 switch。

建议保留现有接口，并按真实消费者补充以下窄接口：

```cpp
// items/item_crafting.hpp
const char* ui_directed_category_name(items::DirectedCategory id) noexcept;

// 只有产品确认血条旁需要怪物职业短名时添加
const char* ui_monster_short_name(combat::MonsterId id) noexcept;

// 小节点使用短名/符号，完整名称只进 tooltip
const char* ui_passive_node_short_name(passives::PassiveNodeId id) noexcept;

// checkpoint / combat / abyss 的死亡结算展示映射
const char* ui_dungeon_element_name(checkpoint::DungeonElement id) noexcept;
const char* ui_death_damage_name(checkpoint::DeathDamageType id) noexcept;
const char* ui_hazard_name(combat::HazardKind id) noexcept;
const char* ui_abyss_rule_name(abyss::AbyssRuleId id) noexcept;
```

注意 `HazardKind` 同时存在于 `src/checkpoint/room_combat_checkpoint.hpp` 和 `src/combat/combat_types.hpp`；当前 `death_overlay_view.cpp` 将快照中的数值转为 `combat::HazardKind`。新增 API 时必须使用真实消费者所用的 `combat::HazardKind`，不要再引入第三套 ID。

还应明确：

- 未知 ID 的回退必须是中文且固定，不向玩家输出数字稳定 ID；
- 每个新增映射都必须进入 `visit_ui_text_catalog`，否则字体图集不会包含其字形；
- 被动节点“完整名”和“小节点短标”是两个显示用途，不能用一个 4—6 字字符串硬塞到节点圆中；
- 若怪物规格中的“头顶短标签”仅指词缀，则不新增 `ui_monster_short_name`，避免无需求扩展。

## 4. 原计划遗漏的真实玩家可见消费者

### 4.1 地面装备

文件：

- `src/platform/raylib/ground_loot_view.cpp`
- `src/platform/raylib/ground_loot_view.hpp`
- `tests/platform/ground_loot_view_tests.cpp`

现状：基础物品名来自核心英文 `base->name`；格式是 `%s %.*s · i%u`；容量不足时回退成 `%s #%u · i%u`，会把稳定 ID 显示给玩家。固定缓冲为 64 字节，使用原始 `snprintf`。

实施要求：改用 `ui_item_base_name`、`ui_item_rarity_name` 和 `ui_utf8_format`，把 `i` 改为“物品等级”；穷举 18 × 3 × 合法等级的最大长度；容量不足只能按完整 UTF-8 码点截断，并保留诊断状态。已有十万次构建的零分配测试必须保留。

### 4.2 拾取装备反馈

文件：

- `src/platform/raylib/loot_pickup_feedback.cpp`
- `src/platform/raylib/loot_pickup_feedback.hpp`
- `tests/platform/loot_pickup_feedback_tests.cpp`

现状：HUD 文案仍类似 `已拾取：稀有 Ward Coat · i24`，同样读取核心英文名称并用原始 `snprintf` 写入 `HudText96`。原计划完全未列此消费者。

实施要求：统一走目录和 UTF-8 格式化；保留快照不变性、十万次 unchanged-observation 零分配契约和固定容量。

### 4.3 材料地面物与材料拾取反馈

文件：

- `src/platform/raylib/material_loot_view.cpp`
- `src/platform/raylib/material_loot_view.hpp`
- `tests/platform/material_loot_view_tests.cpp`

现状：有一份独立的 `material_label(MaterialId)`，其“蜕变石”与目录的“嬗变石”不一致，强化券空格格式也不同；生命药提示包含规格禁止的 `HP`；多段拼接使用重复 `snprintf`，96 字节缓冲可能在 14 种材料聚合时截断半个中文码点。

实施要求：删除重复展示映射，消费 `ui_material_name`；将生命恢复改为全中文；规定明确的有界聚合策略，例如只加入完整条目，余项显示 `……等 N 种` 并置诊断位。若选择扩大固定缓冲，必须先给出字节上界和栈/对象内存增量，不能用动态字符串解决。

### 4.4 技能石与技能装配

相关文件：

- `src/platform/raylib/inventory_view_math.cpp` / `.hpp`
- `src/platform/raylib/inventory_renderer.cpp`
- `src/platform/raylib/active_skill_view.cpp` / `.hpp`
- `src/platform/raylib/active_skill_renderer.cpp` / `.hpp`
- `tests/platform/active_skill_loadout_view_tests.cpp`
- `tests/platform/active_skill_view_tests.cpp`
- `tests/platform/stage17_skill_stones_game_validation.cpp`

现状：`inventory_view_math.cpp::copy_skill_name` 读取核心 `active_skill_definition(id)->display_name` 并用 `snprintf`，未消费 `ui_skill_name`；`active_skill_renderer.cpp::fit_skill_label` 自己寻找 UTF-8 边界并使用 ASCII `...`；技能名在图标下仍左对齐而非居中。原计划目标写了技能石，但任务文件列表没有明确覆盖这些实际路径。

实施要求：先复核前一阶段菜单/HUD代理是否已经改动这些文件；若没有，明确归入背包任务。所有技能名走同一展示目录和 `ui_utf8_fit_with_ellipsis`，图标下名称用测量后的水平居中，不得同时由两个任务修改同一渲染器。

## 5. 背包与装备详情：当前英文、接口冲突和布局问题

### 5.1 `inventory_view_math.cpp`

主要英文残留：

- `stat_name` 中的 Knockback、Shield、Melee damage、各元素伤害/减伤/上限、Armor、Evasion、Max health、Barrier、Damage taken、Move/Attack/Jump/Air control；
- 标题回退 `Unknown` 和 `%s / ESC · EQUIPMENT INVENTORY`；
- 装备详情的 Weapon physical、Attack speed、All element DR、Fire/Water/Lightning DR、cap、`inc`、`more`、`converted`、`LOCAL`、`GLOBAL`。

原 Task 2 示例测试与当前 API 不匹配：

- `InventoryLayout` 只有 `equipment`、`grid`、`detail`、`scale`，不存在 `content_padding`；
- `item_attribute_label` 需要五个参数：`ItemEffectKind`、`StatId`、`ModifierOperation`、`ItemSlot`、`variant`；
- `ItemAttributeLabel` 只有 `name/scope/suffix/qualifier` 四个 `const char*`，不存在 `.bytes`。

计划必须先按实际接口重写测试，或先明确引入新的纯展示模型；不能照抄原示例。

### 5.2 `inventory_renderer.cpp`

仍有本地英文映射/文案：EQUIPMENT、INVENTORY、ITEM DETAIL、Empty、各属性缩写、Slot/Rarity/All、No match、Combine、保存/摧毁/成功/失败/强化结果，以及 iLvl、Required、Reinforcement、Base、Pre、Suf、FULL BUILD DIFFERENCE、Preview unavailable 等。基础物品名仍直接使用 `base->name`，装备词缀目录虽已存在却未被消费。

布局不符合规格：面板标题、页签、按钮都用固定左边距；属性详情也不是“标签左对齐、数值右对齐”的双列布局。`fit_inventory_text` 使用原始 `snprintf` 和 ASCII `...`，没有统一走 `ui_utf8`。

建议新增一个平台通用、纯计算的居中接口，而不是在每个 renderer 复制公式：

```cpp
struct UiCenteredTextLayout final {
    Vector2 position{};
    Rectangle bounds{};
    bool fits{};
};

UiCenteredTextLayout ui_centered_text_layout(
    Rectangle container, Vector2 measured) noexcept;
```

renderer 用共享字体 `MeasureTextEx` 得到 `measured`，纯函数同时做水平和垂直居中；单元测试用合成尺寸检查中心误差和边界。装备属性另建 `label_bounds` / `value_bounds`，测试标签左、值右且不重叠。

## 6. 材料袋、合成与强化

文件：

- `src/platform/raylib/material_bag_renderer.cpp` / `.hpp`
- `tests/platform/material_bag_renderer_tests.cpp`

现状：`material_bag_display_name` 仍有 `Reinf. Stone`、`Invalid` 并回退到核心英文材料定义；标题 `MATERIAL BAG`；定向类别是 Damage/Defense/Speed/Element Core；确认弹窗和按钮仍是英文。文字虽然垂直居中，但 X 坐标仍是固定左边距，标题和弹窗按钮也未按测量结果居中。字体不可用时会回退 `GetFontDefault()`；裁剪使用手写逻辑和 ASCII `...`。

当前测试只覆盖布局/选择/确认状态/资源行为，无法可靠断言 renderer 内嵌的即时文案。计划提出的 literal 测试需要先抽出纯 `ReinforcementConfirmationView` 或“render text plan”，然后测试其固定缓冲和布局；否则只能做脆弱的源码扫描。

## 7. 被动树

相关文件：

- `src/platform/raylib/passive_tree_view_math.cpp` / `.hpp`
- `src/platform/raylib/passive_tree_renderer.cpp` / `.hpp`
- `tests/platform/passive_tree_view_tests.cpp`
- `tests/passives/passive_tree_catalog_tests.cpp`（只可增加核心英文 key 不变的守护，不改核心展示语义）
- 若本阶段负责审计生产者：`src/platform/raylib/ui_text_bounds_audit.cpp` / `.hpp`、对应测试和 host 状态报告。

现状：动作/保存状态/错误理由、节点类型、标题、被动点数和关闭提示仍是英文；收益文案是中文前缀混英文属性名；tooltip 和节点名直接读取核心英文 `source.name` / `node.name`。渲染器大量使用默认 `DrawText` / `MeasureText`；辅助函数在共享字体不可用时仍回退默认字体，当前 `hud_font_tests.cpp` 甚至把这个 fallback 作为预期行为。被动树没有接入 `ui_text_bounds_audit`。

布局风险：800×450 时设计画布缩放约 0.592，小节点直径约 13 px，当前 4—6 个汉字的完整节点名不可能在可读字号下放入圆形节点。应使用 1 个字/图形符号的 `ui_passive_node_short_name`，完整名称只出现在 tooltip。

状态不能只靠颜色。建议纯展示模型同时暴露：

- 中文状态：可分配 / 已分配 / 锁定 / 保存中；
- 非颜色标记：虚线环 / 双环 / 锁标 / 保存旋转标等枚举；
- renderer 只消费该模型，测试每种状态都同时具有文字和形状标记。

原 Task 4 宣称扩展文字边界审计，却没有把 `ui_text_bounds_audit.*`、其测试、host 报告和正式验证器放入文件清单；release-validation 计划也声称负责这部分。必须指定唯一 owner，避免并行冲突。

## 8. 死亡结算

相关文件：

- `src/platform/raylib/death_overlay_view.cpp` / `.hpp`
- `src/platform/raylib/death_overlay_renderer.cpp` / `.hpp`
- `tests/platform/death_overlay_view_tests.cpp`
- 若本阶段负责审计生产者：`src/platform/raylib/ui_text_bounds_audit.cpp` / `.hpp` 及测试。

现状：死亡视图维护了生态、伤害、怪物、地面危险、词缀和深渊规则的本地映射；这些词与共享目录已经漂移，例如“火焰投弹者/烈焰爆破者”、“水之壁垒/水流壁垒”、“燃烧地面/灼烧地面”、“寒冷/冰寒”。同时保留完整 ASCII 映射和 `build_death_overlay_ascii_view`；renderer 在共享字体不可用时选择 ASCII 视图并回退 `GetFontDefault()`。所有格式化仍是原始 `snprintf`，没有 UTF-8 截断状态。

虽然 `raylib_host.cpp` 当前会在 `SharedUiFont::initialize()` 失败时退出，这些 standalone/fallback 分支仍违反正式不变量，并可被测试或其他入口触发。应删除英文回退的产品路径，字体无效时不渲染/返回资源错误；测试应从“ASCII fallback 正常”改为“不会产生英文玩家文案”。

原 Task 6 说消费 element/danger 映射，但 Task 1 并未定义对应接口。必须先补第 3 节的生态、伤害、危险和深渊规则 API，再替换本地 switch，并把这些文本全部纳入字形目录和边界审计。

## 9. 字体、字形容量、固定缓冲与零分配

共享字体基线：

- `src/platform/raylib/shared_ui_font.hpp`：`kUiGlyphCapacity = 640`，单实例，图集预算 32 MiB，基础 source size 96；
- `src/platform/raylib/shared_ui_font.cpp`：目录 visitor 去重收集码点，超限置 `capacity_exceeded`，初始化失败；只加载 `assets/fonts/NotoSansCJKsc-Medium.otf`；
- `src/platform/raylib/raylib_host.cpp`：共享字体初始化失败即阻止进入游戏；
- `tests/platform/shared_ui_font_tests.cpp`：当前只断言码点数 `> 384 && <= 640`，没有断言剩余容量或增量预算。

本次按 `ui_text_catalog.cpp` 当前文本静态去重，得到 633 个码点，其中 ASCII 95 个、非 ASCII 538 个，距离 640 只余 7 个。这个计算是源码级预检，不替代运行时 `SharedUiFont::initialize()`；但足以说明原计划新增多组中文映射会很容易超限。

另有命名重复成本：目录使用“嬗变石”，动态字形语料还包含“蜕变石”，两个不同汉字都会占用图集。应先统一文案并去掉不再可达的旧字形，再评估新增字。

建议实施门禁：

1. 每次新增目录文字前，以现有目录为基线计算“新增唯一码点数”和余量；
2. 完成后由测试验证 visitor 全覆盖、未知回退、UTF-8 有效、共享字体可初始化、只有一个 atlas、估算内存不超过 32 MiB；
3. 不要静默提高 640。用户要求避免内存溢出而非极端压缩；如果统一文案后仍超限，可在实测图集字节数、峰值内存和单 atlas 条件下有意识调整容量，并同步规格；
4. 禁止为每个页面单独加载中文字体或生成第二图集；
5. 每帧展示层只用固定 `std::array<char, N>` 和 `ui_utf8_format` / `ui_utf8_copy` / `ui_utf8_fit_with_ellipsis`；禁止临时 `std::string`、`std::vector` 或每帧字形重建；
6. 每种固定缓冲都要有最坏组合测试、NUL 终止、完整码点截断和诊断位；保留现有十万次压力/零分配测试。

## 10. 默认字体、居中和真实背景对比度

仍需消除的默认字体路径：

- `inventory_renderer.cpp::draw_hud_font_text`；
- `material_bag_renderer.cpp::draw_material_text`；
- `passive_tree_renderer.cpp` 的直接 `DrawText` / `MeasureText` 和 fallback helper；
- `death_overlay_renderer.cpp` 的 `GetFontDefault()` / ASCII fallback。

统一策略：host 是字体生命周期 owner；页面 renderer 若拿不到有效共享字体，只能 no-op 或上报资源不可用，绝不调用默认字体，也绝不构造英文替代文案。

居中必须按 `MeasureTextEx` 的实际像素尺寸计算，覆盖标题、页签、按钮、确认/取消、关闭提示、技能图标下名称；属性表仍按规格使用标签左对齐、数值右对齐。不能用字符数估算或固定 `x + N`。

`ui_text_contrast.hpp` 目前基于近似亮度和全局 `style.backing`；inventory/material/death 的守卫未必拿到按钮、纹理或弹窗的真实背景，被动树则大量使用硬编码颜色且没有统一守卫。实现层应把实际不透明 backing / role 传给文字布局或渲染计划；最终 4.5:1 必须由 release-validation 的真实截图和对比度报告验收，单元测试只能验证选定色对。

## 11. 测试选择器与自动化入口

### 11.1 当前选择器缺口

`tests/platform/platform_test_main.cpp` 当前有：

- `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY`
- `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY`

但没有原计划使用的 `ARPG_FULL_CHINESE_UI_INVENTORY_ONLY`。应在最早的修订任务中新增，且不要只包含 inventory/material bag；建议覆盖以下 suite：

- `ui_text_catalog_tests.cpp`、`ui_utf8_tests.cpp`、`shared_ui_font_tests.cpp`；
- `inventory_view_math_tests.cpp`；
- `active_skill_loadout_view_tests.cpp`、`active_skill_view_tests.cpp`；
- `ground_loot_view_tests.cpp`、`loot_pickup_feedback_tests.cpp`、`material_loot_view_tests.cpp`；
- `material_bag_renderer_tests.cpp`；
- `passive_tree_view_tests.cpp`；
- `monster_view_tests.cpp`、`combat_view_math_tests.cpp`；
- `death_overlay_view_tests.cpp`、`hud_font_tests.cpp`；
- 新增的居中/对比度/边界审计纯函数测试。

选择器 case count 和总 case count 必须在新增测试后重新计算。当前源码仍把总数写为 `670`；并行 Task 8 的交接信息称实际已经运行到 671，因此这个常量很可能已经过期。本次审计没有运行测试，不能独立确认该交接结果。

### 11.2 后续实施时的验证顺序

每个任务先跑窄选择器/目标，再跑完整 Windows 调试验证；本阶段只记录入口，不执行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_passive_tests
$env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY='1'
.\\build\\windows-msvc-debug\\tests\\platform\\Debug\\arpg_platform_tests.exe
Remove-Item Env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY
.\\build\\windows-msvc-debug\\tests\\passives\\Debug\\arpg_passive_tests.exe
scripts\\Test.ps1 -Preset windows-msvc-debug
scripts\\Build.ps1 -Preset windows-msvc-release
```

## 12. 正式视觉验证入口与缺口

现有可复用入口：

| 入口 | 当前覆盖 | 明确缺口 |
|---|---|---|
| `stage12.material_formal`（`tests/platform/CMakeLists.txt`） | 真 raylib；背包和技能石 showcase 有 1280×720、1920×1080；integration death 有 800/1280/1920；检查单字体/atlas 预算和部分 audit role | 背包/技能石没有 800；死亡未接入文字审计；没有被动树 showcase |
| `stage16.loot_reinforcement.real_raylib` | 真实地面材料、材料袋/强化确认与完成画面；1280×720 | 没有 800/1920；没有完整中文/真实对比度的机器判定 |
| `stage17.skill_stones.real_raylib` + `stage17.skill_stones.evidence_validator` | 真实技能石流程，包含重启后背包技能页截图；1280×720 | 没有 800/1920；多数画面是 gameplay，不等同于背包全状态 |
| release-validation 计划 | 800×450、1280×720、1920×1080 的背包空/满、材料袋、强化弹窗、被动 tooltip/状态、拾取 HUD、死亡恢复、F1 调试矩阵 | 必须在实现完成后本机执行；当前不能宣称通过 |

后续可执行的窄 CTest 入口：

```powershell
ctest --preset windows-msvc-debug --output-on-failure -R '^stage12\.material_formal$'
ctest --preset windows-msvc-debug --output-on-failure -R '^stage16\.loot_reinforcement\.real_raylib$'
ctest --preset windows-msvc-debug --output-on-failure -R '^stage17\.skill_stones\.(real_raylib|evidence_validator)$'
```

需要新增或明确归属的正式入口：

- 被动树三分辨率 showcase，覆盖未分配/可分配/已分配/锁定/保存中和 tooltip；
- ground loot 与 pickup feedback 的最坏中文宽度画面；
- 死亡页面的 `ui_text_bounds_audit` 角色与真实中文字体路径断言；
- 怪物职业短名若最终要求显示，则需 F1/debug 场景截图和血条宽度断言。

自动化门禁只证明逻辑、字形、边界记录和资源约束；只有 release-validation 的本机截图/人工矩阵完成后，才可声称“全部汉化、居中、布局合理、对比度合格”。

## 13. 建议重排的实施任务与 ownership

### Task 0：以当前 HEAD 重新基线化

- 先确认 Task 8 已合入后的 clean status 和 HEAD；
- 修正总 case count；
- 新增 `ARPG_FULL_CHINESE_UI_INVENTORY_ONLY` 选择器；
- 明确本计划与 release-validation 对 `ui_text_bounds_audit.*`、正式验证器的唯一 owner。

### Task 1：目录增量与字形门禁

- 验证现有映射，不重做已经存在的物品/材料/被动/怪物/词缀映射；
- 只添加定向合成类别、被动短标、死亡生态/伤害/危险/深渊规则，以及经确认需要的怪物短名；
- 先统一“嬗变/蜕变”等词，再用码点增量测试决定是否需要调整容量；
- 保持核心稳定 ID 与英文内部名不变。

### Task 2：背包、装备详情和技能装配

- `inventory_view_math.*`、`inventory_renderer.*` 及测试；
- 若前一阶段未负责，则同时纳入 `active_skill_view.*`、`active_skill_renderer.*` 及测试；
- 建立通用纯居中布局函数、详情双列布局和 UTF-8 fit；
- 让 base/rarity/slot/affix/skill 全部消费目录。

### Task 3：完整 loot/material 链路

- `ground_loot_view.*`、`loot_pickup_feedback.*`、`material_loot_view.*`；
- `material_bag_renderer.*`、确认弹窗纯视图模型、合成/强化文案；
- 对应全部平台测试和 Stage 16 验证器；
- 固定缓冲、UTF-8 完整性和十万次零分配契约为硬门禁。

### Task 4：被动树

- platform 展示层全中文、短标/tooltip 分层、共享字体、非颜色状态标记；
- 核心 `passives` 只保留稳定英文 key，不把产品文案写回核心；
- 若 ownership 分给本阶段，则同时实现被动 audit role 和正式 showcase；否则只提供纯展示模型，由 release 任务接审计。

### Task 5：怪物/词缀回归，而非重做

- 验证所有 8 个怪物和 12 个词缀实际消费者；
- 明确“头顶短标签”到底指怪物职业还是词缀；
- 仅在明确需要时补怪物短名和血条宽度测试。

### Task 6：死亡结算

- 删除本地重复映射和 ASCII fallback；
- 消费 Task 1 的统一目录；
- 用固定 UTF-8 helper 构造视图；
- 接入死亡 audit role，并验证三分辨率截图。

### Completion gate：自动化与视觉验收分开

- 本计划完成：窄测试、`scripts/Test.ps1 -Preset windows-msvc-debug`、`scripts/Build.ps1 -Preset windows-msvc-release`、单 atlas/字形/固定缓冲门禁；
- release-validation 完成：源码英文扫描、真实 backing 的 4.5:1、800/1280/1920 人工截图矩阵、字体失败 fail-closed、内存稳定性；
- 在第二项完成前只能报告“实现和自动化通过，等待 Windows 正式视觉验收”。

## 14. 实施前必须确认的决策

1. 怪物“头顶短标签”是否只指词缀，还是还要显示怪物职业短名；
2. `ui_text_bounds_audit.*` 与正式 screenshot validator 由本计划还是 release-validation 独占修改；
3. 统一采用“嬗变石”还是“蜕变石”（当前规格/目录偏向前者）；
4. 96 字节材料聚合提示采用“完整条目 + 等 N 种”还是经过测量后扩大固定缓冲；
5. 字形去重后若仍超过 640，是收敛文案还是按实测内存结果调整容量。无论选择哪种，都保持单 atlas 和 32 MiB 总预算。

以上五项不需要阻塞静态测试和消费者清单整理，但会影响展示 API、字形容量与并行文件 ownership，应在进入对应实现任务前确定。
