# Stage 5 Progression Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现数据驱动的 1～100 级经验、99 个永久技能点、房间事务化经验结算、兼容存档和等级 HUD。

**Architecture:** 新增独立 `progression` 静态库负责纯升级规则；`DungeonSession` 负责把怪物死亡转成房间待结算经验，并只在清房时生成本次房间进度结果，直到出口事务提交才写入稳定状态。检查点格式升级到 v2，同时读取 v1 为 1 级新角色进度；raylib 只展示快照。

**Tech Stack:** C++17、CMake、Ninja、CTest、raylib 6.0.0、固定容量数据结构。

## Global Constraints

- 等级范围严格为 1～100；从 1 到 100 恰好发放 99 点。
- 经验曲线和奖励值是可替换的 Stage 5 验收数据，不是最终平衡。
- 不实现星盘消费、属性、装备、掉落、词条、死亡或深度经验倍率。
- Combat、Dungeon、Progression 和 Persistence 核心模块不得依赖 raylib。
- 战斗与房间热路径不得新增堆分配。
- Stage 5 在独立 `milestone/m05-progression` 工作树实施，基于当前已验证 HEAD。

---

### Task 1: 纯 progression 核心与验收数据表

**Files:**
- Create: `src/progression/CMakeLists.txt`
- Create: `src/progression/progression_types.hpp`
- Create: `src/progression/progression_rules.hpp`
- Create: `src/progression/progression_rules.cpp`
- Create: `tests/progression/CMakeLists.txt`
- Create: `tests/progression/progression_test_main.cpp`
- Create: `tests/progression/progression_rules_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `ProgressionState { uint8_t level; uint64_t experience; uint8_t earned_passive_points; uint8_t unspent_passive_points; }`.
- Produces: `ProgressionRules` with `std::array<uint64_t, 99> experience_to_next`, `std::array<uint32_t, 8> monster_experience`, and `uint32_t room_clear_experience`.
- Produces: `ProgressionAward apply_experience(ProgressionState, uint64_t, const ProgressionRules&) noexcept`.
- Produces: `bool valid_progression_state(...)` and `bool valid_progression_rules(...)`.

- [ ] **Step 1: Write failing progression tests**

Cover defaults, no-level gain, exact threshold, multi-level gain, huge saturated award, max level, invalid states and rules.

```cpp
ARPG_REQUIRE(result.state.level == 100U);
ARPG_REQUIRE(result.state.earned_passive_points == 99U);
ARPG_REQUIRE(result.state.unspent_passive_points == 99U);
ARPG_REQUIRE(result.state.experience == 0U);
ARPG_REQUIRE(result.levels_gained == 99U);
```

- [ ] **Step 2: Configure and verify RED**

Run Debug configure/build for `arpg_progression_tests`. Expected: target or interfaces do not exist.

- [ ] **Step 3: Implement minimal pure progression library**

Use fixed arrays only. Validate that every threshold is positive, level is 1～100, points never exceed earned points, and earned points equal `level - 1`. Use subtraction per crossed level and clamp at level 100; avoid summing all future thresholds.

- [ ] **Step 4: Verify GREEN**

Run `arpg_progression_tests`. Expected: all progression cases pass with zero warnings.

- [ ] **Step 5: Commit**

```powershell
git add CMakeLists.txt src/progression tests/progression
git commit -m "feat: add deterministic level progression"
```

### Task 2: 房间待结算经验与事务提交

**Files:**
- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_progression.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_support.hpp`
- Create: `tests/dungeon/dungeon_progression_reward_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`

**Interfaces:**
- Consumes: `ProgressionState`, `ProgressionRules`, `CombatEventKind::defeated`, target monster ID from the combat snapshot.
- Produces: `DungeonRunState::progression` as stable committed progress.
- Produces: snapshot fields `progression`, `pending_room_experience`, `last_room_experience`, `last_levels_gained`.

- [ ] **Step 1: Write failing room reward tests**

Use a minimal one-monster room. Verify defeated events add exactly one monster award to `pending_room_experience`, repeat ticks add nothing, and permanent progression remains unchanged until final room clear.

```cpp
ARPG_REQUIRE(during.pending_room_experience == expected_monster_xp);
ARPG_REQUIRE(during.progression.level == 1U);
ARPG_REQUIRE(cleared.pending_room_experience == 0U);
ARPG_REQUIRE(cleared.last_room_experience
    == expected_monster_xp + rules.room_clear_experience);
```

Add reset coverage: reset before clear restores `stable_state_.progression` and zero pending XP. Add transition coverage: `pending_transition.next_state.progression` equals the cleared in-memory result; failed save returns to awaiting exit without re-awarding; successful save preserves it.

- [ ] **Step 2: Build and verify RED**

Run `arpg_dungeon_tests`. Expected: new snapshot/checkpoint fields are absent.

- [ ] **Step 3: Implement session reward state**

Add fixed members:

```cpp
progression::ProgressionRules progression_rules_{};
progression::ProgressionState room_progression_{};
std::uint64_t pending_room_experience_{};
std::uint64_t last_room_experience_{};
std::uint8_t last_levels_gained_{};
```

When relaying each `defeated` event, read the target slot's `MonsterId` before forwarding and saturating-add its configured XP. On final wave clear, add room-clear XP, call `apply_experience`, update `room_progression_`, record last award/levels, and zero pending XP. Do not mutate `stable_state_.progression` yet.

When building door/descent pending state, assign `built.state.progression = room_progression_`. On room reset or construction, restore `room_progression_ = stable_state_.progression` and clear transient reward fields.

- [ ] **Step 4: Update run-state equality and lifecycle tests**

Every `same_run_state` helper and test comparator must include progression. Existing deterministic room generation values remain unchanged.

- [ ] **Step 5: Verify GREEN**

Run `arpg_dungeon_tests`; expected all reward, lifecycle, transaction and 1000-room tests pass with no allocation growth.

- [ ] **Step 6: Commit**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: settle room experience transactionally"
```

### Task 3: Checkpoint v2 与 v1 迁移

**Files:**
- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/save_store.cpp`
- Modify: `src/persistence/save_store.hpp`
- Modify: `tests/persistence/checkpoint_codec_tests.cpp`
- Modify: `tests/persistence/save_store_tests.cpp`
- Modify: `tests/persistence/save_store_fault_tests.cpp`
- Modify: `tests/persistence/dungeon_save_integration_tests.cpp`

**Interfaces:**
- Produces: checkpoint format v2 with progression payload and CRC coverage.
- Consumes: legacy v1 96-byte file and returns identical dungeon state plus `ProgressionState{1,0,0,0}`.
- Produces: v2 encoder only; successful subsequent save rewrites migrated state as v2.

- [ ] **Step 1: Write failing v2 and migration tests**

Add a round-trip state at level 37 with nonzero XP and 36 points. Add invalid level, inconsistent earned points, unspent > earned, XP at/above threshold and corrupted CRC cases. Freeze a valid v1 byte fixture and require migration to level 1.

- [ ] **Step 2: Verify RED**

Run `arpg_persistence_tests`. Expected: v2 fields/size and v1 variable-size decode are unsupported.

- [ ] **Step 3: Implement version-aware codec**

Keep the 32-byte header. Define explicit v1 payload/encoded sizes and v2 payload/encoded sizes. Dispatch decode by header format and payload length before CRC validation. Encode only v2. Validate progression using the same progression rules table used by the game.

- [ ] **Step 4: Make SaveStore read supported sizes safely**

Read the complete file into a fixed maximum-size stack array, accept only exact v1 or v2 sizes, and pass actual byte count to the codec. Preserve dual-slot generation selection and fault classification.

- [ ] **Step 5: Verify GREEN**

Run `arpg_persistence_tests` and dungeon-save integration tests. Expected: v1 migration, v2 round-trip, corruption rejection and committed-transition restart all pass.

- [ ] **Step 6: Commit**

```powershell
git add src/persistence tests/persistence
git commit -m "feat: persist and migrate character progression"
```

### Task 4: HUD、全量压力验证与可视验收

**Files:**
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `tests/platform/dungeon_view_math_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/core_boundary_test.cmake`

**Interfaces:**
- Consumes: `DungeonSnapshot::progression`, `pending_room_experience`, `last_room_experience`, `last_levels_gained`.
- Produces: HUD labels `Level N/100`, `XP current/required` or `XP MAX`, and `Passive Points N`.

- [ ] **Step 1: Write failing HUD-format tests**

Extract pure helpers for level, XP and point labels. Cover level 1, mid-level, pending room XP and level 100.

```cpp
ARPG_REQUIRE(std::strcmp(level_label(state), "Level 37/100") == 0);
ARPG_REQUIRE(std::strcmp(xp_label(maxed, rules), "XP MAX") == 0);
```

- [ ] **Step 2: Verify RED**

Run `arpg_platform_tests`. Expected: progression label helpers do not exist.

- [ ] **Step 3: Implement HUD from snapshot only**

Render level, current/required XP, unspent passive points and pending room XP beneath HP. Do not add interaction or star-chart UI.

- [ ] **Step 4: Run complete verification**

```powershell
cmake --build out/build/windows-msvc-debug --config Debug
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure
```

Expected: all unit, persistence, architecture and 1000-room stress tests pass; progression core and persistence core contain no raylib includes.

- [ ] **Step 5: Launch visual acceptance**

Open `arpg_game.exe`, clear a room, and verify pending XP during combat, level/XP/point changes at clear, and persistence after restart. Save screenshots before and after settlement.

- [ ] **Step 6: Commit and stop**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: present stage 5 progression HUD"
```

Stop on `milestone/m05-progression`; do not start Stage 6.
