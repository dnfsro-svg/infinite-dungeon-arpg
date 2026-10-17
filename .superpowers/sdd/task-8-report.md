# Task 8：事务地牢存档端到端测试报告

## 范围

提交改动严格限于简报指定的三个测试文件：

- `tests/persistence/dungeon_save_integration_tests.cpp`（新增）
- `tests/persistence/persistence_test_main.cpp`
- `tests/persistence/CMakeLists.txt`

未修改生产代码、raylib、`SaveStore` 或 `DungeonSession`。

## TDD 记录

1. RED：先增加 6 个端到端用例并接入套件，但暂保留持久化守卫为 `21`。
2. 初次构建发现测试宏不能直接接收含逗号的内联 `std::array` 初始化；将其改为命名的 `zero_biases` 局部变量。这是测试语法修正，不涉及生产行为。
3. RED：`scripts/Test.ps1 -Preset windows-msvc-core-debug -Fresh` 已将全部用例编译并执行；唯一行为结果为守卫失败：`persistence case count: actual 27 != expected 21`。
4. GREEN：仅将守卫从 `21` 更新为 `27`，不改变任何生产逻辑。

## 覆盖的六个端到端场景

1. generation=1 的初始状态实际写入 `SaveStore` 后加载，并用加载状态构造 `DungeonSession`；完整 descriptor 字段一致。
2. 真实门移动生成 pending transition，真实 `SaveStore::commit` 结果经 `to_session_result` 回传；旧 combat 被销毁，重启后直接加载下一房。
3. `before_publish` 注入故障返回 `not_committed`；Session 回到 `awaiting_exit`，内存房间字段和磁盘 checkpoint 都仍为旧状态。
4. `after_publish` 注入回执丢失返回 `indeterminate`；当前 Session 进入 `faulted/save_commit_indeterminate`，新 Store 实例扫描到新房 checkpoint。
5. 强制可下洞的旧房完成真实 descent 并保存；重启后验证 depth+1、floor=1、biases 全 0，且 checkpoint 与 pending 目标状态完全一致（包含 hole/abyss）。
6. 对已落盘状态调用 `reset_current_room`，再关闭重开并重复 `load`；room seed、ecology、hole、abyss 与 generation 均保持稳定。

测试中的 `to_session_result` 逐字遵循简报给定的 `SaveCommitState -> SaveDisposition` 映射，默认 `indeterminate`，并回传 `verified_state.commit_generation` 与 `verified_state`。

## 验证命令与结果

RED：

```powershell
try { .\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh } catch { ... }
.\out\build\windows-msvc-core-debug\bin\arpg_persistence_tests.exe
```

结果：Core/Combat/Dungeon 测试通过；Persistence 执行 27 cases，唯一失败为 `actual 27 != expected 21`。

实际关键输出：

```text
[FAIL] persistence case count: actual 27 != expected 21
27 cases, 1 failures
```

GREEN：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\out\build\windows-msvc-core-debug\bin\arpg_persistence_tests.exe
```

结果：CTest 12/12 通过（包含 Core-only 与架构守卫）；持久化测试 `27 cases, 0 failures`。

实际关键输出：

```text
100% tests passed, 0 tests failed out of 12
Total Test time (real) = 15.49 sec
27 cases, 0 failures
```

差异检查：

```powershell
git diff --check
```

结果：退出码 0，无空白错误。

实际输出：无输出（退出码 0）。

## 自审

- 每例都使用实际临时目录、`SaveStore` 和至少一个 `DungeonSession`，没有模拟 SaveStore/Session。
- 门和 descent 均从 Session 的真实 pending transition 流程进入，再用真实 Store 的 commit receipt 桥接。
- committed、not_committed、indeterminate 三种结果均有端到端覆盖。
- 重启场景使用新建的 `SaveStore` 实例，避免复用内存状态。
- 将 `arpg_dungeon` 仅链接到测试可执行文件；生产 `arpg_persistence` 的架构守卫仍在 Fresh CTest 中通过。

## Concerns

无已知 Critical 或 Important concern。测试临时目录析构使用 `std::error_code` 的尽力清理，沿用现有持久化测试模式；异常残留仅会留在系统临时目录，不影响测试状态或项目文件。

## 评审修复（后续）

本次修复仅更新 `tests/persistence/dungeon_save_integration_tests.cpp` 及其已有的
持久化用例守卫；未改动生产代码、CMake 或 raylib Host。

1. 第 5 例现在用重新 `load` 的 checkpoint 创建全新的 `DungeonSession`，并从该
   Session snapshot 显式验证 generation、room seed、depth、floor、biases、ecology、
   hole 与 abyss，确保重启不会重新投掷房间描述。
2. 第 6 例先完成一次真实且 committed 的门过渡，明确确认 generation 不再是 1；
   之后对 R/reset、两个独立的 `load`，以及每个 load 重建的 Session，均按完整
   descriptor 比较 generation 和所有房间稳定字段。
3. 集成测试文件显式包含 `<array>` 与 `<functional>`，不再依赖传递包含。

### 本次 TDD 与验证

- RED：新增覆盖后临时将持久化 case 守卫设为 `26`，Fresh CTest 的唯一失败为
  `persistence case count: actual 27 != expected 26`，证明新编译单元已纳入 27-case
  套件并执行。
- GREEN：恢复守卫为 `27` 后，执行
  `./scripts/Test.ps1 -Preset windows-msvc-core-debug -Fresh`：CTest `12/12` 通过；再执行
  `./out/build/windows-msvc-core-debug/bin/arpg_persistence_tests.exe`：`27 cases, 0 failures`。
- `git diff --check`：退出码 0；工作树的 LF/CRLF 提示来自 Git autocrlf，不是空白错误。
