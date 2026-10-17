# Whole-Game Behavior-Preserving Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 重构并精简当前整个游戏代码，在不改变玩法、数值、随机结果、存档兼容或操作方式的前提下，拆分大型文件、集中重复逻辑并收紧模块职责。

**Architecture:** 基于 Stage 6 最终提交创建独立工作树，按 Combat、Dungeon、raylib、Persistence、公共模块、全局收尾六个审查门顺序执行。每个审查门先用现有测试和新增特征测试锁定行为，再移动职责、删除重复并独立提交。

**Tech Stack:** C++17、raylib 6.0、CMake、Ninja、CTest、MSVC、Git worktree。

## Global Constraints

- 不改变 WASD、J、K、L、E、F1、F12、R、Esc 的含义。
- 不改变战斗数值、攻击 tick、随机调用顺序、房间规则、HUD 语义或存档字节格式。
- 核心模块不得依赖 raylib；战斗热路径不得增加堆分配。
- 不实现 Stage 7 或任何新玩法，不引入新依赖。
- 不设置代码行数硬指标，不通过压缩格式降低行数。
- 每项重构必须先有现有测试或新增特征测试保护。
- 每张任务卡只在 Debug 全量测试通过后提交；最终再运行 Release 全量测试。

---

### Task 1: 建立独立重构工作树与基线报告

**Files:**
- Create: `docs/refactor/2026-07-12-baseline.md`
- Verify: `.gitignore`

**Interfaces:**
- Consumes: Stage 6 commit `3e4b861`。
- Produces: branch `milestone/whole-game-refactor`、独立工作树 `.worktrees/whole-game-refactor`、可复查基线报告。

- [ ] **Step 1: 验证仓库与工作树前置条件**

```powershell
git rev-parse --show-toplevel
git status --short
git worktree list
git check-ignore .worktrees/review-probe
git rev-parse 3e4b861^{commit}
```

Expected: 仓库有效、Stage 6 工作树干净、`.worktrees/review-probe` 被 `/.worktrees/` 忽略规则匹配、基线提交存在。目录本身不作为 `git check-ignore` 探针，因为以斜杠结尾的目录规则需要对子路径进行匹配验证。

- [ ] **Step 2: 创建独立工作树**

```powershell
git worktree add E:\game\.worktrees\whole-game-refactor -b milestone/whole-game-refactor 4abc386
```

Expected: 新工作树位于指定路径，分支为 `milestone/whole-game-refactor`。

- [ ] **Step 3: 记录生产代码基线**

在 `docs/refactor/2026-07-12-baseline.md` 写入：

```markdown
# Whole-Game Refactor Baseline

- Base commit: 3e4b861
- Production scope: src/**/*.cpp, src/**/*.hpp
- Required invariants: controls, combat ticks, RNG, saves, HUD, zero-allocation hot paths
- Largest-file table: generated from the command below
- Dependency table: generated from src/**/CMakeLists.txt
```

运行：

```powershell
Get-ChildItem src -Recurse -Include *.cpp,*.hpp | ForEach-Object { [pscustomobject]@{Lines=(Get-Content $_.FullName).Count;Path=$_.FullName} } | Sort-Object Lines -Descending
rg -n "add_library|target_link_libraries" src --glob CMakeLists.txt
```

- [ ] **Step 4: 运行完整基线**

```cmd
call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Expected: 22/22 tests passed。

- [ ] **Step 5: 提交基线报告**

```powershell
git add docs/refactor/2026-07-12-baseline.md
git commit -m "docs: record whole-game refactor baseline"
```

---

### Task 2: 重构 CombatWorld 与怪物 AI

**Files:**
- Create: `src/combat/combat_effects.cpp`
- Create: `src/combat/combat_snapshot.cpp`
- Create: `src/combat/monster_ai_common.hpp`
- Create: `src/combat/monster_ai_melee.cpp`
- Create: `src/combat/monster_ai_ranged.cpp`
- Create: `src/combat/monster_ai_special.cpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Test: `tests/combat/break_stress_tests.cpp`
- Test: `tests/combat/monster_melee_tests.cpp`
- Test: `tests/combat/monster_ranged_tests.cpp`
- Test: `tests/combat/monster_special_tests.cpp`

**Interfaces:**
- Consumes: `CombatWorld`, `MonsterRuntime`, `modifiers::EffectSet` 的现有公开行为。
- Produces: 私有 `find_effects`, `ensure_effects`, `apply_effect_commands`, `build_combat_snapshot`；`CombatWorld` 公开 API 不变。

- [ ] **Step 1: 添加状态等价特征测试**

在 `break_stress_tests.cpp` 增加两类固定输入测试。第一类可保留两个世界运行 3600 tick 的确定性回放检查，但不得把它作为重构前后等价证据。第二类必须是单世界非 legacy 黄金测试：构造 `CombatEncounterConfig`，明确包含近战、远程/支援、特殊机制和水系护盾目标；以重构前基线记录的公开 `CombatSnapshot`、事件顺序与诊断值为固定断言。使用现有 `queue_action(Action)` 和 `tick(MovementInput)`，不新增公开 API：

```cpp
CombatWorld world{mixed_role_golden_config()};
for (std::uint32_t tick = 0; tick <= 180U; ++tick) {
    if (tick % 90U == 0U) {
        ARPG_REQUIRE(world.queue_action(Action::light));
    }
    world.tick(MovementInput{
        static_cast<std::int8_t>(tick % 120U < 60U ? 1 : -1), 0});
}
ARPG_REQUIRE(tick_45.monsters[0].shield == 90);
ARPG_REQUIRE(tick_45.hazard_count == 1U);
ARPG_REQUIRE(tick_180.monsters[0].shield == 0);
```

- [ ] **Step 2: 运行 Combat 特征测试基线**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
ctest --test-dir out/build/windows-msvc-debug -R 'combat.units' --output-on-failure
```

Expected: PASS；测试在重构前锁定当前行为。

- [ ] **Step 3: 提取 Effect 拥有者与命令处理**

把 `find_effects`、`ensure_effects` 和护盾命令循环移动到 `combat_effects.cpp`。新增私有接口：

```cpp
modifiers::EffectSet* find_effects(std::size_t monster_slot) noexcept;
modifiers::EffectSet* ensure_effects(std::size_t monster_slot) noexcept;
void apply_effect_commands(
    MonsterRuntime& monster,
    modifiers::EffectSet& effects) noexcept;
```

水系护盾定义集中为一个返回值函数，避免 apply/refresh/expire 配置散落：

```cpp
modifiers::EffectDefinition water_barrier_effect(
    const MonsterRuntime& target) noexcept;
```

- [ ] **Step 4: 提取快照组装**

将 `CombatWorld::snapshot()` 完整移动到 `combat_snapshot.cpp`，保持字段顺序和聚合值不变。`combat_world.cpp` 只保留构造、初始化、update 和会话编排。

- [ ] **Step 5: 按职责拆分怪物 AI**

`monster_ai_common.hpp` 只提供无状态原语：

```cpp
float target_distance(Vec3 from, Vec3 to) noexcept;
void face_toward(MonsterRuntime& monster, Vec3 target) noexcept;
bool tick_down(std::uint16_t& ticks) noexcept;
```

将近战追击与接触攻击移入 `monster_ai_melee.cpp`，远程/支援选择移入 `monster_ai_ranged.cpp`，冲锋、自爆、地面危险与传送移入 `monster_ai_special.cpp`。`simulate_monster()` 保留死亡、受击状态和分类调度。

- [ ] **Step 6: 运行 Combat 与架构测试**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
ctest --test-dir out/build/windows-msvc-debug -R 'combat.units|architecture.combat|architecture.modifiers' --output-on-failure
```

Expected: 所有选择测试通过，零分配断言不变。

- [ ] **Step 7: 运行 Debug 全量并提交**

```powershell
ctest --preset windows-msvc-debug
git diff --check
git add src/combat tests/combat
git commit -m "refactor: separate combat simulation responsibilities"
```

Expected: 22/22 passed。

---

### Task 3: 重构 DungeonSession 与刷怪导演

**Files:**
- Create: `src/dungeon/dungeon_transition.cpp`
- Create: `src/dungeon/dungeon_snapshot.cpp`
- Create: `src/dungeon/encounter_budget.cpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/encounter_director.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Test: `tests/dungeon/dungeon_transaction_tests.cpp`
- Test: `tests/dungeon/dungeon_stress_tests.cpp`
- Test: `tests/dungeon/encounter_director_tests.cpp`

**Interfaces:**
- Consumes: `DungeonSession::update`, `commit_exit`, `snapshot` 和现有 director 输入。
- Produces: 私有 `prepare_transition`, `commit_transition`, `build_dungeon_snapshot`, `compute_encounter_budget`；公开 API 不变。

- [ ] **Step 1: 增加固定种子黄金特征测试**

对固定根种子连续生成 256 个房间，把每个房间的深度、生态、怪物 ID/数量、洞口、深渊标志和下一种子写入结构数组，并让第二次运行逐项相等：

```cpp
const auto first = generate_room_trace(0x6d5a56da1234ULL, 256U);
const auto second = generate_room_trace(0x6d5a56da1234ULL, 256U);
ARPG_REQUIRE(first == second);
```

- [ ] **Step 2: 运行 Dungeon 基线**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --test-dir out/build/windows-msvc-debug -R 'dungeon.units' --output-on-failure
```

- [ ] **Step 3: 提取事务切换**

将出口/洞口选择、下一房种子确定、持久化提交和当前房销毁移动到 `dungeon_transition.cpp`。保留调用顺序：奖励已结算 → 计算下一状态 → 持久化切换提交 → 销毁当前战斗 → 加载下一房。

- [ ] **Step 4: 提取 Dungeon 快照**

将快照聚合移动到 `dungeon_snapshot.cpp`，统一 `cleared` 与 `awaiting_exit` 的展示条件，但不合并两个生命周期状态。

- [ ] **Step 5: 拆分刷怪预算与选择**

新增私有纯函数（保留当前公式的真实输入）：

```cpp
namespace arpg::dungeon::detail {
[[nodiscard]] std::uint8_t compute_encounter_budget(
    std::uint64_t depth,
    const EncounterDirectorConfig& config) noexcept;
}
```

函数只搬运现有预算纯算术；`encounter_director.cpp` 保留候选过滤、稳定排序和 RNG 抽取，调用 RNG 的次数与顺序不得改变。

- [ ] **Step 6: 运行压力与全量测试并提交**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --test-dir out/build/windows-msvc-debug -R 'dungeon.units|architecture.dungeon' --output-on-failure
ctest --preset windows-msvc-debug
git diff --check
git add src/dungeon tests/dungeon
git commit -m "refactor: separate dungeon lifecycle responsibilities"
```

Expected: 1000 房压力路径、确定性轨迹和 Debug 22/22 全部通过。

---

### Task 4: 拆分 raylib 渲染器与主循环

**Files:**
- Create: `src/platform/raylib/room_renderer.cpp`
- Create: `src/platform/raylib/actor_renderer.cpp`
- Create: `src/platform/raylib/hud_renderer.cpp`
- Create: `src/platform/raylib/debug_renderer.cpp`
- Create: `src/platform/raylib/render_layout.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Test: `tests/platform/combat_view_math_tests.cpp`
- Test: `tests/platform/dungeon_view_math_tests.cpp`
- Test: `tests/platform/dungeon_runtime_tests.cpp`

**Interfaces:**
- Consumes: `DungeonSnapshot`, `CombatSnapshot`, `CombatRenderer::draw`。
- Produces: 私有 `draw_room`, `draw_actors`, `draw_hud`, `draw_debug_overlay`；`CombatRenderer` 对外调用方式不变。

- [ ] **Step 1: 锁定布局纯函数**

为 HUD 行距、门提示位置、世界到屏幕投影和调试层起始位置增加无窗口测试：

```cpp
const auto layout = make_hud_layout(1024, 576, true);
ARPG_REQUIRE(layout.line_height == 23);
ARPG_REQUIRE(layout.debug_origin.x == 30);
ARPG_REQUIRE(layout.debug_origin.y > layout.status_origin.y);
```

- [ ] **Step 2: 运行 Platform 基线**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R 'platform.view_math|platform.input_latency_source' --output-on-failure
```

- [ ] **Step 3: 拆分四个绘制职责**

移动代码而不改变 Draw 调用相对顺序。共享 `RenderLayout` 只保存坐标、行距和颜色，不持有 raylib 资源。`combat_renderer.cpp` 变成按原顺序调用四个绘制函数的编排层。

- [ ] **Step 4: 精简主循环**

在 `raylib_host.cpp` 中保持 `IsKeyPressed` 采样仍发生在每个渲染帧开头。将输入映射和 screenshot/debug 切换提取为局部函数，不把 raylib 输入传入核心规则层。

- [ ] **Step 5: 构建、测试并真实启动**

```powershell
cmake --build --preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -R 'platform|architecture' --output-on-failure
```

使用 Computer Use 启动 `out/build/windows-msvc-debug/bin/arpg_game.exe`，验证 WASD、J、K、L、E、F1 与 F12。窗口持续运行至少 10 秒。

- [ ] **Step 6: 全量测试并提交**

```powershell
ctest --preset windows-msvc-debug
git diff --check
git add src/platform tests/platform
git commit -m "refactor: split raylib rendering responsibilities"
```

---

### Task 5: 重构 Persistence 与编解码

**Files:**
- Create: `src/persistence/save_slot.cpp`
- Create: `src/persistence/save_transaction.cpp`
- Create: `src/persistence/save_recovery.cpp`
- Modify: `src/persistence/save_store.cpp`
- Modify: `src/persistence/save_store.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/CMakeLists.txt`
- Test: `tests/persistence/save_store_tests.cpp`
- Test: `tests/persistence/save_store_fault_tests.cpp`
- Test: `tests/persistence/checkpoint_codec_tests.cpp`

**Interfaces:**
- Consumes: 现有 `SaveStore`、checkpoint v1/v2 解码和故障注入接口。
- Produces: 私有槽检查、事务写入和恢复函数；公开存档接口与字节格式不变。

- [ ] **Step 1: 增加字节黄金与恢复特征测试**

用固定 checkpoint 编码两次并逐字节比较；保存过程中在每个现有故障注入点失败，再重新打开并断言选择最后一个完整提交：

```cpp
const auto encoded = encode_checkpoint(fixed_checkpoint());
ARPG_REQUIRE(encoded == encode_checkpoint(fixed_checkpoint()));
ARPG_REQUIRE(decode_checkpoint(encoded).value == fixed_checkpoint());
```

- [ ] **Step 2: 运行 Persistence 基线**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_persistence_tests
ctest --test-dir out/build/windows-msvc-debug -R 'persistence.units|architecture.persistence' --output-on-failure
```

- [ ] **Step 3: 分离槽位、事务和恢复**

`save_slot.cpp` 负责读取、校验与 generation 比较；`save_transaction.cpp` 负责临时写入、flush、替换和回滚；`save_recovery.cpp` 负责双槽选择与错误映射。`SaveStore` 只编排这些步骤。

- [ ] **Step 4: 精简编解码验证**

将重复的读取边界检查集中为局部 cursor 类型：

```cpp
struct DecodeCursor final {
    const std::byte* data{};
    std::size_t size{};
    std::size_t offset{};
    bool read_u32(std::uint32_t& value) noexcept;
    bool read_u64(std::uint64_t& value) noexcept;
};
```

不得改变字段顺序、CRC 范围或 v1/v2 迁移结果。

- [ ] **Step 5: 全量测试并提交**

```powershell
ctest --test-dir out/build/windows-msvc-debug -R 'persistence.units|dungeon.units|architecture.persistence' --output-on-failure
ctest --preset windows-msvc-debug
git diff --check
git add src/persistence tests/persistence
git commit -m "refactor: separate save transaction responsibilities"
```

---

### Task 6: 精简 Core、Progression 与 Modifier 公共模块

**Files:**
- Modify: `src/core/bounded_queue.hpp`
- Modify: `src/core/fixed_pool.hpp`
- Modify: `src/progression/progression_rules.cpp`
- Modify: `src/modifiers/modifier_math.cpp`
- Modify: `src/modifiers/effect_set.cpp`
- Test: `tests/core/bounded_queue_tests.cpp`
- Test: `tests/core/fixed_pool_tests.cpp`
- Test: `tests/progression/progression_rules_tests.cpp`
- Test: `tests/modifiers/modifier_math_tests.cpp`
- Test: `tests/modifiers/effect_set_tests.cpp`

**Interfaces:**
- Consumes: 所有现有公共模板和规则函数。
- Produces: 相同公开接口；只删除内部重复和统一已有算法。

- [ ] **Step 1: 补齐极值特征测试**

覆盖空/满容量、generation 回绕、定点溢出、重复 Modifier ID、100 级经验饱和和 Effect 命令溢出。使用现有断言风格，不改变期望值。

- [ ] **Step 2: 运行公共模块基线**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_core_tests arpg_progression_tests arpg_modifier_tests
ctest --test-dir out/build/windows-msvc-debug -R 'core.units|progression.units|modifiers.units' --output-on-failure
```

- [ ] **Step 3: 只提取有两个调用方的重复算法**

允许集中稳定插入排序、饱和加法和固定容量查找；禁止增加万能工具头。若一个辅助只有单一调用方，则保留为对应 `.cpp` 的匿名命名空间函数。

- [ ] **Step 4: 验证公开 API 与零分配**

```powershell
ctest --test-dir out/build/windows-msvc-debug -R 'core.units|progression.units|modifiers.units|combat.units' --output-on-failure
```

- [ ] **Step 5: 全量测试并提交**

```powershell
ctest --preset windows-msvc-debug
git diff --check
git add src/core src/progression src/modifiers tests/core tests/progression tests/modifiers
git commit -m "refactor: simplify shared runtime primitives"
```

---

### Task 7: 全局收尾、双配置验证与结果报告

**Files:**
- Create: `docs/refactor/2026-07-12-result.md`
- Modify: only files with proven unused private declarations or stale comments

**Interfaces:**
- Consumes: Tasks 1–6 的全部提交。
- Produces: 干净分支、Debug/Release 验证证据、重构前后对比报告。

- [ ] **Step 1: 扫描残余重复和边界**

```powershell
rg -n "TODO|TBD|compatibility|temporary" src
rg -n "#include.*raylib" src/core src/combat src/dungeon src/persistence src/progression src/modifiers
git diff 3e4b861 --stat
```

只删除能由编译器、引用搜索和测试共同证明无用的私有声明。

- [ ] **Step 2: 运行 Debug 完整验证**

```cmd
call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Expected: 全部测试通过，包括 1000 房压力测试。

- [ ] **Step 3: 运行 Release 完整验证**

```cmd
call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

Expected: 全部测试通过。

- [ ] **Step 4: 真实窗口验收**

用 Computer Use 启动 Release EXE，执行 WASD、J、K、L、E、F1，连续运行 10 秒并确认窗口存活。Esc 只在验收结束时发送。

- [ ] **Step 5: 写结果报告**

在 `docs/refactor/2026-07-12-result.md` 写入：基线/最终提交、生产文件总行数、最大 15 个文件、模块依赖、删除的重复类别、Debug/Release 测试数量与耗时、窗口验收结果、已知未改变的兼容接口。

- [ ] **Step 6: 最终提交**

```powershell
git diff --check
git status --short
git add docs/refactor src tests
git commit -m "docs: report whole-game refactor results"
git status --short
```

Expected: 工作树干净。停止在 `milestone/whole-game-refactor`，不进入 Stage 7。
