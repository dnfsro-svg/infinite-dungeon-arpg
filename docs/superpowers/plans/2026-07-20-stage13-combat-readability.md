# Stage 13 战斗可读性与首房间清晰度 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不改变战斗数值、AI、房间生成和死亡规则的前提下，让玩家在首个房间内能快速看懂自身状态、受击来源、敌人攻击窗口、击杀结果和下层入口。

**Architecture:** 核心战斗继续只发布不可变事件与快照；raylib 平台层把事件转换为固定容量、定时失效的视觉反馈，并通过纯视图规划函数决定怪物警告状态。所有文字、箭头、地面警告和击杀标记只存在于渲染层。洞口提示的显示判定与地下城交互判定共享同一常量数值，避免“看得见但按不到”。

**Tech Stack:** C++17、raylib 6.0、CMake、Ninja/MSVC、CTest、现有自定义无窗口测试框架。

## Global Constraints

- 普通房间洞口严格保持每房间独立固定 10% 概率，不保底、不补偿、不重掷。
- 不修改伤害、攻击帧、无敌帧、击退、浮空、怪物 AI、生成位置、掉落或死亡规则。
- `src/core`、`src/combat`、`src/dungeon` 不得依赖 raylib；新增视觉状态只进入 `src/platform/raylib`。
- 反馈容器保持固定容量；渲染热路径不得分配内存、读文件或创建纹理。
- 1280x720 与 1920x1080 都必须可读；材质缺失时程序绘制回退仍须显示完整反馈。
- 真实游戏验证必须覆盖进入房间、战斗、受击、击杀、死亡/继续和洞口下层流程，不以单元测试替代画面验收。
- 保留工作树内所有 Stage 12 资产和已有验证证据；不删除文件、不重置 Git、不覆盖无关改动。

---

## File Structure

- `src/platform/raylib/hud_renderer.hpp`: HUD 字号与描边可读性基线。
- `src/platform/raylib/death_overlay_view.cpp`: 死亡面板排版与字号。
- `src/platform/raylib/death_overlay_renderer.cpp`: 死亡面板文字描边。
- `src/platform/raylib/dungeon_view_math.hpp`: 洞口提示共享几何常量。
- `src/dungeon/dungeon_transition.cpp`: 与视图一致的洞口交互半径。
- `src/platform/raylib/room_renderer.cpp`: 洞口状态与按键提示。
- `src/platform/raylib/combat_view_math.hpp/.cpp`: 怪物攻击警告与高威胁标记的纯规划。
- `src/platform/raylib/combat_feedback.hpp/.cpp`: 受击方向和击杀标记的固定池状态。
- `src/platform/raylib/actor_renderer.cpp`: 玩家受击箭头、敌人攻击地面警告、标签分层和击杀反馈绘制。
- `tests/platform/combat_feedback_tests.cpp`: 视觉反馈生命周期测试。
- `tests/platform/monster_view_tests.cpp`: 全直接攻击者警告规划测试。
- `tests/platform/platform_test_main.cpp`: Stage 13 平台测试注册数。
- `docs/validation/stage13-combat-readability.md`: 最终命令、结果、真实画面证据与约束审计。

## 13-A：基础界面与入口清晰度

### Task 1: 提升 HUD、死亡面板、怪物标签和洞口提示可读性

**Files:**
- Modify: `src/platform/raylib/hud_renderer.hpp`
- Modify: `src/platform/raylib/death_overlay_view.cpp`
- Modify: `src/platform/raylib/death_overlay_renderer.cpp`
- Modify: `src/platform/raylib/dungeon_view_math.hpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`

**Interfaces:**
- Consumes: `DungeonSnapshot`、`CombatSnapshot`、`kHoleCenter`。
- Produces: 2px 描边 HUD、稳定怪物信息层、`E: DESCEND` 提示、半径 3.25 的一致交互判定。

- [x] **Step 1: 锁定不变量并检查核心规则无语义差异**

Run: `git diff -- src/combat/combat_world.cpp src/dungeon/room_generation.cpp src/dungeon/encounter_director.cpp`

Expected: 无语义 diff；洞口生成仍为 `hole_value < threshold`，没有房间计数保底。

- [x] **Step 2: 更新 HUD 和死亡面板排版**

将 HUD 正文下限提高到 15px，主要状态提高到 19-22px，并统一使用 2px 描边。死亡面板标题、正文和继续提示分别提高到桌面 36/18/26px，紧凑布局 28/15/22px。

- [x] **Step 3: 统一洞口显示与交互范围**

```cpp
inline constexpr combat::Vec3 kHoleCenter{0.0F, 3.5F, 0.0F};
inline constexpr float kHoleInteractionRadius = 3.25F;
```

地下城转换层使用相同中心与半径数值；渲染提示改为 26px 的 `E: DESCEND`。只扩大交互范围，不改变洞口出现概率。

- [x] **Step 4: 把怪物信息块移至角色上方并分配四条稳定纵向通道**

角色索引通过 `label_lane % 4` 决定偏移，生命/护盾/韧性条、词缀、角色名和阶段名作为一个整体移动，避免近战重叠时互相覆盖。

- [x] **Step 5: 构建并运行地下城和平台测试**

Run: `cmake --build --preset windows-msvc-release --target arpg_dungeon_tests arpg_platform_tests`

Run: `ctest --test-dir out/build/windows-msvc-release -R "dungeon.units|platform.units" --output-on-failure`

Expected: 两个测试套件全部通过，洞口生成统计和固定概率测试不变。

## 13-B：战斗瞬时反馈

### Task 2: 显示玩家受击来源方向

**Files:**
- Modify: `src/platform/raylib/combat_feedback.hpp`
- Modify: `src/platform/raylib/combat_feedback.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `tests/platform/combat_feedback_tests.cpp`

**Interfaces:**
- Consumes: `CombatEventKind::player_hit` 的事件位置。
- Produces: `player_hit_source()` 与最长 0.55 秒的 `player_hit_indicator_seconds()`。

- [x] **Step 1: 添加生命周期失败测试**

```cpp
feedback.consume({CombatEventKind::player_hit, Vec3{-2.0F, 1.5F, 0.0F}});
ARPG_REQUIRE(near(feedback.player_hit_indicator_seconds(), 0.55, 1.0e-4));
feedback.update(0.6F);
ARPG_REQUIRE(near(feedback.player_hit_indicator_seconds(), 0.0, 1.0e-4));
```

Run: `cmake --build --preset windows-msvc-release --target arpg_platform_tests`

Expected before implementation: 编译因缺少受击方向接口而失败。

- [x] **Step 2: 在 `CombatFeedback` 中保存来源和短时计时器**

只在 `player_hit` 事件到达时覆盖来源，`update()` 以帧秒数递减，`clear()` 完全复位；不进入战斗模拟状态。

- [x] **Step 3: 在玩家周围绘制指向伤害来源的红色三角箭头**

将玩家与来源位置投影到屏幕，归一化方向后在玩家半径 46px 处绘制箭头，并按剩余 0.55 秒淡出。

- [x] **Step 4: 运行平台测试确认通过**

Run: `ctest --test-dir out/build/windows-msvc-release -R platform.units --output-on-failure`

Expected: `player hit source indicator` 通过，固定池与镜头震动旧测试继续通过。

### Task 3: 为所有直接攻击者显示攻击窗口，并突出高威胁敌人

**Files:**
- Modify: `src/platform/raylib/combat_view_math.hpp`
- Modify: `src/platform/raylib/combat_view_math.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `tests/platform/monster_view_tests.cpp`

**Interfaces:**
- Consumes: `MonsterId` 与 `MonsterAiPhase`。
- Produces: `MonsterWarningMode` 和 `priority_warning`；支援怪不产生直接攻击预警。

- [x] **Step 1: 扩展纯视图失败测试**

```cpp
ARPG_REQUIRE(monster_visual(MonsterId::water_bulwark,
    MonsterAiPhase::telegraph, DungeonElement::water).warning_mode
    == MonsterWarningMode::telegraph);
ARPG_REQUIRE(monster_visual(MonsterId::lightning_shooter,
    MonsterAiPhase::active, DungeonElement::lightning).warning_mode
    == MonsterWarningMode::active);
```

Expected before implementation: 非优先怪物的警告模式仍为 `none`，测试失败。

- [x] **Step 2: 将 telegraph/active 警告扩展到全部直接攻击者**

`water_support` 和哨兵 `count` 排除；冲锋、位移和危险区怪物继续设置 `priority_warning=true`。

- [x] **Step 3: 绘制强弱分明的警告**

预备阶段绘制 18% 透明红色地面椭圆，生效阶段提高到 38%；高威胁敌人额外显示描边 `!`，原有冲锋线、射击线和危险圈保留。

- [x] **Step 4: 运行纯规划与平台测试**

Run: `ctest --test-dir out/build/windows-msvc-release -R platform.units --output-on-failure`

Expected: 所有 8 种怪物视图映射、颜色、警告和标签测试通过。

### Task 4: 添加明确的击杀确认

**Files:**
- Modify: `src/platform/raylib/combat_feedback.hpp`
- Modify: `src/platform/raylib/combat_feedback.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `tests/platform/combat_feedback_tests.cpp`

**Interfaces:**
- Consumes: `CombatEventKind::defeated`。
- Produces: 固定池 `VisualEffectKind::defeat_marker`，生命周期 0.65 秒。

- [x] **Step 1: 添加击杀标记失败测试**

```cpp
feedback.consume(defeated);
ARPG_REQUIRE(feedback.active_count() == 1U);
ARPG_REQUIRE(feedback.effects()[0].kind == VisualEffectKind::defeat_marker);
feedback.update(0.7F);
ARPG_REQUIRE(feedback.active_count() == 0U);
```

Expected before implementation: 枚举和事件分支不存在，编译失败。

- [x] **Step 2: 复用现有固定池生成标记**

`defeated` 事件只生成一个 0.65 秒标记并立即返回，不触发额外伤害数字、音频或模拟状态。

- [x] **Step 3: 绘制 `DEFEATED` 与扩散圆环**

标记在目标上方上浮并淡出；材质表没有该帧时使用程序绘制，不要求新增资源。

- [x] **Step 4: 运行平台测试**

Run: `ctest --test-dir out/build/windows-msvc-release -R platform.units --output-on-failure`

Expected: `defeated marker` 及全部既有视觉反馈测试通过。

## 13-C：集成验收与交付

### Task 5: 运行完整回归和真实游戏验收

**Files:**
- Create: `docs/validation/stage13-combat-readability.md`
- Preserve: `docs/validation/evidence/stage10-formal-game-validation/`
- Preserve: `docs/validation/evidence/stage12-material-pack/`

**Interfaces:**
- Consumes: Release 构建、实际 `arpg_game.exe`、Stage 10 正式流程验证器。
- Produces: 可重复命令、测试数量、截图路径、可见行为记录和规则不变量审计。

- [x] **Step 1: 运行核心、战斗、地下城和平台无窗口测试**

Run: `ctest --test-dir out/build/windows-msvc-release -R "core.units|combat.units|dungeon.units|platform.units" --output-on-failure`

Expected: 四个套件全部通过；平台套件报告 308 cases、0 failures，战斗套件报告 205 cases、0 failures。

- [x] **Step 2: 运行架构和范围守卫**

Run: `ctest --test-dir out/build/windows-msvc-release -R "architecture|scope|dependency" --output-on-failure`

Expected: raylib 依赖边界、动态分配扫描和范围守卫全部通过。

- [x] **Step 3: 运行 Stage 10 正式游戏流程验证**

Run: `ctest --test-dir out/build/windows-msvc-release -R stage10.formal.game --output-on-failure`

Expected: `formal_captures=7`，死亡、重置、重启和深渊洞口下层均为 PASS，七张截图指标全部 PASS。

- [x] **Step 4: 启动真实游戏并进行人工画面验收**

通过桌面窗口进入首个房间，实际移动和攻击，观察 HUD/敌人标签；主动承受一次伤害确认红色方向箭头；观察敌人 telegraph/active 地面警告；击杀一只敌人确认 `DEFEATED`；死亡后确认面板可读；遇到洞口时确认在明显宽于旧范围的位置按 E 可下层。

- [x] **Step 5: 写入 Stage 13 验收报告并做最终差异审计**

报告记录构建预设、命令、通过数量、正式截图目录和人工结果。最后运行：

Run: `git diff --check`

Run: `git diff -- src/combat/combat_world.cpp src/dungeon/room_generation.cpp src/dungeon/encounter_director.cpp`

Expected: 无空白错误；核心规则无语义 diff；Stage 13 只包含展示和交互可用性改动。

- [x] **Step 6: 提交 Stage 13 独立改动（仅在改动可与既有脏工作树明确分离时）**

Stage only the Stage 13 files listed above, then commit with message `feat: complete stage 13 combat readability`。若任何文件混有无法安全拆分的既有改动，则不提交，在验收报告中记录原因，避免覆盖用户改动。

## Plan Self-Review

- 需求覆盖：HUD、死亡提示、敌人标签、受击方向、攻击窗口、击杀反馈和洞口交互均有实现与验收步骤。
- 约束覆盖：固定 10% 独立洞概率、无保底、无战斗数值/AI/死亡规则修改均列为硬性守卫。
- 类型一致：事件使用现有 `CombatEventKind`/`Vec3`，视觉状态使用固定池和 `float` 秒数，不向核心层引入 raylib 类型。
- 验证覆盖：纯函数、生命周期、四层单元测试、架构守卫、正式游戏流程和真实窗口人工验收均被要求。
- 占位符扫描：计划不存在占位标记、未决参数或未定义文件名。
