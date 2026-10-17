# Task 9 平台测试失败调查

## 失败证据

在 `windows-msvc-debug` 下直接运行 `arpg_platform_tests.exe`，共 38 例，以下 4 例失败：

1. `dungeon_runtime.committed pending transition maps verified state and saved indicator`
2. `dungeon_runtime.pre publish not committed maps to retryable error`
3. `dungeon_runtime.indeterminate maps to faulted runtime and blocks selection`
4. `dungeon_runtime.single slot corruption recovers and subsequent saves alternate`

四例都在 `tests/platform/dungeon_runtime_tests.cpp` 的
`drive_door_pending(*runtime.session())` 处失败；该函数首先调用
`clear_and_await`，因此未到达 pending transition、保存服务或运行时状态断言。

## phase/wave/remaining/pending 路径

`clear_and_await` 的循环只在 `awaiting_exit` 返回成功。Stage 4 房间在波次清空后可经历：

`combat -> wave_delay -> combat (下一波) -> cleared -> awaiting_exit`。

旧平台辅助函数以旧三假人位置、攻击距离和 `Action::light` 自行清房。这条驱动不保证当前导演生成的多角色波次可在 4096 tick 内按旧路径被清空；失败时 `pending_transition` 尚未创建，因此四例共享同一个前置失败点。

## 对照 Task 8

`tests/dungeon/dungeon_wave_tests.cpp` 的 `clear_current_wave` 在 `RoomPhase::combat` 调用：

`arpg::test::force_defeat_current_wave(session)`。

该函数定义在 `tests/dungeon/dungeon_test_support.hpp` 中，通过既有
`DungeonSessionTestAccess` friend 访问固定池并调用真实 `apply_dummy_impact`，随后继续调用真实 `session.tick({})` 推进 `wave_delay`、下一波、清房和出口状态。它没有新增 public 生产 API，也没有 mock `DungeonRuntime`、保存或转场。

## 根因与最小修复假设

根因是平台运行时保存测试仍携带 Stage 3 的玩家操作清房辅助逻辑；该逻辑不再是 Stage 4 波次测试的可靠前置条件。最小修复是在同一个平台测试文件内复用现有测试 namespace friend helper，使 `clear_and_await` 以固定池真实清房，再让原有 runtime/保存流程继续执行。生产代码保持不变。
