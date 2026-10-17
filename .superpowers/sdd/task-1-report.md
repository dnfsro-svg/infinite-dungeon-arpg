# Stage 1 Task 1 实施报告

## 状态

DONE

Task 1 已在 `task/m01-combat-core` 完成并提交。没有修改 `main`、集成工作树或任何 Stage 2 内容。

## 实现内容

- 新增稳定的 `Vec3`、`Aabb`、`AttackId`、`AttackPhase`、`FeedbackLevel`、`ImpactKind` 和 `AttackDefinition` 公共类型；字段、底层类型与默认值按 brief 实现。
- 新增私有 `constexpr std::array` 六行攻击目录；六行 ID、帧段、伤害、破防、反馈、AABB、位移、击退和浮空数值逐项核对 brief。
- 新增 `kAttackCount`、`find_attack_definition`、`attack_phase_at` 和 `validate_attack_catalog`；查询返回目录内稳定指针，阶段使用半开区间。
- 目录校验覆盖重复 ID、零长度阶段、非正伤害、负破防、反向 AABB 和非法反馈枚举。
- 新增 `arpg_combat` STATIC 目标，PUBLIC 暴露 `src` include 和 C++17，PRIVATE 链接 `arpg_core`，启用项目警告。
- 新增独立 `arpg_combat_tests`，只注册攻击目录 suite，精确锁定 3 cases；Core 的 22 cases 未改动。
- 将依赖闭包门禁同时应用于 `arpg_core` 和 `arpg_combat`。
- 将源码 scanner 的输入从 `CORE_DIR` 泛化为 `SOURCE_DIR`/`SOURCE_LABEL`；词法器和自测主体未改动，保留全部 16 种源文件扩展。
- 注册 `architecture.core_no_raylib` 与 `architecture.combat_no_raylib`，标签均为 `headless;architecture`。

## TDD：RED 证据

生产文件创建前先新增 Combat 的 3 个测试、独立入口和测试 CMake 接线，然后执行：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

第一次普通沙箱执行被既有构建缓存的文件所有权拦截，CMake 无法写 `DartConfiguration.tcl.tmp`/`CMakeCache.txt.tmp`；这不是有效 RED。使用获批的工作区权限原样重跑后，配置成功并到达 Combat 测试编译，关键失败为：

```text
attack_catalog_tests.cpp(3): fatal error C1083:
无法打开包括文件: “combat/attack_catalog.hpp”: No such file or directory
ninja: build stopped: subcommand failed.
```

这是预期 RED：测试已经进入编译，失败原因正是尚未创建的 Combat 公共头，而不是测试拼写、CMake 配置或 Core 回归。

## GREEN 与最终验证

最小生产实现和架构接线完成后，首轮 Fresh 已通过 4/4 CTests。审查阶段按主控指令将纯编译期的 `kAttackCount == 6` 检查改为文件作用域 `static_assert`，运行时 case 加强为逐项查询六个 AttackId，并继续验证 `none` 返回空；case 总数仍为 3。随后执行 focused 构建：

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
```

结果：构建退出 0、无编译警告，Combat 输出 `3 cases, 0 failures`。

提交前重新执行完整 Fresh：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

结果：退出 0，4/4 通过：

- `core.units`
- `combat.units`
- `architecture.core_no_raylib`
- `architecture.combat_no_raylib`

附加验证：

```powershell
ctest --test-dir .\out\build\windows-msvc-core-debug -N
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
git diff --check
```

结果分别为 `Total Tests: 4`、`3 cases, 0 failures`、退出 0。最终 Core 可执行测试也确认 `22 cases, 0 failures`。

## 文件

- `CMakeLists.txt`
- `src/combat/CMakeLists.txt`
- `src/combat/combat_types.hpp`
- `src/combat/attack_catalog.hpp`
- `src/combat/attack_catalog.cpp`
- `tests/combat/CMakeLists.txt`
- `tests/combat/combat_test_main.cpp`
- `tests/combat/attack_catalog_tests.cpp`
- `tests/platform/CMakeLists.txt`
- `tests/platform/core_boundary_test.cmake`

## 提交

```text
6cd16bd2b60b7b9dc979b9336c65e801b5459bfb feat: establish stage 1 combat module
```

提交使用基线已有的 `Codex <codex@local>` 身份，并通过命令级 `-c user.name`/`-c user.email` 提供；未修改全局或仓库 Git 配置。

## 自审

- 六行目录与 brief 逐字段核对，无数值偏差。
- J1 边界为 `[0,5)` startup、`[5,8)` active、`[8,17)` recovery、`[17,...)` finished。
- `arpg_combat` 位于 graphics 条件外并紧随 Core；`arpg_core` 仅 PRIVATE 链接，未进入 Combat 公共 API。
- 两个 configure-time 依赖闭包检查均存在；两个源码扫描 CTest 均扫描正确目录。
- scanner diff 只涉及参数 guard、底部扫描变量和通用错误标签，词法器未改。
- 提交只含 brief 列出的 10 个功能/测试文件；提交后工作树干净（报告目录由任务编排使用且不纳入功能提交）。
- 独立只读审查未发现 Critical、Important 或 Minor 问题；主线程仍会执行正式 task review。

## 疑虑与环境说明

- 功能疑虑：无。
- 普通沙箱不能覆盖由交互用户创建的 CMake/CTest 日志与 Git worktree 索引；所有计入结论的构建、CTest 列表、暂存和提交都使用获批的工作区权限重跑并成功。
- 一次直接 `cmake --build` 未加载项目的 VS 开发者环境，因找不到标准库 `<cmath>` 而失败；它不计入代码验证。随后使用项目自带 `Build.ps1` 在固定 MSVC/SDK 环境下通过。
- `git diff --check` 仅输出 Windows `LF will be replaced by CRLF` 提示，退出码为 0，没有空白错误。
