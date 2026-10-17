# Task 5：DungeonSession 显式持久化事务报告

## TDD RED

先新增 `tests/dungeon/dungeon_transaction_tests.cpp` 九个事务用例，并接入
`tests/dungeon/CMakeLists.txt` 与 `dungeon_test_main.cpp`，守卫总数改为 44。
首次运行：

```text
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
```

按预期编译失败，MSVC 报告缺少 `DungeonRunState`、`SaveDisposition`、
`RoomPhase::committing/faulted`、两参数构造、`pending_transition`、
`resolve_pending_transition`、`request_descent` 以及快照新字段等 API。

## GREEN

实现稳定 DTO 别名、事务状态机、提交回执分支、完整事件 subject/destination
字段、故障诊断与 Release 停止传播语义；旧生命周期、导航、压力 helper 改为
显式 `commit_pending`。压力测试仍为 4 例，并对两个相同会话逐字段比较快照、
pending、DungeonEvent 和 CombatEvent。

验证命令及结果：

```text
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
out\\build\\windows-msvc-core-debug\\bin\\arpg_dungeon_tests.exe
44 cases, 0 failures

ctest.exe --test-dir .\\out\\build\\windows-msvc-core-debug -R "^(dungeon\\.units|architecture\\.)$" --output-on-failure
100% tests passed, 0 tests failed out of 1
```

另外运行 `windows-msvc-release`，Dungeon 同样为 44 cases、0 failures，覆盖
Release overflow 不继续传播分支。

## 修改文件

- `src/dungeon/dungeon_types.hpp`
- `src/dungeon/dungeon_session.hpp`
- `src/dungeon/dungeon_session.cpp`
- `tests/dungeon/CMakeLists.txt`
- `tests/dungeon/dungeon_transaction_tests.cpp`
- `tests/dungeon/dungeon_navigation_tests.cpp`
- `tests/dungeon/dungeon_stress_tests.cpp`
- `tests/dungeon/dungeon_test_support.hpp`
- `tests/dungeon/dungeon_test_main.cpp`

## 自审

- committed 仅接受 generation、`next_state.commit_generation` 和逐字段
  `same_run_state` 同时匹配的回执；接受后先销毁旧 CombatWorld，再发提交/销毁事件。
- committing/faulted 不 tick、不接受攻击或移动；not_committed 才回到
  awaiting_exit；indeterminate/mismatch 进入 faulted。
- pending 的 expected_generation 始终等于 next_state.commit_generation。
- 事件 subject 始终为旧房/当前房，提交与销毁事件 destination 为新房。
- `git diff --check` 通过。

## Concerns

- 为保持现有 raylib/旧调用方兼容，保留了 `DungeonSessionConfig` 委托构造器；
  新持久化入口是 `(DungeonRules, DungeonRunState)`。
- 旧的 `RoomDescriptor`（含 CombatLabConfig）仍保留给生成兼容代码，稳定 DTO
  通过 `DungeonRunState`/`DungeonElement`/`TransitionKind` 别名导出。

## 修复追加：descent room-index overflow 诊断

### TDD RED

在既有 `descent requires clear hole range and edge` 用例中追加 max-index + hole
descent 回归断言：房间清理并进入 `awaiting_exit` 后调用
`request_descent(true)`，要求同时进入 `faulted`、报告
`DungeonFault::room_index_overflow`，并保留
`diagnostics.room_index_overflow=true`。修复前 Debug 运行结果：

```text
[FAIL] dungeon_transaction.descent requires clear hole range and edge:
overflow_state.diagnostics.room_index_overflow
44 cases, 1 failures
```

### 最小修复

`DungeonSession::request_descent` 在 `make_descent_transition` 返回
`room_index_overflow` 时，先置 `diagnostics_.room_index_overflow=true`，再调用
`enter_fault`；与 `attempt_exit` 的门请求路径保持一致。未做其它重构。

### 验证

```text
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
out\\build\\windows-msvc-core-debug\\bin\\arpg_dungeon_tests.exe
[stress] exits=1000 index=1000 allocation_before=0 allocation_after=0 delta=0 dungeon_overflow=0 relay_overflow=0 combat_overflow=0 input_overflow=0
44 cases, 0 failures

ctest.exe --test-dir .\\out\\build\\windows-msvc-core-debug -R "^(dungeon\\.units|architecture\\.)$" --output-on-failure
100% tests passed, 0 tests failed out of 1

.\\scripts\\Build.ps1 -Preset windows-msvc-release
out\\build\\windows-msvc-release\\bin\\arpg_dungeon_tests.exe
[stress] exits=1000 index=1000 allocation_before=0 allocation_after=0 delta=0 dungeon_overflow=0 relay_overflow=0 combat_overflow=0 input_overflow=0
44 cases, 0 failures

git diff --check
通过（仅有 Git 的 LF/CRLF 转换提示，无 whitespace 错误）
```

### 修复范围 / concerns

- 修改：`src/dungeon/dungeon_session.cpp`、
  `tests/dungeon/dungeon_transaction_tests.cpp`、本报告追加记录。
- 回归断言复用既有事务用例，不增加 Dungeon case 数，最终仍为 44 cases。
- 无已知 concerns。
