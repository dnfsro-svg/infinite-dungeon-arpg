# Task 2 报告：稳定检查点与规则配置

## Status

GREEN，已提交：`511e390 feat: define stage 3 dungeon rules`。

## TDD 证据

### RED

先注册 `dungeon_rules_suite`、增加五个规则用例并把阶段守卫从 20 调整为 25，未添加生产头或实现。

命令：

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
```

结果：按预期失败。MSVC 在 `tests/dungeon/dungeon_rules_tests.cpp(3)` 报 `C1083`，无法打开 `dungeon/dungeon_checkpoint.hpp`。

### GREEN

加入 checkpoint DTO、规则接口/实现、dungeon CMake 源文件，并将运行时方向/入口别名到 checkpoint 后：

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
```

构建退出码为 0；测试输出包含：

```text
[stress] exits=1000 index=1000 allocation_before=0 allocation_after=0 delta=0 dungeon_overflow=0 relay_overflow=0 combat_overflow=0 input_overflow=0
25 cases, 0 failures
```

完整套件：

```powershell
ctest.exe --test-dir .\out\build\windows-msvc-core-debug --output-on-failure
```

结果：`100% tests passed, 0 tests failed out of 7`，通过 `core.units`、`combat.units`、`dungeon.units` 以及 4 个 `architecture.*_no_raylib`/`combat_no_dungeon` 守卫。

## 变更文件

- `src/dungeon/dungeon_checkpoint.hpp`：新增仅依赖 `<array>` 与 `<cstdint>` 的 `arpg::dungeon::checkpoint` 稳定 DTO。
- `src/dungeon/dungeon_rules.hpp`：新增规则常量、故障枚举、规则配置及 checked 权重接口。
- `src/dungeon/dungeon_rules.cpp`：实现方向到生态映射、规则校验和事务式权重计算。
- `src/dungeon/dungeon_types.hpp`：包含 checkpoint，并为 Stage 2 运行时保留 `RoomDescriptor::combat`，仅将 `ExitDirection`/`EntrySide` 改为 checkpoint 别名。
- `src/dungeon/CMakeLists.txt`：加入 `dungeon_rules.cpp`。
- `tests/dungeon/dungeon_rules_tests.cpp`：新增 5 个规则 RED/GREEN 用例。
- `tests/dungeon/dungeon_test_main.cpp`：注册规则 suite，阶段守卫改为 25。
- `tests/dungeon/CMakeLists.txt`：加入规则测试源文件。

## 自审与 concerns

- `checkpoint::RoomDescriptor` 不含 Combat、`optional`、`string`、`filesystem` 或 raylib 类型；checkpoint 头只包含 array/cstdint。
- `validate_rules` 拒绝非 1 版本、零基础权重和超过 10000 的阈值；0 与 10000 边界均覆盖。
- 权重乘法、逐项加法和累计总和均先 checked；失败时局部结果不会复制到调用者输出。
- 未使用 `std::uniform_int_distribution`，规则模块不链接 raylib。
- Concern（按已确认架构决策）：旧运行时 `RoomDescriptor::combat` 保留至 Task 5，Task 2 未修改 room generation/session；稳定 DTO 位于 `arpg::dungeon::checkpoint`。
