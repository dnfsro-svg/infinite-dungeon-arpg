# Stage 10 Task 4 报告

## 范围与结论

- 实现一次机会生命周期的 `available -> started -> failed`，未实现 clear、奖励物化、领取、放弃或 Task 6/7 的规则数值与环境运行时。
- `available` 只做确定性选择校验、完整强化 encounter plan、reward profile 与首波 Combat 配置预计算；无 `CombatWorld`、无怪物、无封门/战斗事件。
- `abyss_start` 的 verified receipt 成功后才创建 `CombatWorld`；`abyss_fail` 的 verified receipt 成功后才清 `is_abyss` 并以同 seed/ecology/hole 重建普通 encounter。
- Combat 玩家生命可到 0，`player_defeated` 每次生命只发一次；普通死亡与 R/深渊失败共用 `reset_to_normal_room()`。

## TDD 证据

### RED

- Combat：`player_health.damage reaches zero` 首次真实执行失败，实际 HP 为 1；第二项事件测试首次编译失败于缺失 `CombatEventKind::player_defeated`。
- Dungeon 原子启动：6 项启动事务测试首次编译失败于缺失 `PendingSaveKind::abyss_start`；补枚举后，旧回归暴露 transition 后直接 locked、深渊标记仍走普通 director 的旧假设。
- Runtime：新增 6 项首次执行有 4 项失败，分别是 loaded-started 未先提交 failed、自动失败发布未 fault、V4 initial/door migration 未发布 V5。
- 压力回归：定位到两项真实语义冲突：直接重载 started 绕过 Runtime crash gate；自定义 threat=2/max=2 的普通最小房遇深渊预算 3 必然 generation fault。分别改为模拟 Runtime 的 started->failed 与显式选择普通门，没有放宽生产故障语义。

### GREEN / Refactor

- Combat 137 cases、Dungeon 159 cases、Persistence 70 cases、Platform 80 cases 全绿。
- Refactor 后移除重复 abyss affix supplement；Task 5 的 `build_abyss_encounter_plan` 已返回完整强化 plan。
- `DungeonSession`/Runtime 使用 move 避免大 inventory 的额外拷贝；start/fail pending 构造异常进入 fault，不跨越 `noexcept`。
- `git diff --check` 通过；四测试目标构建无警告。

## 完整事务状态表

| 入口/稳定盘面 | 预提交内存状态 | SaveDisposition / receipt | 发布后的稳定盘面 | 可玩副作用 |
|---|---|---|---|---|
| 普通门、`none/failed` | `transition`, committing，旧 Combat 冻结 | committed + exact receipt | 新房 descriptor；若 abyss 门则 `available` | transition 时销毁旧 Combat；目标房尚未构造 |
| 普通门 | 同上 | not_committed | 旧稳定房、awaiting_exit | 无新房副作用，可重试 |
| 普通门 | 同上 | indeterminate / mismatch | faulted | 无可玩新房 |
| `available` 构造 | 校验 seed/depth selection；预计算强化 plan/reward/config；`abyss_start`, committing | 尚未提交 | stable 仍 available | 无 CombatWorld、无怪物 |
| `abyss_start` | cached plan + cached Combat config | committed + exact verified state | 同房 `started`, generation+1 | 此时才 emplace CombatWorld，phase=locked |
| `abyss_start` | 同上 | not_committed | stable 仍 available，session faulted | 无 CombatWorld |
| `abyss_start` | 同上 | indeterminate / generation/state mismatch | stable 仍 available，session faulted | 无 CombatWorld |
| `started` + R/`player_defeated` | `abyss_fail`, committing；Combat 冻结 | 尚未提交 | stable 仍 started | 不清规则、不重建、不重复排队 |
| `abyss_fail` | next: is_abyss=false, lifecycle=failed | committed + exact verified state | 同 seed/ecology/hole 的 failed | 清挑战标记并构造普通 encounter，phase=locked |
| `abyss_fail` | 同上 | not_committed / indeterminate / mismatch | stable 仍 started，session faulted | Combat 对象可留作诊断但不可玩 |
| V5 load started | session 尚未创建 | 自动 failed commit 成功 | failed, generation+1 | 成功后才创建普通 session |
| V5 load started | 同上 | 自动 commit 非 committed / mismatch | runtime faulted | 无 session |
| V1-V4 migrated | helper 后 generation+1，session 尚未创建 | migration commit 成功 | 合法 V5 | 成功后才创建 session；若 available，再单独排 start |
| V1-V4 migrated | 同上 | migration commit 非 committed / mismatch | runtime faulted | 无 session |

## SaveStore fault matrix

`abyss_start` 与 `abyss_fail` 都逐点使用真实 `SaveStore` hook：

| SaveFaultPoint | 典型 disposition | 磁盘允许状态 | Session 允许状态 |
|---|---|---|---|
| before_temp_write | not_committed | old | faulted；start 无 Combat |
| after_temp_write | not_committed | old | faulted；start 无 Combat |
| after_temp_validation | not_committed | old | faulted；start 无 Combat |
| before_publish | not_committed | old | faulted；start 无 Combat |
| after_publish | indeterminate | old 或 new（实测为可恢复稳定状态） | faulted，不可玩 |
| final_scan_a | indeterminate | old 或 new | faulted，不可玩 |
| final_scan_b | indeterminate | old 或 new | faulted，不可玩 |
| before_archive | hook 不属于 commit 路径，commit 正常 | new | exact receipt 后进入 new |

矩阵断言磁盘始终 `same_run_state(old) || same_run_state(new)`；不存在 session 已开始挑战但磁盘仍 available 的可玩状态。

## 事件时序

1. transition receipt：`transition_requested -> transition_committed -> room_destroyed`。
2. 进入 available：下一 tick 只排 `abyss_start`，phase=committing，无 `room_entered/combat_started`。
3. start exact receipt：创建 CombatWorld，phase=locked；下一 tick 才发 `room_entered -> combat_started`。
4. 致死伤害：`player_hit -> player_hurt_started -> player_defeated`，HP=0；后续伤害不再发事件。
5. 普通死亡：relay defeat 后立即调用普通 reset helper，发 `room_reset`。
6. started 深渊死亡/R：先排 `abyss_fail`；提交期间 tick 冻结且 R rejected；exact receipt 后普通重建并发 `room_reset`。

## 文件

- Runtime：`src/dungeon/dungeon_types.hpp`, `dungeon_rules.hpp`, `dungeon_session.*`, `dungeon_transition.cpp`, `src/combat/combat_types.hpp`, `combat_world.cpp`, `src/platform/raylib/dungeon_runtime.cpp`, `raylib_host.cpp`。
- Tests：combat health；dungeon transaction/lifecycle/navigation/stress compatibility；persistence fault matrix；platform runtime migration/crash tests；suite case-count baselines。
- 仅稳定声明未实现副作用：`abyss_clear`, `abyss_reward_materialized`, `abyss_reward_claim`, `abyss_abandon` 及四个新增 fault 值。

## 验证

```text
ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
2/2 passed; dungeon 159 cases, persistence 70 cases

ctest --preset windows-msvc-debug -R '^dungeon.units$' --output-on-failure
refactor 后 fresh 1/1 passed (143.70s)

ctest --preset windows-msvc-debug -R "combat.units|platform.units|architecture" --output-on-failure
21/21 passed; combat 137 cases, persistence 70 cases,
platform 80 cases, architecture 18 tests
```

## 疑虑/后续边界

- `abyss_generation_failed` 对不可组成 1.5x 精确预算的自定义 director 配置是硬 fault，按简报不得降级或重投；普通兼容压力测试因此显式避开 abyss 门。
- Task 8/9 接管 cleared/reward；Task 6/7 接管六条数值规则与三种环境效果。本提交没有提前实现这些行为。
