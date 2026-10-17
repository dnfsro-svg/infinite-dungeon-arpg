# Task 3 Report — Deterministic Ground Movement and Jump Arc

## Status

完成。基线 `e901d65250c8325c4d89646388176eb3f74dc7c6`，提交
`7c660d673b265b2df3ce85dcedf3f319bd3c6166`（`feat: add deterministic combat movement and jump`）。

## RED

先只添加四个 `movement_jump` 测试、CMake 接线和总例数 `7 -> 11`，生产世界尚不存在。

首次运行：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
```

managed sandbox 无权写该新工作树的 `out/build`，CMake 以 `System Error: Permission denied`
退出；这是环境失败，不计作行为 RED。按正常 `require_escalated` 流程重跑同一命令后，得到预期 RED：

```text
movement_jump_tests.cpp(3): fatal error C1083:
无法打开包括文件: "combat/combat_world.hpp": No such file or directory
ninja: build stopped: subcommand failed.
```

## GREEN 与最终验证

最小实现后运行同一 Core-only 测试命令：CTest `4/4`。首次 GREEN 暴露测试中三处
忽略 `[[nodiscard]] queue_action` 的 C4834；改为显式成功/溢出断言后复跑，新增警告清零，CTest 仍为 `4/4`。

提交前 Fresh 验证：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

关键输出：

```text
100% tests passed, 0 tests failed out of 4
Total Test time (real) = 2.02 sec
```

Combat 精确计数：

```powershell
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
```

```text
11 cases, 0 failures
```

补充检查：`git diff --cached --check` 通过；针对本任务四个源/测试文件的动态分配与动态容器词法扫描无命中。

## 文件

- `src/combat/combat_types.hpp`
- `src/combat/combat_world.hpp`
- `src/combat/combat_world.cpp`
- `src/combat/CMakeLists.txt`
- `tests/combat/movement_jump_tests.cpp`
- `tests/combat/combat_test_main.cpp`
- `tests/combat/CMakeLists.txt`

## 自审

- `CombatWorld` 固定持有玩家、三个木桩、攻击 runtime 与有界输入缓冲；三个 runtime 均为私有完整嵌套结构，无 pimpl、堆分配或运行期扩容。
- `tick(MovementInput)` 无 delta；60 Hz、5.4 地速、0.70 空控、8.5 跳速、24 重力和精确对角系数均按 brief。
- X/Y 边界、无水平输入保朝向、先积分 Z 再施加重力、落地 Z/速度归零均有测试。
- `PlayerState::landing` 在快照中恰好持续一个 fixed tick；没有提前加入 `CombatEvent`。
- reset 恢复传入 config 的出生点、固定木桩属性、输入内容及诊断；攻击/受击字段保持稳定默认值。
- 攻击、碰撞、受击、目标模拟、事件队列、raylib 与 Stage 2 均未实现。

## 疑虑

无功能阻塞。Fresh 命令重新配置后 Ninja 报 `no work to do`；不过所有变更文件已在紧邻 Fresh
之前的 GREEN 构建中实际编译，修正 C4834 后的测试文件也再次实际编译且无新警告。
