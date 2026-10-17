# Task 9 — Stage11D 正式证据修复报告

状态：DONE

## 修改文件

- `src/platform/raylib/host_validation_stage11d.hpp`
- `src/platform/raylib/host_validation_stage11d_report.cpp`
- `tests/platform/stage11d_loot_formal_game_validation.cpp`
- `tests/platform/stage11d_loot_formal_validator.ps1`
- `tests/platform/stage11d_loot_evidence_guard_test.cmake`
- `tests/platform/stage11d_loot_evidence_guard_self_test.cmake`

未修改 Stage11C、`src/checkpoint/room_checkpoint_schema.hpp`、`src/dungeon/dungeon_transition.cpp` 或任何正式证据 PNG/TXT。

## 要求逐项落实

1. 玩家受伤且存活：`Stage11DLootValidationState` 新增 `player_max_hp` 与单调 `player_damage_observed`。生产快照记录仅在 `0 < hp < max_hp` 时置位；summary 输出 `progress_hp`、`progress_max_hp` 和观测位。验证器要求观测位为 1、最终 `0 < hp <= max_hp`，因此怪物受伤位不再可替代玩家掉血证据。
2. 精确拾取守恒：pickup 场景从 show-all 的三个原始 ID 计算“除精确 picked ID 外的集合”，并用 `Compare-Object` 与剩余地面 ID 比较；仅“剩两个”不再足够。
3. 精确 rarity tuple：正式 manifest 分别写入 `ordinary_normal_tuple`、`ordinary_magic_tuple`、`ordinary_rare_tuple`，每项为 `(item_id, monster_ordinal)`。每个普通房场景将自己的 `observed_normal/magic/rare` 与对应 tuple 完全相等比较；未再把 ID/ordinal 拆成无序集合。
4. A/B：不新增 replay。Task 9 的计划/设计只要求六个确定性场景的各一次真实生产 host 验证；每个场景本来就是独立目录、真实 `SaveStore`/`SettingsStore` 提交后冷启动。持久化 A/B 槽选择与轮转由 `tests/persistence/save_store_tests.cpp`、`tests/persistence/save_store_fault_tests.cpp` 独立覆盖，设置 A/B 字节确定性由 `tests/settings/settings_stress_tests.cpp` 独立覆盖。正式 gate 不宣称已证明同场景字面 A/B。
5. 防旁路守卫：guard 从 C++ 已消毒的真实状态/函数体检查玩家观测与 summary 转发；对 validator 则检查精确三 tuple 比较和精确 pickup 集合比较。self-test 增加三项变异：删除玩家掉血观测、把三 tuple 缩为 normal-only、把集合守恒缩为计数，三项各自期望被拒绝，避免注释或无关位置的 token 充数。

## RED / GREEN 记录

RED（先添加 self-test 变异后执行）：

```powershell
cmake -DSOURCE_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801' -DGUARD_TEST_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801/out/task9-red-guard-mutations' -P tests/platform/stage11d_loot_evidence_guard_self_test.cmake
```

实际结果：失败。self-test 的 baseline 先被已有无关 guard 失败阻断：`Stage11D loot evidence guard rejected its baseline`；直接运行 guard 定位为 `Stage11D loot evidence guard rejected a draw-external renderer plan`。未启动 raylib 或六场景正式测试。

GREEN：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug; cmake --build out/build/windows-msvc-debug --target arpg_stage11d_loot_formal --config Debug -j 1
```

实际结果：成功（6/6 Ninja steps），编译并链接 `arpg_stage11d_loot_formal.exe`；没有启动游戏。

```powershell
$tokens=$null; $errors=$null; [System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path 'tests/platform/stage11d_loot_formal_validator.ps1'), [ref]$tokens, [ref]$errors) | Out-Null; if ($errors.Count -ne 0) { exit 1 }; 'PowerShell validator syntax: PASS'
```

实际结果：`PowerShell validator syntax: PASS`。

补充：初次未加载 VS 开发环境的直接 target build 在 raylib 编译时因 `stdarg.h` 缺失失败；随后使用项目 `Configure.ps1` 加载要求的 MSVC/SDK 环境后，同一目标编译成功。

## A/B 结论

正式证据仍是“真实单槽冷启动集成验证”，不是同场景 A/B replay。现有独立持久化和设置测试承担 A/B/字节确定性证明；本任务没有重复或伪造这一声明。

## Self-review

- 未触碰禁止文件、Stage11C 或证据文件；目标序号锁仅修正为按目标实体死亡/对应掉落确认推进，不再把全局击杀计数当作目标死亡。
- 玩家观测保持固定字段、无动态分配，且没有公共 snapshot setter、直接拾取、假 renderer 或截图旁路。
- 新 manifest tuple 从现有生产派生的 room plan/drop 数据写出，验证器只消费真实 summary/manifest。
- 初次 renderer guard 基线失败已在下列修复轮复测通过。

## 修复轮 1/5（guard 与目标锁）

- 验证器不再要求诊断字段 `monster_damage_observed=1`，普通三掉落只要求实际 `defeated >= 3`，不把 fixture 的 `ordinary_prefix_kills=4` 当作前置击杀数。
- renderer guard 仅在 `CombatRenderer::draw` 的真实函数体内允许且要求一次 `make_combat_render_plan`；函数定义外的可执行调用会被拒绝。tuple 与 pickup 守卫要求 PowerShell brace depth 为 0，不能移入未调用函数或 `$false` 分支。
- 优先目标锁仅在该目标实体明确死亡（`hp <= 0`、reaction/AI 已 defeated）或存在同 ordinal 的 `monster_drop` 时推进；缺失目标且只有其他怪死亡时保持等待，ordinal 不回退。

### 本轮验证

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

实际结果：GREEN，`platform.units` 1/1 通过（16.39 秒）。此前新增的“缺失锁定目标、全局击杀数增加”RED 断言失败；修正后通过。

```powershell
cmake -DSOURCE_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801' -P tests/platform/stage11d_loot_evidence_guard_test.cmake
```

实际结果：`Stage11D loot formal evidence guard passed`。

```powershell
cmake -DSOURCE_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801' -DGUARD_TEST_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801/out/tmp/stage11d-task9-targeted-selftest' -DTASK9_TARGETED_ONLY=ON -P tests/platform/stage11d_loot_evidence_guard_self_test.cmake
```

实际结果：退出码 `0`，`Stage11D Task9 targeted evidence guard passed: bad_mutations=9`。该入口先执行 baseline guard，随后仅执行本轮九个承重拒绝变异，另含一个无害 renderer 定义诱饵接受用例；不运行旧的全量 90 项变异链。

未按本任务范围运行 raylib 六场景正式 gate；Windows 画面、输入与音频验收仍待本机实际运行。

## 修复轮 2/5（PowerShell AST 执行域）

- 新增 `tests/platform/stage11d_loot_validator_ast_guard.ps1`，由 evidence guard 显式检查存在并以 `Parser.ParseFile` 调用。解析错误立即 fail closed。
- AST guard 只接受顶层真实 `AssignmentStatementAst`：`$ordinaryTupleChecks` 的非字符串 RHS 必须含五场景、三 rarity 的 15 个实际比较；相应 `Require` 必须是顶层可执行 `PipelineAst/CommandAst`。`$expectedPickupSnapshot` 同样要求顶层非字符串赋值、真实 `Where-Object` 差集及顶层 `Compare-Object` 守恒 `Require`。
- self-test 增加 tuple 与 pickup 顶层 here-string 迁移变异。`TASK9_TARGETED_ONLY` 现执行 11 个承重拒绝变异，并跳过无关 host sequence/accepted helper。

### 本轮 RED / GREEN

RED：

```powershell
cmake -DSOURCE_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801' -DGUARD_TEST_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801/out/tmp/stage11d-task9-round2-red' -DTASK9_TARGETED_ONLY=ON -P tests/platform/stage11d_loot_evidence_guard_self_test.cmake
```

实际结果：退出码 `1`，`Stage11D loot evidence guard accepted mutation: tuple assignment cannot move into a here string`，证明旧 raw-token/brace-depth 判断确实可被 inert here-string 绕过。

GREEN：PowerShell parser syntax 通过；baseline `stage11d_loot_evidence_guard_test.cmake` 通过。

```powershell
cmake -DSOURCE_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801' -DGUARD_TEST_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801/out/tmp/stage11d-task9-round2-green-retry' -DTASK9_TARGETED_ONLY=ON -P tests/platform/stage11d_loot_evidence_guard_self_test.cmake
```

实际结果：退出码 `0`，`Stage11D Task9 targeted evidence guard passed: bad_mutations=11`。

## 修复轮 3/5（AST 比较与谓词语义）

- `$ordinaryTupleChecks` 的 15 个比较改为括号化的独立数组元素；AST guard 不再读取 `Right.Extent.Text`，而是逐项验证实际 `BinaryExpressionAst`、`Ieq`、左右 `MemberExpressionAst` 绑定，并以精确 15 项集合去重匹配。
- pickup 守卫不再在 RHS 文本中搜索谓词。它要求 `$expectedPickupSnapshot` 顶层数组 RHS 的直接 pipeline 为 `$ordinaryIds | Where-Object { ... }`，并要求 `Where-Object` 的实际 scriptblock 只有一个 `Ine` 谓词：`$_ -ne $pickup.pickup_item_id`。
- self-test 新增“tuple 比较注释化”和“pickup 谓词注释化”两项；`ROUND3_TARGETED_ONLY` 只执行 baseline 加这两项，`TASK9_TARGETED_ONLY` 清单扩至 13 项。

### 本轮 RED / GREEN

RED：

```powershell
cmake -DSOURCE_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801' -DGUARD_TEST_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801/out/tmp/stage11d-task9-round3-red' -DROUND3_TARGETED_ONLY=ON -P tests/platform/stage11d_loot_evidence_guard_self_test.cmake
```

实际结果：退出码 `1`，旧 guard 错误接受 `tuple comparisons cannot move into comments`。第二条 pickup-only RED 同样退出码 `1`，命中 `pickup predicate cannot move into a comment`。

GREEN：PowerShell parser syntax 通过；baseline `stage11d_loot_evidence_guard_test.cmake` 通过。

```powershell
cmake -DSOURCE_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801' -DGUARD_TEST_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801/out/tmp/stage11d-task9-round3-green' -DROUND3_TARGETED_ONLY=ON -P tests/platform/stage11d_loot_evidence_guard_self_test.cmake
```

实际结果：退出码 `0`，`Stage11D Task9 round3 targeted evidence guard passed: bad_mutations=2`。

### 修复轮 3/5 补充（消费顺序）

- AST guard 现在要求 tuple 与 pickup 的顶层 Assignment 使用 `=`，并且对应顶层 `Require` 必须位于同一 `EndBlock.Statements` 中紧随该 Assignment；不能把真实赋值移到 Require 之后。成员、变量 splat、直接 pipeline、Where 调用形式以及 Where predicate scriptblock 也维持 fail-closed 结构约束。
- 新增 tuple/pickup 两条错序变异。RED：`ROUND3_TARGETED_ONLY` 退出码 `1`，命中 `tuple assignment cannot move after its Require` 被旧 guard 接受。

```powershell
cmake -DSOURCE_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801' -DGUARD_TEST_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801/out/tmp/stage11d-task9-round3-order-green' -DROUND3_TARGETED_ONLY=ON -P tests/platform/stage11d_loot_evidence_guard_self_test.cmake
```

GREEN：退出码 `0`，`Stage11D Task9 round3 targeted evidence guard passed: bad_mutations=4`。本轮 PowerShell parser syntax 与 baseline `stage11d_loot_evidence_guard_test.cmake` 也均通过。

```powershell
cmake -DSOURCE_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801' -DGUARD_TEST_ROOT='E:/game/.worktrees/finish-unfinished-branches-20260801/out/tmp/stage11d-task9-round3-full-green' -DTASK9_TARGETED_ONLY=ON -P tests/platform/stage11d_loot_evidence_guard_self_test.cmake
```

GREEN：退出码 `0`，`Stage11D Task9 targeted evidence guard passed: bad_mutations=15`。
