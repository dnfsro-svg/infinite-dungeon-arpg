# Task 7：双槽发布、恢复和故障注入报告

## 范围与隔离

- 工作树：`E:\game\.worktrees\m03-persistence`
- 分支：`task/m03-persistence`
- 仅实现 persistence；保存载荷统一使用 `arpg::dungeon::checkpoint::DungeonRunState`。
- `arpg_persistence` 未链接 `arpg_dungeon`、`arpg_combat` 或 raylib。

## TDD 证据

### RED

先新增 7 个正常双槽用例、6 个故障用例并注册为 persistence 测试；未创建生产 SaveStore 文件时运行：

```text
PS> .\scripts\Build.ps1 -Preset windows-msvc-core-debug
...
save_store_tests.cpp(4): fatal error C1083: 无法打开包括文件: “persistence/save_store.hpp”: No such file or directory
save_store_fault_tests.cpp(4): fatal error C1083: 无法打开包括文件: “persistence/save_store.hpp”: No such file or directory
```

失败原因是目标接口尚不存在，符合预期 RED。

### GREEN

实现路径、双槽扫描、损坏归档、atomic publish 三态和八个 fault point 后：

```text
PS> .\scripts\Build.ps1 -Preset windows-msvc-core-debug
21 cases, 0 failures
```

完整 Core-only CTest：

```text
PS> .\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
100% tests passed, 0 tests failed out of 12
```

Release 构建及 CTest：

```text
PS> .\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
100% tests passed, 0 tests failed out of 13
```

## 变更文件

- `src/persistence/save_paths.hpp`
- `src/persistence/save_paths.cpp`
- `src/persistence/save_store.hpp`
- `src/persistence/save_store.cpp`
- `src/persistence/CMakeLists.txt`
- `tests/persistence/save_store_tests.cpp`
- `tests/persistence/save_store_fault_tests.cpp`
- `tests/persistence/persistence_test_main.cpp`
- `tests/persistence/CMakeLists.txt`

## 行为自审

- 空目录首次提交写入 A；随后按非当前槽交替发布。
- 两槽不同代数选择最高代；同代同载荷固定选择 A；同代不同载荷返回 `recovery_required/conflicting_slots`。
- 单有效槽遇到损坏槽先归档；归档失败返回 `blocked/archive_failed`，不删除原损坏字节。
- 临时文件不参与最高代数选择；有效槽存在时会清理残留 temp。
- 发布前任一故障返回 `not_committed`；publish 后验证无法证明时只返回 `indeterminate`。
- 两槽损坏且 temp 残留时保持 `recovery_required`；`archive_invalid_and_create` 会先归档四个文件，再创建初始代。
- fault hook 只在 brief 列出的八个边界调用；所有公开 `noexcept` 边界将异常转换为错误结果。

## 检查与 concerns

- `git diff --check`：通过（仅显示 Windows 行尾转换提示）。
- Concern：归档文件名采用 `corrupt_<UTC数字>_<原文件名>`，同一时间戳通过数字后缀避免冲突；调用方不应依赖具体后缀布局，只应按 `corrupt_` 前缀识别。
- Concern：SaveStore 未包含 Session/Host 集成，按任务边界留待后续任务。

## Important 修复追加（本轮）

### RED 回归证据

- 在旧 `SaveLoadResult` 字段顺序（`checkpoint` 位于 `recovered` 前）上加入五项聚合初始化探针后运行 Debug 构建，编译失败：

  ```text
  save_store_tests.cpp(26): error C2607: 静态断言失败
  ```

  该断言要求公共聚合顺序为 `state, error, active_slot, bool recovered, checkpoint`。
- 在旧 copy/delete 归档实现上运行新增的中途故障场景，回归失败：

  ```text
  [FAIL] save_store_faults.archive failure blocks and preserves corrupt files: result.state == persistence::SaveLoadState::blocked (...save_store_fault_tests.cpp:248)
  21 cases, 1 failures
  ```

  该失败证明旧实现无法在第二个归档边界失败后保持原损坏文件。

### 修复内容

- 调整 `SaveLoadResult` 声明及所有聚合返回为 `state, error, active_slot, recovered, checkpoint`，保留公共接口顺序。
- 将损坏文件归档改为逐文件 `rename` 事务；中途失败或异常时按逆序 rename 回原路径，避免删除原文件造成不可恢复状态。`before_archive` 故障点在每个归档文件边界触发，覆盖中途失败回滚。
- `commit` 增加 publish 边界状态跟踪。rename 成功后才进入 `indeterminate`；publish 前异常统一返回 `not_committed`。
- 去除 `slot_name`、`temp_name`、`read_slot`、`remove_temps` 中不安全的 `noexcept`，使路径/分配异常进入外层 `load`/`commit` 错误转换；内部已具备捕获的扫描/归档边界继续保持防泄漏。
- 将中途归档断言合并到既有故障用例，Persistence case 数保持简报要求的 21。

### 最终验证

```text
PS> .\scripts\Build.ps1 -Preset windows-msvc-core-debug
...
21 cases, 0 failures

PS> .\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
100% tests passed, 0 tests failed out of 12

PS> .\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
100% tests passed, 0 tests failed out of 13

PS> git diff --check
通过（仅显示 Windows 行尾转换提示）
```

## Important 复审修复：final scan 错误三态判定

### RED 回归证据

- 将既有 `final_scan_a`（另一槽）故障场景改为要求发布后返回 `indeterminate`，旧实现仍返回 `committed`：

  ```text
  [FAIL] save_store_faults.publish final scan fault is indeterminate: result.state == persistence::SaveCommitState::indeterminate (...save_store_fault_tests.cpp:213)
  21 cases, 1 failures
  ```

- 同一故障用例增加 after-publish 篡改 target 为截断载荷的场景，要求“target 明确无效且旧 active 唯一有效”返回 `not_committed`；旧实现返回 `indeterminate`。

### 最小实现

- `SaveStore::commit` final scan 先检查 `directory_error`、`faulted` 及任一槽 `unavailable`，发布后任何目录/读取不确定性统一返回 `indeterminate`。
- 仅在 target 已明确解码为 `invalid`、另一槽为 valid 且该槽就是发布前 active 时返回 `not_committed`；target valid 且完整匹配 expected 的既有 committed 逻辑保持不变。
- 保留 `final_scan_a/b` fault hook；target 槽 final-scan 故障仍不会误报为 `not_committed`。
- 删除未使用的 `final_scan_point` helper；`ScanResult::faulted` 现由 final-scan 判定实际消费。

### 本轮验证

```text
PS> .\out\build\windows-msvc-debug\bin\arpg_persistence_tests.exe
21 cases, 0 failures

PS> .\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
100% tests passed, 0 tests failed out of 13

PS> .\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
100% tests passed, 0 tests failed out of 12

PS> .\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
100% tests passed, 0 tests failed out of 13

PS> git diff --check
通过（仅显示 Windows 行尾转换提示）
```

## Important 复审修复：扫描 exists 错误不得误报 empty

### RED 回归证据

- 在 `truncated_temp_without_valid_slot_requires_recovery` 中加入可控的
  Win32 重解析路径：`run_a.sav` 指向 `\\.\NUL\child`，其
  `std::filesystem::exists` 返回错误，而其余目标路径探测成功返回 false。
- 旧实现复用同一 `error_code` 的短路 OR，后续成功探测清掉错误，导致
  `load()` 错误返回 `empty`；运行结果：

  ```text
  [FAIL] save_store_faults.truncated temp without valid slot requires recovery:
  21 cases, 1 failures
  ```

### 最小实现

- `scan_directory` 为目录、槽位和临时文件分别使用独立 `error_code`；四个目标路径均完成探测，任何路径错误都累计 `directory_error`，不再由短路 OR 覆盖。
- `load()` 在扫描目录错误时保持 `blocked/directory_unavailable`，不可读且无有效槽不再进入 `empty` 分支。
- `remove_temps` 将删除错误或异常标记为 `directory_error`，调用方转为
  `blocked/directory_unavailable`。

### 本轮验证

```text
PS> .\out\build\windows-msvc-debug\bin\arpg_persistence_tests.exe
21 cases, 0 failures

PS> .\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
100% tests passed, 0 tests failed out of 12

PS> .\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
100% tests passed, 0 tests failed out of 13

PS> git diff --check
通过（仅显示 Windows 行尾转换提示）
```
