# Stage 17 Task 3 实现报告

## 结论

已把五个主动技能槽接入 `DungeonSession` 的候选状态 → 保存 → 发布事务链：

- 新增固定的 0-based remove/equip/swap 请求 API。
- 所有请求先在 `ReusablePendingSave::next_state` 中用已有容量复制 `stable_state_`，只在候选上调用 `skills` 纯函数并整体校验。
- 候选通过 `PendingSaveKind::skill_loadout` 进入现有保存事务；保存成功才由 `commit_pending_save` 一次发布，失败只清 pending 并恢复原 phase。
- loadout 提交不创建、不应用 `PlayerCombatBuild`，没有接数字键、Raylib UI，也没有改变战斗技能行为。
- `DungeonSnapshot` 按值携带 stable loadout；pending 候选不会提前泄露到快照。
- 占用槽不能 overwrite；必须先 remove 后 equip，或使用 swap。

## TDD：RED

先创建 `dungeon_skill_loadout_transaction_tests.cpp`，覆盖：

1. 默认值快照且快照按值隔离。
2. 使用真实 `SaveStore` 依次执行 remove 槽 0、equip 到槽 4、swap 槽 1/4；每次先验证仅产生 pending 且 stable 快照未变，再提交并验证一次发布。
3. 使用真实 `SaveStore` 在 `before_publish` 注入失败，验证 stable 全状态、generation、owned bits 和五槽字节不变，并重试确认 reusable candidate 会从 stable 重建。
4. 越界、空槽、非法技能、未拥有技能、重复技能、占用槽、无变化 swap 拒绝。
5. 已有 pending save、committing、death_pending、transitioning 状态拒绝。
6. committed 回执中的 loadout 与候选不一致时进入 `save_receipt_mismatch`，stable loadout 不发布。

第一次直接从普通 PowerShell 构建时，CMake 因当前 shell 未加载 Windows SDK 而在项目门禁停止；该次不计作功能 RED。加载 VS 2022 x64 / Windows SDK 10.0.26100.0 后执行：

```powershell
$env:WindowsSDKVersion='10.0.26100.0\'
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests'
```

真实 RED 在编译期出现，原因与预期一致：

- `DungeonSnapshot` 缺少 `skill_loadout`。
- `DungeonSession` 缺少 `request_remove_active_skill`、`request_equip_active_skill`、`request_swap_active_skill_slots`。
- `PendingSaveKind` 缺少 `skill_loadout`。

没有生产实现时测试无法编译，证明测试确实约束了新增能力。

## GREEN 实现

### 请求与候选事务

三个请求先检查：

- 当前没有 pending save。
- phase 仅限既有稳定请求阶段，拒绝 committing、death_pending、transitioning、faulted。
- 槽索引为 0-based 且在 `[0, 5)`。
- remove 不接受空槽；swap 不接受同槽或双空槽。

通过前置检查后：

1. `pending_save_.prepare()` 取得可复用候选。
2. `copy_run_state_reusing_items(candidate, stable_state_)` 重建完整候选。
3. 仅在 `candidate.skill_loadout` 上调用领域纯函数。
4. 调用 `validate_skill_loadout` 整体校验。
5. 将候选 move 进现有 `prepare_item_save`，不复制/修改 stable。

### 保存与发布

`prepare_item_save` 现在会对所有候选同时校验 ownership、passive/progression 和 skill loadout。只有 equipment/craft/recipe/reinforcement 才构建 `PlayerCombatBuild`；`skill_loadout` 不创建 build cache。

`commit_pending_save` 对 `skill_loadout` 成功回执发布完整候选并恢复请求前 phase；失败分支沿用原子事务路径，只恢复 phase、递增失败诊断并清 pending。

### 完整状态覆盖

为防止回执或复用路径遗漏 loadout，补齐：

- `copy_run_state_reusing_items` 复制 loadout。
- `publish_run_state_reusing_items` 发布 loadout。
- `same_run_state` 比较 owned bits、五个 active 槽及全部 support 槽。
- `DungeonSnapshot` 从 stable state 按值复制 loadout。

## GREEN 与回归命令

聚焦构建与测试：

```powershell
$env:WindowsSDKVersion='10.0.26100.0\'
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests && set ARPG_STAGE17_SKILL_LOADOUT_ONLY=1 && out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe'
```

结果：`6 cases, 0 failures`。

指定回归：

```powershell
ctest --test-dir out/build/windows-msvc-core-debug -R '^(dungeon\.units|stage16\.crafting_transaction\.units)$' --output-on-failure
```

结果：

- `dungeon.units`：Passed，168.45 秒。
- `stage16.crafting_transaction.units`：Passed，0.01 秒。
- 总计 2/2，通过率 100%，0 failures。

## 修改文件

生产代码：

- `src/dungeon/dungeon_types.hpp`
- `src/dungeon/dungeon_session.hpp`
- `src/dungeon/dungeon_session.cpp`
- `src/dungeon/dungeon_transition.cpp`
- `src/dungeon/dungeon_snapshot.cpp`
- `src/dungeon/dungeon_progression.cpp`

测试：

- `tests/dungeon/dungeon_skill_loadout_transaction_tests.cpp`
- `tests/dungeon/CMakeLists.txt`
- `tests/dungeon/dungeon_test_main.cpp`

报告：

- `.superpowers/sdd/task-3-report.md`

`src/dungeon/CMakeLists.txt` 无需修改：`arpg_dungeon` 已公开链接 `arpg_skills`，新增会话代码不需要新的源文件或依赖边。

## 自审

- stable state 从未作为 mutation 目标；不存在“先改 stable 再回滚”。
- 保存失败路径不调用 publish；测试对完整 `same_run_state`、generation、owned bits 和五槽字节做了断言。
- loadout pending 不占用 `pending_item_build_`，成功提交也不调用 `CombatWorld::apply_player_build`。
- 快照只读取 stable loadout，pending candidate 不可见。
- occupied equip 由领域函数返回 `destination_occupied`，不会覆盖已有技能。
- duplicate、invalid id、not owned、invalid/empty slot 都会 reset candidate engagement 并返回 rejected。
- 提交回执通过扩展后的 `same_run_state` 比较完整 loadout，不能用相同 generation 的错误槽位状态发布。
- 保留了工作树中既有的 `.superpowers/sdd/task-5-report.md` 改动，未编辑、未暂存。
