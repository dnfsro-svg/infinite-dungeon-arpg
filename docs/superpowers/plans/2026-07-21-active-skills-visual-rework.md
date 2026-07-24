# 里程碑 1：主动技能视觉重做 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 作为全量材质包重制的里程碑 1，将主动技能石“拔刀斩”和“极·鬼剑术（暴风式）”重做为由 60 Hz 动画事件驱动、可逐帧播放且与已确认分镜一致的完整战斗演出。

**Architecture:** 战斗层新增固定容量技能时间线；伤害、无敌、牵引、实体剑生成与清理由同一事件序列推进。Raylib 层将时间线快照投影为分层精灵、拖尾、碎屑和动态光；云端负责纯 C++ 时间线、测试和资源校验，本地负责 raylib 6.0 构建、真实窗口录制、性能与人工验收。

**Tech Stack:** C++17、raylib 6.0.0、CMake 3.25、MSVC 19.44、Windows SDK 10.0.26100.0、CTest。

## Global Constraints

- 以 `docs/superpowers/specs/2026-07-21-full-art-material-animation-rebuild-design.md` 和其中的 `active-skills-storyboard.png` 为视觉基准；不得复制 DNF 受版权保护资产。
- 不增加光剑、太刀或职业分支；使用暗钢古金实体剑和蓝白能量。
- 拔刀斩为 36 个有效帧、一次伤害；暴风式约 6 秒、至少 96 个有效角色帧、24 把实体剑（12 地面、12 空中）。
- 伤害、牵引、无敌、剑阵、终结和回收由同一事件表驱动；渲染层不得独立计时修改战斗状态。
- 保持五个主动技能石槽、默认小键盘 `NumPad 1..5`、可取出/交换；辅助技能石槽继续为空且无效果。
- 不改存档格式、装备数值、掉落、怪物 AI、房间词条和 J/K/L/WASD 语义。
- 游戏循环、战斗、渲染热路径不得堆分配；死亡、换房和中断必须释放临时状态。
- 每项任务遵守 RED → GREEN → 回归 → 独立提交；不覆盖用户未提交文件。

---

## File responsibility map

| Area | Files | Responsibility |
|---|---|---|
| 时间线 | `src/combat/active_skill_timeline.hpp/.cpp` | 两招的帧率、事件、帧区间、剑位和阶段查询；无 raylib 依赖。 |
| 战斗 | `active_skill_runtime.*`, `combat_types.hpp`, `combat_world.*` | 推进快照、伤害、牵引、无敌、剑体生命周期和中断回收。 |
| 资源 | `assets/skills/*`, `active_skill_assets.*` | 自制图集、锚点、分层帧和完整性验证。 |
| 演出 | `active_skill_renderer.*`, `active_skill_view.*`, `raylib_host.cpp` | 分层绘制、拖尾、灯光、震屏和 HUD 投影。 |
| 验收 | `tests/combat/*`, `tests/platform/*`, `docs/validation/*` | 单元、零分配、真实窗口截图、录屏与性能记录。 |

---

### Task 1: 固定容量动画事件时间线

**Files:**
- Create: `src/combat/active_skill_timeline.hpp`
- Create: `src/combat/active_skill_timeline.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Create: `tests/combat/active_skill_timeline_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Produces `ActiveSkillClip`, `ActiveSkillTimelineEvent`, `active_skill_clip(ActiveSkillId)`, `active_skill_events_at(ActiveSkillId, std::uint16_t)`.
- Consumed only by `CombatWorld::tick_active_skill`.

- [ ] **Step 1: Write the failing test.** 覆盖拔刀斩总长 90 tick（36 帧 × 24 fps）、唯一 `damage`；暴风式总长 360 tick、24 个 `spawn_sword`、12 个普通斩击、一次终结、一次无敌开关，且事件按 tick 非递减。

- [ ] **Step 2: Run the RED test.**

```powershell
.\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
$env:ARPG_ACTIVE_SKILL_TIMELINE_ONLY='1'
& out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
Remove-Item Env:ARPG_ACTIVE_SKILL_TIMELINE_ONLY
```

Expected: 仅因时间线接口尚不存在而失败。

- [ ] **Step 3: Write the minimal implementation.**

```cpp
enum class ActiveSkillTimelineEventKind : std::uint8_t {
    phase_startup, phase_strikes, phase_finisher, phase_recovery,
    damage, invulnerability_on, invulnerability_off, pull, spawn_sword,
    clear_transients,
};
struct ActiveSkillTimelineEvent final {
    std::uint16_t tick{};
    ActiveSkillTimelineEventKind kind{};
    std::uint8_t ordinal{};
};
struct ActiveSkillClip final {
    std::uint16_t duration_ticks{};
    std::uint16_t frame_count{};
    std::uint8_t frames_per_second{};
};
[[nodiscard]] const ActiveSkillClip* active_skill_clip(
    skills::ActiveSkillId id) noexcept;
[[nodiscard]] std::span<const ActiveSkillTimelineEvent>
active_skill_events_at(skills::ActiveSkillId id, std::uint16_t tick) noexcept;
```

用 `std::array` 保存事件：拔刀斩在 tick 0/45/46/66/90 分别蓄势、斩击、唯一伤害、收刀、清理；暴风式在 tick 0/72/96/324/360 进入剑阵、升空连斩、无敌、终结、清理，24 剑和 12 段斩击逐条列出，禁止动态容器。

- [ ] **Step 4: Run the GREEN test.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
ctest --test-dir out/build/windows-msvc-debug -R '^combat\.units$' --output-on-failure
```

Expected: 时间线顺序、数量和既有战斗单元测试通过。

- [ ] **Step 5: Commit.**

```powershell
git add src/combat tests/combat
git commit -m "feat: add active skill animation timelines"
```

### Task 2: 用时间线重做战斗状态与清理

**Files:**
- Modify: `src/combat/active_skill_runtime.hpp`
- Modify: `src/combat/active_skill_runtime.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `tests/combat/draw_slash_skill_tests.cpp`
- Modify: `tests/combat/storm_swords_skill_tests.cpp`

**Interfaces:**
- Consumes `active_skill_events_at`.
- Extends `ActiveSkillSnapshot` with `frame_index`, `swords_spawned`, `player_invulnerable`, `transients_active`.
- Emits existing `CombatEvent` with preserved `skill`, `strike_index`, `finisher` semantics.

- [ ] **Step 1: Write the failing test.** 替换旧 10/24 tick 断言：拔刀斩在 tick 46 才命中且每目标仅一次；暴风式出现 24 把剑、12 次普通伤害、一次终结、连斩窗口无敌；死亡、换房与重置后剑数为零、无敌关闭、无残留事件。

- [ ] **Step 2: Run the RED test.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
$env:ARPG_STAGE17_DRAW_SLASH_ONLY='1'; & out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
$env:ARPG_STAGE17_STORM_SWORDS_ONLY='1'; & out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
Remove-Item Env:ARPG_STAGE17_DRAW_SLASH_ONLY,ARPG_STAGE17_STORM_SWORDS_ONLY
```

Expected: 旧短计时器无法满足逐事件断言。

- [ ] **Step 3: Write the minimal implementation.** 每 tick 计算 `frame_index = min(frame_count - 1, elapsed_ticks * fps / 60)`，只在该 tick 事件调用伤害、牵引与临时状态切换。拔刀斩改为锁定中心周身椭圆，伤害值沿用现有平衡；暴风式牵引具有每 tick 上限且不穿过障碍，终结沿用现有击飞参数。

- [ ] **Step 4: Add unified cleanup.** 新增私有 `clear_active_skill_transients() noexcept`，由死亡、`reset`、`load_wave`、房间切换和 `clear_transients` 事件调用，清空 hit latch、剑数、无敌、快照和视觉瞬态。

- [ ] **Step 5: Run GREEN and regression.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
ctest --test-dir out/build/windows-msvc-debug -R '^combat\.units$' --output-on-failure
```

Expected: 技能、伤害、死亡、移动与零分配测试通过。

- [ ] **Step 6: Commit.**

```powershell
git add src/combat tests/combat
git commit -m "feat: drive active skill combat from animation events"
```

### Task 3: 自制分层资源与 raylib 演出

**Files:**
- Create: `assets/skills/draw_slash_atlas.png`
- Create: `assets/skills/storm_swords_atlas.png`
- Create: `assets/skills/active_skill_frames.json`
- Create: `src/platform/raylib/active_skill_assets.hpp`
- Create: `src/platform/raylib/active_skill_assets.cpp`
- Modify: `src/platform/raylib/active_skill_renderer.hpp`
- Modify: `src/platform/raylib/active_skill_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Create: `tests/platform/active_skill_asset_tests.cpp`
- Create: `tests/platform/active_skill_renderer_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- Produces `ActiveSkillFrameAtlas`，含 `body`、`weapon`、`back_effect`、`front_effect`、`shadow` 图框及脚底/武器锚点。
- `ActiveSkillEffectPlan` 只读取 `ActiveSkillSnapshot` 和帧图集，携带 24 剑位置、拖尾、动态灯与震屏，不回写战斗。

- [ ] **Step 1: Write the failing test.** 检查两张原创 PNG 和帧清单；拔刀 36 个非空帧；暴风角色不少于 96 个非重复关键姿态；锚点在图框内；拔刀伤害帧与白色刃核同步；暴风式恰有 12 地面剑、12 空中剑，清理后全部不可见。

- [ ] **Step 2: Run the RED test.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
```

Expected: 当前程序化扇形/三角剑和缺失资源无法通过。

- [ ] **Step 3: Produce and load original assets.** 原创资源遵循暗钢人物、古金实体剑、`#F8FBFF/#82CFFF/#2469C9/#102653` 能量层与克制琥珀火花；不提取 DNF 像素。清单每帧有 `clip/frame/source/foot_anchor/weapon_anchor/layer`。加载失败必须报告资源 ID 和路径，`unload()` 可重复调用。

- [ ] **Step 4: Replace placeholder geometry.** 绘制顺序为阴影、后景剑阵、角色身体、实体剑、蓝白轨迹、前景火花/碎屑。拔刀环边缘清晰且不以半透明雾遮挡敌人；暴风终结包含能量柱、古金闪光、碎屑与短震屏。

- [ ] **Step 5: Pool all transient visuals.** 拖尾、碎屑、短时灯、震屏使用预分配数组；低资源档只降非关键粒子、拖尾采样和动态灯，不删关键姿态、伤害帧或 24 剑。

- [ ] **Step 6: Run GREEN.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
```

Expected: 资源完整性、锚点、渲染计划、HUD 和热路径检查通过。

- [ ] **Step 7: Commit.**

```powershell
git add assets/skills src/platform/raylib tests/platform
git commit -m "feat: render storyboard active skill effects"
```

### Task 4: 真实 raylib 验收与性能证据

**Files:**
- Modify: `tests/platform/stage17_skill_stones_game_validation.cpp`
- Modify: `tests/platform/stage17_skill_stones_validator.ps1`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `docs/validation/active-skills-visual-rework.md`

**Interfaces:**
- Produces `active-skill-rework-run`：1280×720 PNG、短录屏、状态 JSON 与性能 CSV。
- 只通过公开 `DungeonRuntime`、`NumPad 1`、`NumPad 2`；禁止测试专属产品快捷键。

- [ ] **Step 1: Write the failing validator.** 要求五项证据：拔刀蓄势、拔刀命中、暴风剑阵、空中连斩、终结落地；JSON 记录帧、伤害 tick、24 剑、无敌窗口、清理状态、平均 FPS 与 1% low。旧 Stage17 证据必须被拒绝。

- [ ] **Step 2: Run the RED validator.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_stage17_skill_stones_game_validation
ctest --test-dir out/build/windows-msvc-debug -R '^stage17\.skill_stones\.' --output-on-failure
```

Expected: 旧占位效果无法生成新证据或所需状态字段。

- [ ] **Step 3: Implement capture and performance sampling.** 固定 seed、1280×720、30 个怪物、一次暴风式；验证 PNG 尺寸/非空/事件顺序和清理完成。目标 60 fps、1% low ≥45、峰值纹理显存 ≤256 MB；若超标只先降低非关键粒子、拖尾和灯光。

- [ ] **Step 4: Run Debug and Release acceptance.**

```powershell
.\scripts\Configure.ps1 -Preset windows-msvc-debug
.\scripts\Build.ps1 -Preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -R '^(combat\.units|platform\.units|stage17\.skill_stones\.)' --output-on-failure
.\scripts\Configure.ps1 -Preset windows-msvc-release
.\scripts\Build.ps1 -Preset windows-msvc-release
ctest --test-dir out/build/windows-msvc-release -R '^(combat\.units|platform\.units|stage17\.skill_stones\.)' --output-on-failure
```

Expected: Debug 与 Release 通过；验收文档记录证据绝对路径、命令、帧率和已知限制。

- [ ] **Step 5: Commit.**

```powershell
git add tests/platform docs/validation
git commit -m "test: add active skill visual acceptance evidence"
```

## 自审

- 覆盖：Task 1/2 实现统一事件、24 剑、无敌、牵引、判定与清理；Task 3 实现 36/96 帧、色彩、分层、拖尾和低资源档；Task 4 实现真实窗口、性能和证据。
- 排除：不触碰技能石装卸/存档、默认小键盘、J/K/L/WASD、掉落、装备、怪物、房间和音频资产。
- 一致性：`ActiveSkillTimelineEvent` 是唯一战斗事件源，`ActiveSkillSnapshot` 是唯一渲染运行态输入。
- 占位扫描：通过；每个任务有文件、接口、RED、GREEN、验收命令与提交边界。
