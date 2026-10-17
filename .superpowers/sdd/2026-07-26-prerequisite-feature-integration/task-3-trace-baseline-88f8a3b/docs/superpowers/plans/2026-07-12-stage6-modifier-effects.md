# Stage 6 Modifier and Effect Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立确定性、固定容量的 Modifier/Effect/Trigger 核心，并用它替换水系支援护盾与轻型怪 1.25 倍冲量硬编码。

**Architecture:** 新增只依赖 core 的 `arpg_modifiers` 库。属性求值使用 10000 比例的定点整数和稳定 ID 排序；EffectSet 使用固定数组与有界命令队列。Combat 仅负责上下文适配和命令消费，现有外部表现与数值保持不变。

**Tech Stack:** C++17、CMake、CTest、固定容量容器、raylib 6.0.0（仅表现层）。

## Global Constraints

- Modifier/Effect 核心不得依赖 combat、dungeon、progression、persistence 或 raylib。
- `10000 = 100%`，所有乘法使用 64 位中间值、明确舍入和饱和。
- 热路径零堆分配；容量满时拒绝并计数，不覆盖旧状态。
- 水系护盾和轻型怪冲量的迁移前后数值与持续时间不变。
- 不实现装备、星盘、掉落、随机词条或永久 Effect 存档。
- 在独立 `milestone/m06-modifier-effects` 工作树实施并停止。

---

### Task 1: Modifier 定点求值与转换

**Files:**
- Create: `src/modifiers/CMakeLists.txt`
- Create: `src/modifiers/modifier_types.hpp`
- Create: `src/modifiers/modifier_math.hpp`
- Create: `src/modifiers/modifier_math.cpp`
- Create: `tests/modifiers/CMakeLists.txt`
- Create: `tests/modifiers/modifier_test_main.cpp`
- Create: `tests/modifiers/modifier_math_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: stable IDs, `StatId`, `ModifierOperation`, tag/condition masks, `Modifier`, `ModifierContext`.
- Produces: `evaluate_stat(base, stat, modifiers, context, bounds) -> StatEvaluation`.
- Produces: `evaluate_conversions(source_values, modifiers, context) -> ConversionResult`.

- [ ] **Step 1: Write failing math tests**

Cover flat → increased → more order, reduced/less, negative values, bounds, conditional tags, duplicate IDs, insertion-order independence, saturating multiply, and empty identity.

```cpp
// 100 base + 20 flat, +50% increased, 20% more = 216.
ARPG_REQUIRE(result.value == 2160000); // fixed-point units
```

Add conversion tests: 40% + 80% requests convert exactly 100% by stable priority; target does not convert again in the same pass; source==target and negative conversion are invalid.

- [ ] **Step 2: Verify RED**

Configure/build `arpg_modifier_tests`. Expected: requested module and APIs are absent.

- [ ] **Step 3: Implement minimal deterministic math**

Use fixed-capacity `std::array<Modifier, 32>` input views. Copy matching modifier indexes into a fixed local array, apply allocation-free insertion sort by priority then ID, reject duplicate IDs, and use saturating arithmetic. Do not allocate or use exceptions.

- [ ] **Step 4: Verify GREEN**

Run `arpg_modifier_tests`; expected all math and conversion cases pass.

- [ ] **Step 5: Commit**

```powershell
git add CMakeLists.txt src/modifiers tests/modifiers
git commit -m "feat: add deterministic modifier math"
```

### Task 2: 固定容量 EffectSet 与 Trigger 命令

**Files:**
- Create: `src/modifiers/effect_types.hpp`
- Create: `src/modifiers/effect_set.hpp`
- Create: `src/modifiers/effect_set.cpp`
- Create: `tests/modifiers/effect_set_tests.cpp`
- Modify: `src/modifiers/CMakeLists.txt`
- Modify: `tests/modifiers/CMakeLists.txt`
- Modify: `tests/modifiers/modifier_test_main.cpp`

**Interfaces:**
- Produces: `EffectDefinition`, `EffectInstance`, `EffectRefreshRule`, `TriggerSignal`, `EffectCommand`.
- Produces: `EffectSet<8>` behavior through fixed implementation capacity 8 and command capacity 16.
- Produces: apply/refresh/tick/remove/signal, snapshot iteration, overflow diagnostics.

- [ ] **Step 1: Write failing lifecycle tests**

Cover exact duration boundaries, reject, refresh duration, add stack up to maximum, replace weaker, explicit removal, stable iteration, and room-reset clear.

- [ ] **Step 2: Write failing trigger tests**

Verify on_apply/on_refresh/on_expire/on_hit/on_hurt emit commands in stable EffectId/TriggerId order. Fill the queue, require new commands rejected with overflow count. A command produced by a trigger must not recursively run another trigger in the same signal pass.

- [ ] **Step 3: Verify RED**

Build tests; expected Effect APIs are absent.

- [ ] **Step 4: Implement fixed EffectSet**

Use arrays plus active flags and generation-free stable Effect IDs. Stage mutations in the command queue; apply them after iteration. Decrement duration once per effective tick and emit expiry exactly once.

- [ ] **Step 5: Verify GREEN and zero allocations**

Run modifier tests with allocation probe around 36,000 effect ticks; expected zero allocation delta and zero unrequested overflow.

- [ ] **Step 6: Commit**

```powershell
git add src/modifiers tests/modifiers
git commit -m "feat: add fixed capacity effect runtime"
```

### Task 3: 迁移轻型怪冲量与水系护盾

**Files:**
- Modify: `src/combat/monster_pool.hpp`
- Modify: `src/combat/monster_pool.cpp`
- Modify: `src/combat/target_simulation.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Modify: `tests/combat/dummy_reaction_tests.cpp`
- Modify: `tests/combat/monster_melee_tests.cpp`
- Modify: `tests/combat/monster_pool_tests.cpp`
- Create: `tests/combat/modifier_effect_integration_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Consumes: `evaluate_stat` for `StatId::impulse_scale`.
- Consumes: per-monster `EffectSet` and shield commands.
- Produces: `MonsterSnapshot::active_effect_count` and effect overflow diagnostics.

- [ ] **Step 1: Write failing impulse migration tests**

Keep exact existing expectations: light horizontal/vertical impulse is 1.25 times normal, normal/heavy is 1.0, insertion order does not change results, and source contains no `impulse_scale(DummyKind)` hard-coded branch.

- [ ] **Step 2: Write failing water barrier tests**

Support cast applies one `water_barrier`, refreshes duration without duplicates, damage exhaustion removes it, exact expiry clears shield, and reset/new room has zero effects.

- [ ] **Step 3: Verify RED**

Run combat tests; expected effect counts/migration source guard fail.

- [ ] **Step 4: Integrate modifier impulse evaluation**

Create the stable light-target modifier once as constant data. Build context from DummyKind, evaluate 10000 base, and convert the result to the existing float multiplier only at the combat boundary.

- [ ] **Step 5: Integrate water barrier Effect**

Add `EffectSet` to each fixed MonsterRuntime slot. Translate apply/refresh/expire/remove commands to shield fields. Tick effects only on active monsters and clear with the existing pool reset.

- [ ] **Step 6: Verify GREEN**

Run all combat tests including 36,000-tick determinism and allocation probes. Expected existing combat snapshots remain equal except new effect diagnostics.

- [ ] **Step 7: Commit**

```powershell
git add src/combat tests/combat
git commit -m "feat: migrate combat mechanics to effects"
```

### Task 4: 可观测性、架构和全量验收

**Files:**
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `tests/platform/modifier_boundary_test.cmake`
- Modify: `tests/platform/monster_view_tests.cpp`

**Interfaces:**
- Consumes: monster active Effect count and overflow diagnostics.
- Produces: Debug HUD `Effects N  Overflow N`.

- [ ] **Step 1: Write failing architecture and view tests**

Require modifiers source to contain no raylib/combat/dungeon/progression/persistence includes. Verify debug view values come directly from snapshot.

- [ ] **Step 2: Verify RED**

Run focused platform tests; expected new HUD/view helper missing.

- [ ] **Step 3: Implement minimal debug presentation**

Display Effect count and overflow only in F1 debug mode. No gameplay UI or interaction.

- [ ] **Step 4: Run Debug and Release verification**

```powershell
cmake --build out/build/windows-msvc-debug --config Debug
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

Expected: unit, combat, dungeon, persistence, progression, modifier, architecture and stress tests all pass.

- [ ] **Step 5: Visual regression**

Launch game, verify water support shield appears/expires as before, light targets retain stronger launch, and F1 shows Effect diagnostics.

- [ ] **Step 6: Commit and stop**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: expose stage 6 effect diagnostics"
```

Stop on `milestone/m06-modifier-effects`; do not start Stage 7.
