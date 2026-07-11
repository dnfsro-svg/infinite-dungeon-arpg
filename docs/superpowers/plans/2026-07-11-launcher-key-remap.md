# 取消重击并将上挑改到 L 键实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 彻底删除重击动作，将现有上挑原样绑定到 L，取消 U 战斗绑定，并保持其余 Stage 1 战斗规则稳定。

**Architecture:** 先把 raylib 键位映射收敛为一个可单测的固定绑定表，再一次性收缩 Combat Core 的动作枚举、攻击目录和所有重击测试场景。破韧、倒地和多目标覆盖分别迁移到上挑与 J3，不通过删除测试规避回归。

**Tech Stack:** C++17、raylib 6.0.0、CMake 3.25+、Ninja、MSVC 19.44、CTest、PowerShell。

## Global Constraints

- 60 Hz 固定逻辑步长保持不变。
- 上挑继续使用 7/4/17 tick、38 伤害、18 破韧、1.2 水平击退、9.5 垂直初速度和中等反馈。
- 轻型目标继续获得 1.25 倍冲量；不增加固定 0.5 秒浮空计时。
- 未破韧重甲继续拒绝控制；造成破韧的上挑同一击立即浮空。
- J1/J2/J3、空中 J、K、R、F1、F12、Esc 和关闭按钮保持现有规则。
- Combat Core 不包含或链接 raylib；热路径不新增动态分配。
- 基线为 `milestone/m01-combat-lab` 的 `3c75fc3`。
- 执行分支使用 `task/m01-launcher-remap`，工作树使用 `E:\game\.worktrees\m01-launcher-remap`。
- 只合并回 `milestone/m01-combat-lab`；不合并 `main`，不创建 Stage 2 分支。

---

### Task 1: 建立可测试的 L 上挑键位表

**Files:**

- Create: `src/platform/raylib/combat_key_bindings.hpp`
- Create: `tests/platform/combat_key_bindings_tests.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp:32-45`
- Modify: `src/platform/raylib/combat_renderer.cpp:460-462`
- Modify: `tests/platform/CMakeLists.txt:1-5`
- Modify: `tests/platform/platform_test_main.cpp:5-15`

**Interfaces:**

- Consumes: `arpg::combat::Action` 与 raylib `KeyboardKey` 常量。
- Produces: `inline constexpr std::array<CombatKeyBinding, 3> kCombatKeyBindings`，Host 每帧遍历该表提交动作。

- [ ] **Step 1: 写失败的键位表测试**

将 `combat_key_bindings_tests.cpp` 写为：

```cpp
#include "test_framework.hpp"

#include "combat_key_bindings.hpp"

namespace {

using namespace arpg;

test::Failure combat_keys_are_j_k_and_l_without_u() noexcept {
    ARPG_REQUIRE(platform::kCombatKeyBindings.size() == 3);
    ARPG_REQUIRE(platform::kCombatKeyBindings[0].key == KEY_J);
    ARPG_REQUIRE(
        platform::kCombatKeyBindings[0].action == combat::Action::light);
    ARPG_REQUIRE(platform::kCombatKeyBindings[1].key == KEY_K);
    ARPG_REQUIRE(
        platform::kCombatKeyBindings[1].action == combat::Action::jump);
    ARPG_REQUIRE(platform::kCombatKeyBindings[2].key == KEY_L);
    ARPG_REQUIRE(
        platform::kCombatKeyBindings[2].action == combat::Action::launcher);
    for (const platform::CombatKeyBinding& binding
         : platform::kCombatKeyBindings) {
        ARPG_REQUIRE(binding.key != KEY_U);
    }
    return {};
}

constexpr test::TestCase kCases[] = {
    {"J K L bindings without U", &combat_keys_are_j_k_and_l_without_u},
};

}  // namespace

arpg::test::TestSuite combat_key_bindings_suite() noexcept {
    return {
        "combat_key_bindings",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
```

把该文件加入 `arpg_platform_tests`，在 `platform_test_main.cpp` 声明并加入 `combat_key_bindings_suite()`，同时把 `kExpectedCaseCount` 从 6 改为 7。

- [ ] **Step 2: 运行 RED，确认因绑定表尚不存在而失败**

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-debug
```

Expected: FAIL，编译器报告无法打开 `combat_key_bindings.hpp`。如果失败原因不是缺少该接口，先修正测试构建接线再继续。

- [ ] **Step 3: 实现固定键位表并让 Host 使用它**

创建 `combat_key_bindings.hpp`：

```cpp
#pragma once

#include "combat/input_buffer.hpp"

#include <raylib.h>

#include <array>

namespace arpg::platform {

struct CombatKeyBinding final {
    int key{};
    combat::Action action{};
};

inline constexpr std::array<CombatKeyBinding, 3> kCombatKeyBindings{{
    {KEY_J, combat::Action::light},
    {KEY_K, combat::Action::jump},
    {KEY_L, combat::Action::launcher},
}};

}  // namespace arpg::platform
```

在 `raylib_host.cpp` 包含该头文件，并把 `submit_frame_actions` 改为：

```cpp
void submit_frame_actions(combat::CombatWorld& world) noexcept {
    for (const CombatKeyBinding& binding : kCombatKeyBindings) {
        if (IsKeyPressed(binding.key)) {
            static_cast<void>(world.queue_action(binding.action));
        }
    }
}
```

HUD 两行改为：

```cpp
DrawText("WASD Move  J Light  K Jump  L Launcher", 30, y, 16, accent);
y += 25;
DrawText("R Reset  F1 Debug  F12 Screenshot  Esc Exit", 30, y, 16, accent);
```

- [ ] **Step 4: 运行 GREEN**

```powershell
cmake --build --preset windows-msvc-debug
& .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --test-dir .\out\build\windows-msvc-debug -R platform.view_math --output-on-failure
```

Expected: `7 cases, 0 failures`；CTest 1/1 通过。

- [ ] **Step 5: 核对 U 不再进入运行时输入路径**

```powershell
rg -n "KEY_L|KEY_U|L Heavy|U Launcher|L Launcher" src/platform/raylib tests/platform
```

Expected: `KEY_L` 只出现在绑定表及其测试；`KEY_U` 只出现在“不得包含 U”的测试断言；HUD 只包含 `L Launcher`。

- [ ] **Step 6: 提交键位任务**

```powershell
git add src/platform/raylib/combat_key_bindings.hpp src/platform/raylib/raylib_host.cpp src/platform/raylib/combat_renderer.cpp tests/platform/combat_key_bindings_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git commit -m "feat: bind launcher to L"
```

---

### Task 2: 从 Combat Core 删除重击并迁移规则测试

**Files:**

- Modify: `src/combat/combat_types.hpp:19-27`
- Modify: `src/combat/input_buffer.hpp:9-14`
- Modify: `src/combat/attack_catalog.hpp:9`
- Modify: `src/combat/attack_catalog.cpp:8-21`
- Modify: `src/combat/player_simulation.cpp:199-212`
- Modify: `src/platform/raylib/combat_renderer.cpp` 的攻击名称映射
- Modify: `tests/combat/attack_catalog_tests.cpp`
- Modify: `tests/combat/attack_state_tests.cpp`
- Modify: `tests/combat/input_buffer_tests.cpp`
- Modify: `tests/combat/hit_resolution_tests.cpp`
- Modify: `tests/combat/dummy_reaction_tests.cpp`
- Modify: `tests/combat/break_stress_tests.cpp`

**Interfaces:**

- Produces: `Action` 仅包含 `light`、`jump`、`launcher`；`AttackId` 仅包含 J1、J2、J3、launcher、air_j 和哨兵 none。
- Preserves: `find_attack_definition`、`attack_phase_at`、`CombatWorld::queue_action` 的函数签名。

- [ ] **Step 1: 写攻击目录 RED**

先只修改 `attack_catalog_tests.cpp`，包含 `combat/input_buffer.hpp` 与 `<type_traits>`，再加入成员不存在检测：

```cpp
template <typename T, typename = void>
struct has_heavy_member : std::false_type {};

template <typename T>
struct has_heavy_member<T, std::void_t<decltype(T::heavy)>>
    : std::true_type {};

static_assert(!has_heavy_member<Action>::value);
static_assert(!has_heavy_member<AttackId>::value);
static_assert(kAttackCount == 5);

arpg::test::Failure catalog_has_five_attacks() noexcept {
    ARPG_REQUIRE(find_attack_definition(AttackId::j1) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::j2) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::j3) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::launcher) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::air_j) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::none) == nullptr);
    return {};
}

arpg::test::Failure launcher_definition_is_stable() noexcept {
    const AttackDefinition& launcher =
        *find_attack_definition(AttackId::launcher);
    ARPG_REQUIRE(launcher.startup_ticks == 7);
    ARPG_REQUIRE(launcher.active_ticks == 4);
    ARPG_REQUIRE(launcher.recovery_ticks == 17);
    ARPG_REQUIRE(launcher.damage == 38);
    ARPG_REQUIRE(launcher.break_damage == 18);
    ARPG_REQUIRE(launcher.impact == ImpactKind::launch);
    ARPG_REQUIRE(arpg::test::near(launcher.knockback_speed, 1.2, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(launcher.launch_speed, 9.5, 1.0e-4));
    ARPG_REQUIRE(launcher.feedback == FeedbackLevel::medium);
    return {};
}
```

`kCases` 保持 3 项，名称改为 `five attacks`、`stable launcher definition` 和 `half-open phases and validation`。

- [ ] **Step 2: 运行 RED，确认现有六攻击目录被捕获**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
```

Expected: FAIL 于 heavy 成员不存在断言及 `kAttackCount == 5`；现有两个枚举仍含 heavy，目录仍为 6。

- [ ] **Step 3: 收缩生产枚举、目录和状态机**

`AttackId` 改为：

```cpp
enum class AttackId : std::uint8_t {
    j1 = 0,
    j2,
    j3,
    launcher,
    air_j,
    none = 0xFF,
};
```

`Action` 改为：

```cpp
enum class Action : std::uint8_t {
    light,
    jump,
    launcher,
};
```

从 `kAttackDefinitions` 删除整条 `AttackId::heavy` 数据。保留的上挑行必须仍为：

```cpp
{AttackId::launcher, 7, 4, 17, 38, 18, ImpactKind::launch,
 {{0.10F,-0.65F,0.00F},{1.40F,0.65F,1.90F}},
 0.12F,1.2F,9.5F,FeedbackLevel::medium},
```

同时把 `attack_catalog.hpp` 的常量改为：

```cpp
inline constexpr std::size_t kAttackCount = 5;
```

玩家地面动作优先级改为 `jump -> launcher -> light`：

```cpp
if (input_buffer_.consume(Action::jump)) {
    player_.velocity.z = kJumpSpeed;
    player_.state = PlayerState::jump_rise;
    airborne = true;
} else if (input_buffer_.consume(Action::launcher)) {
    start_attack(AttackId::launcher);
    return;
} else if (input_buffer_.consume(Action::light)) {
    start_attack(AttackId::j1);
    return;
}
```

从渲染器攻击名称 switch 删除 `AttackId::heavy` 分支，并让 `AttackId::launcher` 返回 `"L"`；`FeedbackLevel::heavy`、J3 重反馈和低频音频路由必须保留。

- [ ] **Step 4: 完整迁移仍引用重击的测试场景**

按下列精确规则修改，保持 Combat 测试总数为 30：

| 文件 | 迁移规则 |
|---|---|
| `input_buffer_tests.cpp` | 用 launcher 替代所有 heavy 队列样本；仍验证最旧匹配、8 tick 过期、容量 32、clear 与诊断计数。 |
| `attack_state_tests.cpp` | 删除 41-tick 重击时间线；保留 28-tick launcher 时间线。动作优先级改为 jump > launcher > light；blocked 场景以 launcher 为当前攻击，缓冲 jump/launcher/light 三项并断言 3 项均在 8 个有效 tick 后过期。空中 J 场景只缓冲 launcher 与 light，light 被消费后剩余 1 项并最终过期。 |
| `hit_resolution_tests.cpp` | 三目标聚合测试改用 launcher，在第 7 tick 命中。HP 断言为 262/412/662，重甲破韧为 102，Hit Stop 为 5，summary 为 medium；轻型与普通进入 airborne，未破韧重甲保持 idle。 |
| `dummy_reaction_tests.cpp` | 倒地边界只由 J3 验证：命中后速度 X 为 5.0、Hit Stop 7 tick、Knockdown 45 个有效 tick、Rising 30 个有效 tick。删除原 heavy 与房间右边界子场景，不删除该测试用例。 |
| `break_stress_tests.cpp` | 统一增加 `launcher_hit_and_finish`，以 startup 7 调用 `start_action_and_reach_hit`。所有破韧场景由 launcher 驱动，reset 也用 launcher 制造非初始状态；压测动作只调度 light/jump/launcher 三类。 |

`launcher_hit_and_finish` 必须为：

```cpp
bool launcher_hit_and_finish(CombatWorld& world) noexcept {
    if (!start_action_and_reach_hit(world, Action::launcher, 7)) {
        return false;
    }
    return finish_attack(world);
}
```

破韧临界测试先完成 6 次 launcher，断言 HP 472、破韧 12、仍为 armored；第 7 次命中后断言：

```cpp
ARPG_REQUIRE(snapshot.dummies[2].hp == 434);
ARPG_REQUIRE(snapshot.dummies[2].break_value == 0);
ARPG_REQUIRE(snapshot.dummies[2].armor == ArmorState::broken);
ARPG_REQUIRE(snapshot.dummies[2].break_window_ticks == 180);
ARPG_REQUIRE(snapshot.dummies[2].reaction == ReactionState::airborne);
ARPG_REQUIRE(arpg::test::near(
    snapshot.dummies[2].velocity.x, 1.2, 1.0e-4));
ARPG_REQUIRE(arpg::test::near(
    snapshot.dummies[2].velocity.z, 9.5, 1.0e-4));
ARPG_REQUIRE(snapshot.dummies[2].hit_stop_ticks == 5);
```

`break_started` 事件必须断言 attack 为 launcher、feedback 为 medium。先运行 5 个冻结 tick，break window 仍为 180；再运行 179 个有效 tick为 1；下一 tick 恢复 armored 和 120 破韧。

Defeated 优先级场景使用 18 次完整 launcher 把 HP 降至 16，再让第 19 次命中致死；断言 HP 0、reaction defeated、速度清零、break window 0、无 break_started 事件，90 个有效 tick 后仍只产生一次 respawned。

压力调度保留原有四个时间槽以维持压力密度，其中两个不同周期都提交 launcher：

```cpp
std::uint8_t expected_action_mask(int tick) noexcept {
    std::uint8_t mask = 0;
    if (tick % 37 == 0) {
        mask |= 1U << 0U;
    }
    if (tick % 181 == 0) {
        mask |= 1U << 1U;
    }
    if (tick % 251 == 0) {
        mask |= 1U << 2U;
    }
    if (tick % 307 == 0) {
        mask |= 1U << 3U;
    }
    return mask;
}
```

`schedule_actions` 对应提交 light、jump、launcher、launcher；251 与 307 周期分别记录 bit 2 和 bit 3。2,400-tick 双回放与 36,000-tick 零分配压力循环长度保持不变。

- [ ] **Step 5: 运行 GREEN 并确认不存在重击符号**

```powershell
cmake --build --preset windows-msvc-debug
& .\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
& .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --preset windows-msvc-debug --output-on-failure
$hits = @(rg -n "Action::heavy|AttackId::heavy" src tests)
if ($hits.Count -ne 0) { $hits; throw "heavy symbols remain" }
```

Expected: Combat `30 cases, 0 failures`；Platform `7 cases, 0 failures`；CTest 5/5；重击符号 0 处。

- [ ] **Step 6: 提交 Combat Core 收缩**

```powershell
git add src/combat src/platform/raylib/combat_renderer.cpp tests/combat
git commit -m "refactor: remove heavy attack"
```

---

### Task 3: 同步正式 Stage 1 文档

**Files:**

- Modify: `docs/superpowers/specs/2026-07-11-raylib-stage1-combat-lab-design.md`
- Modify: `docs/superpowers/plans/2026-07-11-raylib-stage1-combat-lab.md`
- Verify: `docs/superpowers/specs/2026-07-11-launcher-key-remap-design.md`

**Interfaces:**

- Produces: 唯一一致的用户键位和五攻击书面基线。

- [ ] **Step 1: 更新 Stage 1 主设计中的最终规则**

进行以下确定性修改：

- 目标描述从“重击、上挑”改为“上挑”。
- 控制表改为 `L：上挑`，删除 `U` 条目。
- 动作表删除 L 重击行，把 `U 上挑` 改名为 `L 上挑`，数值不变。
- “六种攻击”改为“五种攻击”。
- “重击低频”改为“重反馈低频”，明确当前由 J3 触发，避免和已删除动作混淆。
- 可见验收从 `WASD、J、K、L、U` 改为 `WASD、J、K、L`；删除“L 比 J1 更强”的断言，保留 J3 重反馈断言。
- 把工作树与 Stage 2 边界保持原样。

- [ ] **Step 2: 给历史实施计划添加覆盖声明**

在旧计划标题后添加：

```markdown
> 状态说明：本计划记录最初六攻击 Stage 1 的历史实施过程。当前键位与攻击目录已由 `2026-07-11-launcher-key-remap.md` 和 `../specs/2026-07-11-launcher-key-remap-design.md` 覆盖：重击已删除，L 为上挑，U 无绑定。
```

不重写旧任务的历史命令和旧提交证据。

- [ ] **Step 3: 文档自检**

```powershell
rg -n "L.*重击|U.*上挑|六种攻击|J3 与 L" docs/superpowers/specs/2026-07-11-raylib-stage1-combat-lab-design.md
rg -n "TB[D]|TO[D]O|待[定]" docs/superpowers/specs/2026-07-11-launcher-key-remap-design.md docs/superpowers/plans/2026-07-11-launcher-key-remap.md
git diff --check
```

Expected: 两次内容扫描均无输出；`git diff --check` 无输出。

- [ ] **Step 4: 提交文档同步**

```powershell
git add docs/superpowers/specs/2026-07-11-raylib-stage1-combat-lab-design.md docs/superpowers/plans/2026-07-11-raylib-stage1-combat-lab.md
git commit -m "docs: align stage 1 with launcher binding"
```

---

### Task 4: Fresh 门禁、交叉审查、集成与可见验收

**Files:**

- Create ignored: `.superpowers/sdd/launcher-key-remap-report.md`
- Merge into: `milestone/m01-combat-lab`
- Do not modify: `main` 或任何 Stage 2 分支

**Interfaces:**

- Consumes: Tasks 1-3 的已提交分支状态。
- Produces: 经审查和 Fresh 验证的 Stage 1 里程碑 HEAD。

- [ ] **Step 1: 运行三套 Fresh 门禁**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
ctest --test-dir .\out\build\windows-msvc-debug -N
ctest --test-dir .\out\build\windows-msvc-release -N
```

Expected: Core 4/4；Full Debug 5/5；Full Release 5/5。完整测试名仍恰好为 `core.units`、`combat.units`、`platform.view_math`、`architecture.core_no_raylib`、`architecture.combat_no_raylib`。

- [ ] **Step 2: 运行直接测试和静态边界核验**

```powershell
& .\out\build\windows-msvc-debug\bin\arpg_core_tests.exe
& .\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
& .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
& .\out\build\windows-msvc-release\bin\arpg_core_tests.exe
& .\out\build\windows-msvc-release\bin\arpg_combat_tests.exe
& .\out\build\windows-msvc-release\bin\arpg_platform_tests.exe
rg -n "raylib|raymath|rlgl" src/core src/combat
Get-ChildItem .\out\build\windows-msvc-release -Recurse -Filter 'raylib*.dll'
```

Expected: 两种配置均为 Core 22/0、Combat 30/0、Platform 7/0；核心边界扫描 0 处；raylib DLL 0 个。

- [ ] **Step 3: 运行可见键位矩阵**

启动 Release EXE，逐项验证：

1. `L` 触发 7/4/17 上挑，普通木桩进入自然浮空。
2. `U` 连续按下不改变玩家动作、缓冲或木桩状态。
3. `J` 三段、`K`、`R`、`F1`、`F12`、`Esc` 保持正常。
4. HUD 显示 `L Launcher`，不显示 Heavy 或 U Launcher。
5. F12 截图包含四个对象、状态条和新 HUD。
6. Esc 与关闭按钮各运行一次，退出后 `@(Get-Process arpg_game -ErrorAction SilentlyContinue).Count` 均为 0。

- [ ] **Step 4: 请求最终交叉审查**

审查范围为 `3c75fc3..task/m01-launcher-remap`，重点检查：

- 是否仍存在可触发或不可触发的重击死路径。
- launcher 数值、轻型倍率、重甲破韧和 J3 回归是否保持。
- KEY_U 是否完全退出运行时绑定。
- 36,000 tick 是否仍零分配、零输入/事件溢出。
- 枚举收缩是否造成数组下标、switch 或事件路由遗漏。

Critical 或 Important 发现必须先写失败测试，再修复并重跑 Steps 1-3；最终要求 C0/I0。

- [ ] **Step 5: 写验收报告并检查分支**

报告记录精确命令、测试数量、产物大小、截图路径、GUI 矩阵、审查结论、任务 HEAD 和限制。随后运行：

```powershell
git diff --check 3c75fc3..HEAD
git status --short --branch
git log --graph --decorate --oneline 3c75fc3..HEAD
git worktree list
```

Expected: tracked 状态干净；任务工作树仍保留。

- [ ] **Step 6: 合并到里程碑并复验**

在 `E:\game\.worktrees\m01-combat-lab`：

```powershell
git merge --no-ff task/m01-launcher-remap -m "merge: replace heavy attack with L launcher"
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
& .\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
& .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
git status --short --branch
```

Expected: CTest 5/5、Combat 30/0、Platform 7/0、里程碑工作树干净。停止在 `milestone/m01-combat-lab`，不合并 main，不创建 Stage 2。
