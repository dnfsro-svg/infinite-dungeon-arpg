# Stage 19 Task 3 实现报告：精确单批次遭遇

## 结论

已完成预算驱动到精确数量驱动的导演迁移：

- `EncounterBuildRequest` 显式携带房间 seed、深度、生态、入口、洞口状态和目标怪物数。
- 普通与深渊构建均一次生成完整批次；合法数量为 12～45，`wave_count` 固定为 1，`initial_monster_count` 与首波出生数严格相等。
- 保留生态权重、首个直接目标、怪物词缀、深渊词缀补充及各类安全标签上限。
- 出生点覆盖完整四倍房间，并排除玩家出生点半径 4、四门中心半径 3、激活洞口中心半径 3；随机拒绝 32 次后使用确定性 1 单位网格回退。
- `total_budget` / `spent_budget` 仅作为威胁值诊断，使用有界累加；旧深度预算、双波阈值和深渊预算适配 API 已移除。

## TDD 过程

恢复任务时保留了前一实现代理的全部未提交修改。其连续完整回归记录依次收敛为 17、4、1 个旧测试失败；最后一个真实输入机器人用例保留 512 tick 真实输入后仅做一次测试辅助清场，没有把自动清场改入生产代码。

恢复后的第四轮先确认目标构建成功，再完整执行：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^dungeon\.units$' --output-on-failure
```

结果：`dungeon.units` 通过，注册用例 294/294，0 failures，259.81 秒。

随后执行全量 Debug 构建，第一次准确暴露 3 处平台验收程序仍调用旧四参数签名。只把这 3 处迁移为 `EncounterBuildRequest`，普通房临时数量保持 12，再次全量构建成功（91 个目标已完成，最终增量 7/7）。

## 生产实现

### 精确数量与合法性

- 请求先校验深度、生态、入口和 `[12,45]` 数量范围，非法请求返回 `invalid_rules` 且不截断。
- 第一个出生固定选择不受限制的直接目标 `chaos_chaser`，后续沿用生态加权候选选择直到精确数量。
- 合法性检查覆盖单波、精确数量、第二波为空、威胁和、连续 ordinal、怪物词缀、至少一个直接目标、标签上限及全部安全出生区。
- 普通构建先生成既有词缀；深渊构建在相同编队上继续补充深渊词缀。

### 临时 Session 迁移边界

为在 Task 4 接入 `room_affix` 前保持会话与快照可构建，`DungeonSession` 只做了明确标注的临时适配：

- 普通房 `target_monster_count = 12`。
- 深渊房 `target_monster_count = 18`。
- 快照合法性使用同一临时上下文。

这些值未写入存档、HUD、相机或房间词条；Task 4 必须用确定性的密度词条结果替换。

## 验证

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --test-dir out/build/windows-msvc-debug -R '^dungeon\.units$' --output-on-failure
cmake --build --preset windows-msvc-debug
git diff --check
```

结果：

- 聚焦目标构建成功。
- `dungeon.units`：Passed，294/294，259.81 秒。
- 全量 Debug 构建成功。
- `git diff --check` 通过。

## 范围自检

- 未接入 `room_affix` / density 到 Session。
- 未修改 HUD、相机或存档格式。
- 未删除文件，未重置工作树，保留并完成了中断前的全部修改。
- 平台文件仅迁移导演 API 的直接调用点。

## 审查 P1 修复：恢复纯输入自然清场门禁

审查确认原 `drive_real_input_clear` 在 512 tick 后调用
`force_defeat_current_wave`，并在 4096 tick 后回退到 `drive_clear`；同时
名为“launcher input robot clears thousand rooms”的压力用例实际从第一房起
就用辅助清场。这会让 12 怪、大房间下的真实输入行为失去门禁作用。

### RED 与根因

先删除两处清场回退，保留 10 房真实 `queue_action(Action::launcher)` 路径。
未修复机器人时完整程序得到 `294 cases, 2 failures`：

- 第 0 房在 tick 1388 自然死亡，剩余 7 怪；30 次上挑、48 次真实命中、
  5 次自然击杀、25 次怪物命中，无队列溢出。
- room 38 在 4096 tick 内 21 次上挑、46 次命中但 0 击杀。

最低生命目标锁定的第一版最小修复仍保持 RED：room 38 提升到 3 次击杀，
但第 0 房仍在 tick 1504 死亡、剩余 8 怪。这证明击退后最近目标切换造成的
伤害分散是原因之一，但旧机器人原本针对单怪/低密度设计，默认裸装仍无法
在 12 怪围攻下完成输入链门禁。

### 最小 GREEN

- 机器人固定优先最低剩余生命、同生命时取最近目标，避免上挑击退后立即
  换靶。
- 10 房门禁明确命名为 `combat ready launcher input robot naturally clears
  ten rooms`，使用只影响测试角色的确定性构筑：50 倍近战倍率、2 倍攻速、
  1.5 倍移速。它不改怪物生命或死亡状态，也不修改生产配置。
- 每房断言初始怪物恰为 12、输入入队成功且零拒绝、真实 swing/命中存在、
  12 个 defeated 事件全部携带 `AttackId::launcher`，非 launcher 击杀为 0。
- 1000 房用例准确改名为
  `assisted clear thousand rooms preserves lifecycle and capacity`，只声明并
  继续验证辅助清场下的生命周期、容量、分配和溢出压力。

GREEN 运行中 10 房均自然清场：每房 8～12 次真实上挑，合计 120 次命中、
120 次 launcher 击杀、0 次非 launcher 击杀、0 次输入拒绝。直接运行完整
测试程序结果为 `294 cases, 0 failures`。

最终 fresh 门禁：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^dungeon\.units$' --output-on-failure
```

结果：`dungeon.units` Passed，276.33 秒；1/1 通过，0 失败。P1 修复只修改
`tests/dungeon/dungeon_stress_tests.cpp` 和本报告；未修改生产
Encounter/Session 数量，也未触碰 density、HUD、camera 或 save。

---

# 历史报告：Stage 17 Task 3

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

## 审查修复：progression 隔离与单一候选流程

### 审查问题验证

审查指出三个装配请求在复制 stable candidate 后执行：

```cpp
candidate.progression = room_progression_;
```

这会把本应仅存在于运行中房间的 progression 顺带写入 loadout 保存。进一步检查成功提交路径发现，即使只删除上述赋值，`commit_pending_save` 后段仍会无条件执行：

```cpp
room_progression_ = stable_state_.progression;
```

因此还会把运行中 progression 回退到旧 checkpoint。两点均需由 loadout kind 隔离。

### 修复 RED

新增 remove、equip、swap 三个独立真实 `SaveStore` 测试。每个场景都：

1. 用 `progression::apply_experience` 构造合法且与 stable checkpoint 不同的 `room_progression_`。
2. 断言请求成功后 pending 存在且 kind 为 `skill_loadout`。
3. 断言提交前完整 stable checkpoint 与 generation 不变，快照仍只显示 stable loadout，live progression 仍可见。
4. 构造 expected checkpoint：只允许 generation 加一并替换预期 loadout。
5. 断言 pending candidate 与 expected 完全 `same_run_state`。
6. 真实保存提交后，断言 published stable 与 expected 完全相同，同时 live progression 不丢失。

命令：

```powershell
$env:WindowsSDKVersion='10.0.26100.0\'
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests && set ARPG_STAGE17_SKILL_LOADOUT_ONLY=1 && out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe'
```

修复前结果：`9 cases, 3 failures`，失败项精确为 remove/equip/swap 三个 checkpoint-local 场景。

### 最小 GREEN 与去重

- 删除三处 `candidate.progression = room_progression_`，loadout candidate 保持 stable checkpoint 的 progression。
- 把 `skill_loadout` 成功分支移到通用 `room_progression_ = stable_state_.progression` 之前，完成 stable 发布后直接恢复原 phase 并返回；其他 transaction kind 的 progression 同步语义不变。
- 新增私有 `SkillLoadoutMutation` 枚举和 `request_skill_loadout_mutation` 单一流程；三个公开 API 只传递固定参数。
- 单一流程统一负责 phase/pending 检查、固定容量 candidate 复制、纯函数 mutation、整体 validate 和 `prepare_item_save`。
- 未使用 `std::function`、捕获闭包或新增容器，不引入新的堆分配。

聚焦 GREEN：`9 cases, 0 failures`。

### 审查修复回归

```powershell
ctest --test-dir out/build/windows-msvc-core-debug -R '^(dungeon\.units|stage16\.crafting_transaction\.units)$' --output-on-failure
```

结果：

- `dungeon.units`：Passed，165.82 秒（最终 fresh 复跑）。
- `stage16.crafting_transaction.units`：Passed，0.01 秒。
- 总计 2/2，通过率 100%，0 failures。

本轮额外修改测试支持文件：`tests/dungeon/dungeon_test_support.hpp`，只增加设置运行中 `room_progression_` 的既有 friend 测试 seam。
