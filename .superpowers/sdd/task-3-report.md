# Stage 11-A Task 3 报告

## 范围与结果

- 基线：`79e2649`。
- 仅实现稳定 `DeathCheckpoint`、Combat 快照转换、结构/Dungeon 语义校验、`same_run_state()` 覆盖和独立 `death_retreat` 目标生成。
- 未实现 Session 死亡事务、V6 codec、继续流程或 UI。
- 稳定枚举、结构和 `valid_death_checkpoint_structural()` 全部位于 `dungeon_checkpoint.hpp`，后者为 `constexpr/noexcept` header-only；Persistence 不依赖或链接 `arpg_dungeon`。

## TDD：RED -> GREEN -> REFACTOR

### RED 1

先新增 9 个 death checkpoint/retreat 用例和 1 个 `same_run_state` 全字段矩阵，再运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
```

修正 MSVC/Windows SDK 环境后，编译按预期因 `dungeon/death_checkpoint.hpp`、`DeathCheckpoint`、`death_sequence` 和 retreat API 未定义而失败。

### GREEN 1

- 实现稳定类型、转换、结构/目录/重生成校验、命名随机域和 overflow fault。
- 构建成功且无编译警告。
- 首次 focused：`dungeon.units` 213 cases 全通过，179.81 秒。
- 稳定状态增大使 Debug 测试目标越过 Windows 默认 1 MiB 栈限制，异常为 `0xC00000FD`，且 trace 证明在 stress 用例进入函数、首条业务语句前崩溃；仅对 `arpg_dungeon_tests` 将栈保留提高到 4 MiB 后原 stress 回归通过。

### RED/GREEN 2

- 自审发现致死快照 `health_loss > 0` 时 barrier 必须已耗尽。
- 先把 `barrier=1` 写成非法结构用例，确认当前实现失败于该断言；再收紧为 `barrier == 0`。
- focused 复验：`dungeon.units` 213 cases 全通过，179.74 秒。

### 最终验证

```powershell
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
```

- 全项目 Debug build：47/47 构建成功。
- 全量 CTest：43/43 通过，0 失败，268.30 秒。
- `architecture.persistence_checkpoint_only`、`architecture.dungeon_no_persistence` 和既有门/下降/深渊/掉落 golden 均通过。
- `git diff --check` 无错误。

## 结构校验矩阵

| 状态/字段 | 规则 |
|---|---|
| `none` | `data_version=0`；死亡位置、伤害、防御、数组全零；生态/伤害类型使用 canonical 默认；来源为 `unknown/0xFF/0`；target 的 index/seed/depth/floor 全为 0，initial/fire/false/false |
| 生命周期/枚举 | 仅接受已声明 lifecycle、source、damage type 和 ecology |
| `pending_continue` 版本/位置 | data version 为 1；死亡 depth、floor 均非零 |
| 来源组合 | attack/projectile 要求 monster 非 sentinel 且 detail=0；hazard/affix 要求 monster 非 sentinel；abyss 要求 sentinel；unknown 要求 sentinel/detail=0 |
| 最后一击 | raw、health loss 非零；`final = barrier_loss + health_loss` 且加法不溢出 |
| 最近承伤 | 五项求和不溢出，且总量不小于最后一击实际承伤 |
| 生命/护盾 | hp=0、max_hp>0；barrier=0、max_barrier>=0 |
| 防御比率 | armor/evasion rate 在 0..10000；元素减伤在 -6000..cap；cap 在 7500..9500 |
| target 结构 | index/seed/depth 非零；floor=0；initial；合法生态；非 abyss；洞口允许普通规则结果 |
| Dungeon 语义 | Monster/Hazard/Affix/Abyss ID 必须在目录；死亡房摘要必须匹配；用冻结输入重生成 target 后逐字段相同 |

## 随机输入/输出规则

- 唯一域：`kDeathRetreatDomain = 0x44454154485F5254ULL`（`DEATH_RT`）。
- 固定混入顺序：死亡房 seed -> domain -> 提交前 `commit_generation` -> 下一 `death_sequence` -> `max(1, death_depth - 1)`。
- 输出 0 固定映射为 `0xD34D5EEDULL`，不重抽。
- global index 固定为死亡房 index + 1；floor index=0；entry=initial；biases 全零；最终强制非 abyss。
- 生态和洞口仍由 `generate_room_descriptor()` 的现有独立普通房流生成。
- room index、commit generation、当前 death sequence 达到 `uint64_t` 最大值时，在加法/构造前返回对应明确 fault；`death_sequence_overflow` 有稳定可读诊断。

## 文件列表

- 新增：`src/dungeon/death_checkpoint.hpp`
- 新增：`src/dungeon/death_checkpoint.cpp`
- 新增：`tests/dungeon/death_checkpoint_tests.cpp`
- 修改：`src/dungeon/dungeon_checkpoint.hpp`
- 修改：`src/dungeon/dungeon_progression.cpp`
- 修改：`src/dungeon/dungeon_rules.hpp`
- 修改：`src/dungeon/room_generation.hpp`
- 修改：`src/dungeon/room_generation.cpp`
- 修改：`src/dungeon/CMakeLists.txt`
- 修改：`tests/dungeon/dungeon_progression_tests.cpp`
- 修改：`tests/dungeon/dungeon_test_main.cpp`
- 修改：`tests/dungeon/CMakeLists.txt`

## 自审与疑虑

- 未发现 Task 3 范围外的 Session/V6 行为改动。
- `death_checkpoint.hpp/.cpp` 承载 Combat/Dungeon 依赖；稳定 `dungeon_checkpoint.hpp` 不包含 Combat 或 DungeonRules。
- 4 MiB 栈调整仅作用于 Dungeon 测试可执行文件，用于容纳固定容量 stress fixture 与增大的稳定状态；生产目标未改链接栈设置。
- 后续 Task 5/6 应按事务前冻结的 generation/death sequence 调用语义重生成，不能使用提交后代数重新抽样。

## Review 修复（2026-07-16，覆盖上文相关旧结论）

### 契约收紧

- `make_death_retreat_target()` 现在只接受 `next_death_sequence == current.death_sequence + 1`；当前序列为 `uint64_t` 最大值时仍优先返回 `death_sequence_overflow`，其余 zero/same/skipped 输入返回稳定可读的 `death_sequence_mismatch`，且不进入随机生成。
- header-only 结构校验新增 armor/evasion 非负、`barrier_loss <= max_barrier`、`health_loss <= max_hp`，并要求 target depth 严格等于 `max(1, death_depth - 1)`。
- `death_floor_room_index=0` 是合法死亡位置；target floor 仍严格为 0。

### TDD 与默认栈修复

- RED：先加入 zero/same/skipped 序列测试，编译按预期因缺少 `death_sequence_mismatch` 失败；实现 enum/name/guard 后 GREEN。
- RED：负 armor、负 evasion、错误 target depth、floor zero 合法、过量 barrier/health loss 六个独立结构用例先确认失败，再收紧校验后 GREEN。
- RED：移除 `/STACK:4194304` 后，默认 1 MiB 栈稳定复现 `0xC00000FD`；trace 将首处定位到 `dungeon_stress::drive_exit` 同一栈帧保留三份大型 `DungeonSnapshot`，拆成 transition/locked/combat 三个 helper 后通过 stress。继续直跑又将第二处定位到单个 abyss reward 用例同时保留四个 `DungeonSession`，拆成四个独立用例；stress 中确需并行对比的双 Session 改为堆持有。
- `/STACK` 覆盖已完全删除；没有提高生产或测试目标的栈保留。

### Review 最终验证

- 默认栈直接运行 `arpg_dungeon_tests.exe`：`223 cases, 0 failures`，包含 1000 房 robot/real-room、被动树和深渊压力场景。
- `ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "^dungeon\\.units$"`：1/1 通过，178.19 秒。
- `ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure`：43/43 通过，0 失败，277.34 秒。
- `git diff --check`：无错误；仅报告现有 LF/CRLF 转换警告。

### Review 后疑虑

- 无已知未解决问题。测试总数由 213 增至 223：7 个死亡契约回归用例，另将原 1 个多 Session 收据用例拆为 4 个（净增 3）。

## 二审测试假阳性修复（2026-07-16）

- 范围仅限 `each_retreat_input_changes_the_named_stream()`，未修改生产代码。
- RED：先为 seed、commit generation、death sequence、depth 四个变体逐一加入 `fault == DungeonFault::none` 断言；旧 sequence 变体仍使用 state `death_sequence=40`、next `42`，运行后精准失败于 `changed_sequence.fault == DungeonFault::none`，证明原先直接比较 fault 结果中的零 seed 会产生假阳性。
- GREEN：base 保持 state `40`、next `41`；sequence 变体改为 state `41`、next `42`。四个变体均先确认无 fault，再比较 seed 不同。
- 增量构建 `arpg_dungeon_tests` 成功；默认栈直接运行：`223 cases, 0 failures`。
- `git diff --check` 无错误，仅有现有 LF/CRLF 转换警告。
