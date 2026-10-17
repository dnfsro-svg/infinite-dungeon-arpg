# Behavior-Preserving Simplification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 删除 Stage 1 中无效运行时状态和重复测试样板，保持所有游戏行为与测试覆盖不变。

**Architecture:** 测试层集中通用 runner、suite 构造和战斗测试推进辅助函数；压力测试用一张常量表作为调度单一来源。生产层只删除未读取状态，并让攻击推进复用已有阶段函数，不引入新系统或新依赖。

**Tech Stack:** C++17、raylib 6.0.0、CMake 3.25+、Ninja、CTest、MSVC 19.44 x64、Windows SDK 10.0.26100.0、PowerShell。

## Global Constraints

- 只去重复和无效状态，不改变战斗数值、输入、固定步长、碰撞、事件和渲染行为。
- 保留 J 轻击、K 跳跃、L 上挑；不恢复重击。
- 保留 core/combat/platform 的 22/30/7 用例总数守卫。
- 保留压力测试四个调度周期 37/181/251/307，以及两个独立 launcher bit。
- `finish_attack` 的 80/100/128 tick 上限必须由调用点显式传入。
- 不修改 raylib 依赖版本、编译器/SDK 限制、构建目录布局或目标名称。
- 不删除已有文件，不改写无关代码，不合并阴影和角色绘制遍历。

---

### Task 1: 集中测试框架样板

**Files:**
- Modify: `tests/core/test_framework.hpp`
- Modify: `tests/core/test_main.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: all fourteen `*_tests.cpp` files that return a `TestSuite`

**Interfaces:**
- Produces: `make_suite(const char*, const TestCase (&)[N]) noexcept -> TestSuite`
- Produces: `run_suites(const TestSuite (&)[N], int, const char*) noexcept -> int`

- [ ] **Step 1: Confirm the characterization baseline**

Run:

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
```

Expected: 5/5 CTest tests pass.

- [ ] **Step 2: Add the generic suite builder and runner**

Add to `test_framework.hpp`:

```cpp
template <std::size_t CaseCount>
[[nodiscard]] constexpr TestSuite make_suite(
    const char* name,
    const TestCase (&cases)[CaseCount]) noexcept {
    return {name, cases, CaseCount};
}

template <std::size_t SuiteCount>
int run_suites(
    const TestSuite (&suites)[SuiteCount],
    int expected_case_count,
    const char* case_count_label) noexcept {
    int failures = 0;
    int checks = 0;
    for (const auto& suite : suites) {
        for (std::size_t index = 0; index < suite.count; ++index) {
            ++checks;
            const auto failure = suite.cases[index].function();
            if (failure.expression != nullptr) {
                ++failures;
                std::fprintf(stderr, "[FAIL] %s.%s: %s (%s:%d)\n",
                    suite.name, suite.cases[index].name,
                    failure.expression, failure.file, failure.line);
            }
        }
    }
    if (checks != expected_case_count) {
        ++failures;
        std::fprintf(stderr, "[FAIL] %s case count: actual %d != expected %d\n",
            case_count_label, checks, expected_case_count);
    }
    std::printf("%d cases, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
```

Also include `<cstdio>` in this header.

- [ ] **Step 3: Replace the three duplicated main loops**

Each main keeps its suite array and returns one call, for example:

```cpp
return arpg::test::run_suites(suites, 30, "stage 1");
```

Use `22, "stage 0"` for core and `7, "platform"` for platform.

- [ ] **Step 4: Replace manual suite counts**

Each suite function becomes:

```cpp
return arpg::test::make_suite("suite_name", kCases);
```

- [ ] **Step 5: Build and run all three test executables**

Run:

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-debug
.\out\build\windows-msvc-debug\bin\arpg_core_tests.exe
.\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Expected: 22, 30 and 7 cases, all with 0 failures.

- [ ] **Step 6: Commit**

```powershell
git add tests/core tests/combat tests/platform
git commit -m "refactor: centralize test suite execution"
```

### Task 2: 提取战斗测试辅助函数

**Files:**
- Create: `tests/combat/combat_test_support.hpp`
- Modify: `tests/combat/attack_state_tests.cpp`
- Modify: `tests/combat/break_stress_tests.cpp`
- Modify: `tests/combat/dummy_reaction_tests.cpp`
- Modify: `tests/combat/hit_resolution_tests.cpp`

**Interfaces:**
- Produces: `arpg::test::tick_n(CombatWorld&, int, MovementInput) noexcept`
- Produces: `arpg::test::finish_attack(CombatWorld&, int) noexcept -> bool`
- Produces: `arpg::test::drain_events(CombatWorld&) noexcept`

- [ ] **Step 1: Add the focused support header**

```cpp
#pragma once

#include "combat/combat_world.hpp"

namespace arpg::test {

inline void tick_n(
    combat::CombatWorld& world,
    int count,
    combat::MovementInput movement = {}) noexcept {
    for (int tick = 0; tick < count; ++tick) {
        world.tick(movement);
    }
}

[[nodiscard]] inline bool finish_attack(
    combat::CombatWorld& world,
    int max_ticks) noexcept {
    for (int tick = 0; tick < max_ticks; ++tick) {
        if (world.snapshot().player.active_attack == combat::AttackId::none) {
            return true;
        }
        world.tick(combat::MovementInput{});
    }
    return world.snapshot().player.active_attack == combat::AttackId::none;
}

inline void drain_events(combat::CombatWorld& world) noexcept {
    while (world.try_pop_event().has_value()) {
    }
}

}  // namespace arpg::test
```

- [ ] **Step 2: Replace local helper copies**

Include the header, delete the local implementations, and import only the helpers each file uses. Preserve explicit limits:

```cpp
using arpg::test::finish_attack;
using arpg::test::tick_n;

ARPG_REQUIRE(finish_attack(world, 128));
```

Use 80 in `hit_resolution_tests.cpp`, 100 in `break_stress_tests.cpp`, and 128 in `dummy_reaction_tests.cpp`.

- [ ] **Step 3: Build and run combat tests**

Run:

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-debug
.\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
```

Expected: 30 cases, 0 failures.

- [ ] **Step 4: Commit**

```powershell
git add tests/combat
git commit -m "refactor: share combat test helpers"
```

### Task 3: 数据化压力测试动作调度

**Files:**
- Modify: `tests/combat/break_stress_tests.cpp`

**Interfaces:**
- Produces: `ScheduledActions schedule_actions(CombatWorld&, int) noexcept`
- `ScheduledActions` contains `requested` and `accepted` masks.

- [ ] **Step 1: Replace duplicated rules with one table**

```cpp
struct ScheduledAction final {
    int period;
    Action action;
    std::uint8_t bit;
};

struct ScheduledActions final {
    std::uint8_t requested{};
    std::uint8_t accepted{};
};

constexpr std::array<ScheduledAction, 4> kScheduledActions{{
    {37, Action::light, static_cast<std::uint8_t>(1U << 0U)},
    {181, Action::jump, static_cast<std::uint8_t>(1U << 1U)},
    {251, Action::launcher, static_cast<std::uint8_t>(1U << 2U)},
    {307, Action::launcher, static_cast<std::uint8_t>(1U << 3U)},
}};

ScheduledActions schedule_actions(CombatWorld& world, int tick) noexcept {
    ScheduledActions result{};
    for (const auto& scheduled : kScheduledActions) {
        if (tick % scheduled.period != 0) {
            continue;
        }
        result.requested |= scheduled.bit;
        if (world.queue_action(scheduled.action)) {
            result.accepted |= scheduled.bit;
        }
    }
    return result;
}
```

- [ ] **Step 2: Compare requested and accepted masks at callers**

For each replay world compare both fields; for stress accumulate:

```cpp
const ScheduledActions scheduled = schedule_actions(stress, tick);
all_queued = scheduled.accepted == scheduled.requested && all_queued;
```

- [ ] **Step 3: Run the combat test executable twice**

Run:

```powershell
.\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
.\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
```

Expected: both runs report 30 cases, 0 failures; deterministic stress remains stable.

- [ ] **Step 4: Commit**

```powershell
git add tests/combat/break_stress_tests.cpp
git commit -m "refactor: centralize stress action schedule"
```

### Task 4: 删除无效运行时状态并复用阶段结果

**Files:**
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/player_simulation.cpp`
- Modify: `src/combat/target_simulation.cpp`
- Modify: `src/combat/hit_resolution.cpp`
- Modify: `tests/combat/hit_resolution_tests.cpp`

**Interfaces:**
- No new public interface.
- `CombatSnapshot` and event output remain unchanged.

- [ ] **Step 1: Delete fields and their writes**

Remove `AttackRuntime::serial`, `DummyRuntime::pending_impact`, `DummyRuntime::has_pending_impact`, all assignments to them, and rename the stale test label from `one target once per attack serial` to `one target once per attack`.

- [ ] **Step 2: Use one attack phase result**

Replace the duplicated total-tick calculation with:

```cpp
++attack_.elapsed_ticks;
const AttackPhase phase =
    attack_phase_at(*definition, attack_.elapsed_ticks);
if (phase == AttackPhase::finished) {
    finish_attack();
    advance_vertical(false);
    return;
}

player_.state = state_for_phase(phase);
```

- [ ] **Step 3: Build and run combat tests**

Run:

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-debug
.\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
```

Expected: 30 cases, 0 failures; compile with `/W4 /permissive-` without new warnings.

- [ ] **Step 4: Commit**

```powershell
git add src/combat tests/combat/hit_resolution_tests.cpp
git commit -m "refactor: remove unused combat runtime state"
```

### Task 5: 全量验证和最终审查

**Files:**
- Verify only; no planned source changes.

**Interfaces:**
- Consumes all prior task outputs.

- [ ] **Step 1: Run all supported configurations**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
.\scripts\Test.ps1 -Preset windows-msvc-release
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
```

Expected: every preset configures, builds and passes all registered CTest tests.

- [ ] **Step 2: Verify direct case counts**

```powershell
.\out\build\windows-msvc-debug\bin\arpg_core_tests.exe
.\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Expected: `22 cases, 0 failures`, `30 cases, 0 failures`, `7 cases, 0 failures`.

- [ ] **Step 3: Run launcher smoke**

Start `out/build/windows-msvc-debug/bin/arpg_game.exe` hidden, confirm it stays alive briefly, then terminate only that spawned process.

- [ ] **Step 4: Inspect scope and line reduction**

```powershell
git diff --check
git status --short
git diff --stat 265fcc2e5900bcb7601168ef6797530626af50cf..HEAD
```

Expected: no whitespace errors; only the files named in this plan plus its design/plan documents changed.

- [ ] **Step 5: Request independent final code review**

Reviewer checks behavior preservation, exact test guards, explicit finish limits, stress order, dead-field proof and absence of unrelated refactors.
