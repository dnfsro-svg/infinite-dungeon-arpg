# Stage 1 Task 4 实施报告：Attack State Machine

## 状态

- Status: COMPLETE
- 工作树：`E:\game\.worktrees\m01-combat-core`
- 分支：`task/m01-combat-core`
- 基线：`7c660d6` (`feat: add deterministic combat movement and jump`)
- 提交：`1ac0a8e` (`feat: add combat attack state machine`)
- 本报告为 `.superpowers/sdd` scratch，不纳入提交。

## TDD 证据

### 已继承的 RED

前一执行代理先只写入四个 `attack_state_tests.cpp` 行为用例及 CMake/test-main 接线，并在全新 `out/build-task4-red` 运行真实测试。生产文件当时未参与 RED。

```text
15 cases, 4 failures
```

四个失败分别证明旧 Task 3 行为缺少 Task 4 能力：J1 未启动、攻击 elapsed 未推进、heavy 优先级动作未消费、air_j 未启动。接手后未重写、删除或弱化任何测试断言。

### GREEN

使用全新目录配置和全量构建（MSVC 19.44、SDK 10.0.26100.0、Core-only）：

```powershell
cmake.exe --preset windows-msvc-core-debug -B out/build-task4-green
cmake.exe --build out/build-task4-green
```

首次构建结果：21 个 build steps 全部完成，无 C++ 编译 warning。

Focused Combat runner：

```powershell
.\out\build-task4-green\bin\arpg_combat_tests.exe
```

```text
15 cases, 0 failures
```

完整 Core-only CTest（提升上下文只用于写入该 fresh 目录的 Testing 日志；未修改 ACL）：

```powershell
ctest.exe --test-dir out/build-task4-green --output-on-failure --no-tests=error
```

```text
1/4 core.units ....................... Passed
2/4 combat.units ..................... Passed
3/4 architecture.core_no_raylib ...... Passed
4/4 architecture.combat_no_raylib .... Passed
100% tests passed, 0 tests failed out of 4
```

`git diff --check` 无空白错误；仅有 Git 的 LF/CRLF 工作树提示。

## 实现文件

- `src/combat/player_simulation.cpp`：攻击启动/推进/结束、阶段映射、J1/J2 whiff continuation、动作优先级、空中攻击配额、一次性 lunge，以及既有移动/跳跃逻辑的迁移。
- `src/combat/combat_world.cpp`：tick 后 input aging；player hit-stop 冻结门禁；保留 world reset/snapshot。
- `src/combat/CMakeLists.txt`：接入 `player_simulation.cpp`。
- `tests/combat/attack_state_tests.cpp`：四个 Task 4 行为测试。
- `tests/combat/CMakeLists.txt`、`tests/combat/combat_test_main.cpp`：注册 suite，精确 case 门禁 11 -> 15。

`AttackRuntime` 所需字段已由当前基线的 `combat_world.hpp` 提供，无需重复修改头文件。

## 规格核对

- 攻击启动 tick：消费动作并输出 `elapsed_ticks == 0`；之后仅非冻结 tick 递增。
- cancel 判定使用 tick 开始时的 elapsed；J1 whiff 在 `>=13`、J2 whiff 在 `>=15` 才消费 buffered J。
- J3 不连回；J3/heavy/launcher 总帧结束后回 Idle；air_j 结束后按 Z 速度回 rise/fall。
- 地面 Idle/Move 严格按 jump -> heavy -> launcher -> light 消费；攻击中拒绝非法 K/L/U/J cancel。
- 空中只消费 light，且 `air_attack_available` 每次滞空只允许一次，落地恢复。
- 每次启动生成非零 serial，清理 connected/event/hit-target flags，只施加一次 lunge，并记录 combo stage。
- Task 4 没有命中逻辑，`connected` 始终 false；没有新增碰撞、伤害、event、reaction 或 raylib 逻辑。
- 输入先尝试消费，再在 tick 末 aging；player hit-stop tick 不推进状态机且调用 `age(true)`。

## 自审与交接说明

- 四个测试覆盖六条攻击时间线、J1/J2 精确 whiff 窗口、J3 terminal、地面优先级/非法 cancel、air_j 单次配额与落地重置。
- 时间线总帧使用 `uint32_t` 累加，避免窄类型加法；attack id 仅由已校验 catalog 启动，snapshot 不存在空 definition 解引用路径。
- 启动、连段和结束都在单一 tick 内返回，避免同 tick 额外推进或重复 lunge。
- 交接后检测到被中断的原 Task 4 代理已并行落盘生产实现；本代理未覆盖其有效内容，只补齐冻结门禁，并完成逐项审计、fresh GREEN、范围检查与最终提交。
- 独立 reviewer 子线程因线程槽位上限无法创建；已完成逐行自审。无代码或规格疑虑。

## 正式审查修复：actor-local Hit Stop

- 审查发现：Task 4 提交 `1ac0a8e` 的 `CombatWorld::tick()` 在 player hit-stop 分支直接 `return`。这会让 Task 5/6 加入的未冻结木桩更新、落地/复位、碰撞和事件聚合等后续世界阶段也被跳过，把演员局部冻结错误扩大成整个 world tick 冻结。
- 设计证据：`docs/superpowers/specs/2026-07-11-raylib-stage1-combat-lab-design.md:135` 明确 Hit Stop 通过演员自己的冻结计数实现，未命中的其他木桩继续更新；`docs/superpowers/plans/2026-07-11-raylib-stage1-combat-lab.md:79-85` 明确玩家、各木桩及后续世界阶段分别推进。
- 最小修复：tick 开始记录 `player_frozen`；冻结时只递减玩家计数，未冻结时才调用 `simulate_player()`；随后统一执行 `input_buffer_.age(player_frozen)`，不再从玩家冻结分支提前返回。
- 测试限制：Task 4 没有命中/碰撞实现，公共 API 无法设置 `PlayerRuntime::hit_stop_ticks`，也没有木桩运动阶段可观察。因此无法在不增加测试专用公共 API、私有布局 hack 或提前实现 Task 5/6 的前提下构造该状态；按审查要求不扩大范围、不添加测试钩子，使用现有行为回归与静态控制流审查。
- Fresh 验证目录：`out/build-task4-review-fix`。完整 configure/build 成功；Combat runner 为 `15 cases, 0 failures`；Core-only CTest 为 `4/4`；`git diff --check` 无空白错误（仅 Git LF/CRLF 提示）。
- 修复提交：`59c71a6` (`fix: keep combat hit stop actor local`)。
