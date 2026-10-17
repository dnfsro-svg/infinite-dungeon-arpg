# Task 3 报告：房间三流随机、战斗模板派生和纯状态推进

## Status

GREEN，已提交：`69a8ef8 feat: add deterministic dungeon progression`。

## TDD 证据

### RED

先将房间生成固定八个用例、纯推进固定七个用例加入测试，注册
`dungeon_progression_suite`，并将 Dungeon 阶段守卫设为 35；当时未添加
任何生产接口。

命令：

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
```

结果：按预期失败。MSVC 在
`tests/dungeon/room_generation_tests.cpp(3)` 报 `C1083`，无法打开
`dungeon/room_combat_template.hpp`。

### GREEN

实现六个固定 RNG 域、生态/洞口/深渊独立流、checked 推进、战斗模板迁移，
并加入新的 Dungeon CMake 源文件后：

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_core_tests.exe
.\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
```

结果：构建退出码为 0；Core 输出 `24 cases, 0 failures`；Dungeon 输出：

```text
[stress] exits=1000 index=1000 allocation_before=0 allocation_after=0 delta=0 dungeon_overflow=0 relay_overflow=0 combat_overflow=0 input_overflow=0
35 cases, 0 failures
```

架构/完整规则门：

```powershell
ctest.exe --test-dir .\out\build\windows-msvc-core-debug -R "^(core\.units|dungeon\.units|architecture\.)$" --output-on-failure
ctest.exe --test-dir .\out\build\windows-msvc-core-debug --output-on-failure
git diff --check
```

首条命令通过 `core.units` 与 `dungeon.units`；补跑明确的
`^architecture\.` 过滤后 4 个架构守卫均通过；完整 CTest 为
`100% tests passed, 0 tests failed out of 7`。`git diff --check` 无空白错误，
仅报告工作树 LF/CRLF 转换提示。

## 变更文件

- `src/dungeon/room_combat_template.hpp/.cpp`：迁移五种 Stage 2 CombatLabConfig 模板，并校验 rules version。
- `src/dungeon/room_generation.hpp/.cpp`：新增 checkpoint 稳定 DTO 生成结果、固定域种子派生和三流随机判定；保留旧运行时 `make_initial_room`/`make_next_room` 包装。
- `src/dungeon/dungeon_progression.hpp/.cpp`：新增初始、门、下坠纯状态推进与逐字段比较；溢出及无效方向失败时保持输入状态。
- `src/dungeon/CMakeLists.txt`：加入生成、模板和推进实现。
- `tests/dungeon/room_generation_tests.cpp`：固定八个生成/模板用例。
- `tests/dungeon/dungeon_progression_tests.cpp`：固定七个纯推进用例。
- `tests/dungeon/dungeon_test_main.cpp`：注册推进 suite，阶段守卫改为 35。
- `tests/dungeon/CMakeLists.txt`：加入推进测试源文件。

## 自审与 concerns

- 新接口中的 `RoomDescriptor`、`RoomGenerationResult` 和 `DungeonRunState` 均显式使用 `arpg::dungeon::checkpoint` DTO；旧运行时 `RoomDescriptor::combat` 仍由两个 Stage 2 包装保留，未提前改动 `dungeon_session.*`。
- 生态样本为按当前总权重 `next_bounded(total)` 的值；洞口和深渊分别从各自域取一次 `next_bounded(10000)`，阈值采用左闭右开比较。
- 生成过程在规则/权重/随机边界失败时返回零初始化 DTO，未复制半有效房间；推进先在局部 state 上 checked，再成功后提交。
- `same_run_state` 为逐字段比较，没有使用 `memcmp`。
- Concern：计划命令中的架构过滤表达式只实际匹配 `core.units`/`dungeon.units`；已额外执行 `-R "^architecture\\."` 验证全部 4 个架构守卫。
- 未实现 Task 4/5 的事务会话、持久化或 raylib。
