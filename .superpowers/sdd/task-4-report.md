# Stage 19 Task 4 实现报告：密度词条接入会话生命周期

## 结论

- `DungeonSession` 现在只保存瞬态 `RoomDensityRoll`，按当前房 seed 与
  `is_abyss` 确定性重算；未修改 checkpoint、版本号或序列化布局。
- 普通房目标数为词条基础数量 12～30；深渊房为
  `ceil(base * 1.5)`，最大 45。临时普通 12 / 深渊 18 已完全移除。
- 普通与深渊遭遇均在首次 combat tick 前一次装载全部怪物；生产 tick
  不再进入 `wave_delay`，`start_next_wave` 已移除。
- 快照公开 `density_affix`、`base_monster_count`、
  `initial_monster_count`，诊断同时公开初始数量和威胁和；
  `remaining_targets` 继续读取实时存活数。
- reset、普通/深渊构造、稳定状态重载、死亡撤退和死亡继续目标切换均重算
  瞬态密度；持久化真实重载测试验证三项派生值一致。

## TDD 证据

### RED 1：会话与深渊快照

先修改 wave/lifecycle 测试，再构建。编译只因缺少
`DungeonSnapshot::{density_affix,base_monster_count,initial_monster_count}`
和 `DungeonEncounterDiagnostics::initial_monster_count` 失败，证明新 API
在生产实现前被测试约束。

### RED 2：死亡继续切换

新增动态寻找“死亡房与目标房密度结果不同”的固定测试输入。生产修复前，
提交 death-continue 后的 transitioning 快照仍暴露旧房词条，精确失败于
`transitioning.density_affix == target_roll.affix`。加入目标房重算后，
Stage 11 death 聚焦套件为 `32 cases, 0 failures`。

### 回归 RED 与最小修复

首次完整回归准确暴露：

- encounter trace 仍是固定 12 数量的旧 golden；更新为密度驱动后的
  `0x58fdb223c26bd06a`。
- persistence 门移动夹具仍用 256 tick，无法穿越 Task 1 的四倍房间；
  只把测试移动上限同步为已有大房间使用的 640 tick。

纯输入门禁未改为辅助清场：10 个房间均只通过 launcher 输入自然清理，
实际初始怪物数覆盖 12、14、16、17、21、25，全部击杀归属 launcher，
输入拒绝为 0。

## 验证

- `ARPG_STAGE11_DEATH_ONLY=1`：32/32，0 failures。
- fresh `dungeon.units` 可执行程序：296/296，0 failures；包含 10 房纯输入、
  1000 房生命周期/容量、固定 trace 与深渊压力。
- fresh `persistence.units`：94/94，0 failures；既有 codec golden 和
  round-trip 全通过，真实 SaveStore 重载后派生密度一致。
- architecture：18/18，0 failures（从 `LastTest.log` 复核）。
- 全量 Debug 增量构建：退出码 0，最终 `ninja: no work to do`。
- `git diff --check`：通过。

## 范围

未实现 HUD 文本、房间渲染、相机或新存档字段；未改变词条目录、权重和
数量区间。旧 `RoomPhase::wave_delay` 枚举仅为源码兼容保留。

---

# Stage 16 Task 4 实现报告

## 状态

完成材料独立地面池、确定性掉落、战斗近距自动拾取、清房原子真空吸取，以及 V7 材料认领位图持久化。未修改 raylib 背包 UI、材料消费或 Boss 逻辑。

## 实现

- 新增 400 槽 `GroundMaterial` 固定池，与 192 槽装备地面池、装备 RNG 和金装路径完全独立；普通/券掉落固定映射为 `spawn*2` / `spawn*2+1`，深渊奖励使用 384 起始保留槽。
- 材料概率、材料类型、强化券阶级、深渊材料分别使用独立 RNG domain；普通材料和强化券可在同一怪物上同时掉落，深度/危险分阈值与权重按正式设计冻结。
- 仅在 `combat`/`wave_delay` 阶段按距离自动拾取；单个拾取使用 `material_pickup` 原子保存，失败保留地面物。
- 普通房与深渊清房使用 `room_clear` / `abyss_clear` 单一事务，把剩余材料、发现位和认领位一起写入 next state；提交后才清空地面池并发布拾取回执。
- 死亡丢弃未拾取地面材料，保留已提交计数，并清空新房间的材料认领位；过渡、深渊失败同样清理房间局部位图。
- `ItemOwnershipState` 新增 7 个 64 位材料认领字。V7 追加 56 字节，旧 V1-V6 解码为全零，并拒绝 ordinal 400 以上的高位。
- 地面槽冲突不会覆盖已有材料，记录 `material_ground_saturation_count`；快照发布固定材料列表、待拾取 ordinal 和逐材料提交回执。

## TDD 证据

- 纯逻辑 RED：缺少 `material_loot.hpp` 及掉落接口；实现后焦点纯逻辑 5/5。
- 持久化 RED：缺少材料认领字段/V7 尺寸；实现后 persistence 85/85。
- 会话 RED：缺少独立地面池、拾取与清房事务接口；实现后初始 Stage16 焦点 11/11。
- 审计 RED：已有材料认领位后死亡，焦点用例在 `resolve_committed(session)` 失败；精确重建同步清位后恢复 11/11。
- Important 审查 RED：真实 tick 清房产生 `room_clear` 后，提交中出口已经开放；`not_committed` 又恢复到 `cleared`，下一 tick 绕过真空重试。新用例明确失败于 `!open`。普通房 pending 的恢复阶段改为 `combat` 后，下一真实 tick 自动重建完全相同的真空事务，焦点恢复 12/12；深渊恢复阶段和受保护保存语义未改。
- 全量回归先暴露 Windows Debug 默认 1MB 测试栈溢出，以及旧 helper 未提交 `room_clear`；测试目标栈储备提升到 4MB，旧驱动按真实保存边界提交并重新计数分配，生产事务未放宽。

## 最终验证

```text
scripts/Build.ps1 -Preset windows-msvc-debug
  PASS，完整 MSVC Debug 构建

ctest.exe --preset windows-msvc-debug -R '^stage16\.material_loot\.units$' --output-on-failure
  PASS，12/12，0.07s

ctest.exe --preset windows-msvc-debug -R '^dungeon\.units$' --output-on-failure
  PASS，266/266，188.12s

ctest.exe --preset windows-msvc-debug -R '^persistence\.units$' --output-on-failure
  PASS，85/85，0.40s

git diff --check
  PASS（仅 Git 的 LF->CRLF 工作树提示，无空白错误）
```

## 主要文件

- `src/dungeon/material_loot.hpp/.cpp`
- `src/dungeon/dungeon_session.hpp/.cpp`
- `src/dungeon/dungeon_transition.cpp`
- `src/dungeon/dungeon_snapshot.cpp`
- `src/dungeon/dungeon_types.hpp`
- `src/items/item_types.hpp`, `src/items/item_catalog.cpp`
- `src/persistence/checkpoint_codec.hpp/.cpp`, `src/persistence/save_slot.cpp`
- `tests/dungeon/dungeon_material_loot_tests.cpp` 及全量驱动兼容更新
- `tests/persistence/checkpoint_codec_tests.cpp`, `death_checkpoint_codec_tests.cpp`, `dungeon_save_integration_tests.cpp`

## 注意事项

- 400 槽生产池保持固定容量、无清房热路径动态扩容。Windows Debug 测试因多个完整 Session/快照同时驻栈，测试可执行文件单独预留 4MB；生产可执行文件链接设置未改。
- V7 仍是当前格式版本，只扩展其固定基区；旧 V7 测试金样已同步更新，V1-V6 迁移行为不变。
- 普通房 `room_clear` 在 pending 与 `not_committed` 后均保持出口关闭；只有真空事务 committed 后才进入 `cleared` 并发布出口事件。
- 无已知功能顾虑；本报告与实现同一提交。

---

# 历史报告：Stage 11-D Task 4

## 状态

完成 Production Snapshot Metadata and Runtime Mapping，并按真实 RED -> GREEN 执行 TDD。

## 实现

- `GroundItemSnapshot` 新增 `base_id`、`item_level`，快照直接复制已存在的生产 `ItemInstance` 字段。
- `DungeonSnapshot` 新增 `pending_pickup_ordinal`；仅 `loot_pickup` 与 `abyss_reward_claim` 从真实 `PendingSave::pickup_ordinal` 填值，其余 pending kind 保持 `nullopt`。
- `DungeonRuntime::fixed_tick` 接受可选 `AutoPickupPolicy` 并透传给 `DungeonSession::tick`；默认参数仍为 `normal`，兼容原有 show-all 调用者。
- host 边界新增 `loot_pickup_policy(settings::LootFilterMode)`，三档映射为 normal/magic/rare。
- 唯一 host fixed-tick 调用每步读取 `live_settings.loot_filter_mode`；未读取 `pause_menu.draft`。
- 未实现 ground label、renderer 或 feedback。

## TDD 证据

### RED

命令：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_platform_tests -- -j1
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
cmake -DSOURCE_ROOT=E:/game/.worktrees/stage11d-loot-filter -DGUARD_TEST_ROOT=E:/game/.worktrees/stage11d-loot-filter/out/build/windows-msvc-debug/tests/platform/stage11b-evidence-guard -P tests/platform/stage11b_settings_evidence_guard_test.cmake
```

预期失败：

- dungeon：`GroundItemSnapshot` 缺少 `base_id`/`item_level`，`DungeonSnapshot` 缺少 `pending_pickup_ordinal`。
- platform：缺少 `loot_pickup_policy`，`DungeonRuntime::fixed_tick` 不接受两个参数。
- host guard：`Stage11B evidence guard requires live loot policy behind host gate`。

说明：首次普通构建因工作树构建目录写权限/PDB 竞争失败，不计作 RED；加载 MSVC 开发环境并在沙箱外串行构建后取得上述有效 RED。

### GREEN

构建：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_platform_tests -- -j1
```

结果：成功链接 `arpg_dungeon_tests.exe` 与 `arpg_platform_tests.exe`。增量复验同样成功。

主 focused tests：

```powershell
ctest --preset windows-msvc-debug -R "^(dungeon.units|platform.units|stage11b.settings_evidence_guard)$"
```

提交前 fresh 结果：3/3 通过，0 失败；`dungeon.units` 180.84 秒，`platform.units` 3.20 秒，host evidence guard 0.01 秒。

补充 stress/architecture tests：

```powershell
ctest --preset windows-msvc-debug -R "^(stage10.abyss_stress.determinism_and_zero_alloc|platform.input_latency_source|stage11b.settings_evidence_guard_self_test|stage11c.architecture.hud_boundaries|stage11c.architecture.hud_boundaries_self_test)$"
```

结果：5/5 通过，0 失败。

## 文件

- `src/dungeon/dungeon_types.hpp`
- `src/dungeon/dungeon_snapshot.cpp`
- `src/platform/raylib/dungeon_runtime.hpp`
- `src/platform/raylib/dungeon_runtime.cpp`
- `src/platform/raylib/raylib_host.hpp`
- `src/platform/raylib/raylib_host.cpp`
- `tests/dungeon/dungeon_loot_drop_tests.cpp`
- `tests/dungeon/dungeon_abyss_reward_tests.cpp`
- `tests/dungeon/dungeon_stress_tests.cpp`
- `tests/platform/dungeon_runtime_tests.cpp`
- `tests/platform/platform_test_main.cpp`
- `tests/platform/stage11b_settings_evidence_guard_test.cmake`
- `.superpowers/sdd/task-4-report.md`

## 提交

主题：`feat: expose filtered ground loot snapshots`

## 自审

- 新 snapshot 字段只来自现有生产 ground item，不复制额外对象或新增分配。
- pending ordinal 分支严格限制为两种 pickup save kind；transition 等其他 save kind 已断言为无值。
- rare-only runtime 会保留 normal drop；省略 policy 的既有调用仍自动拾取 normal drop。
- source guard 同时要求 `live_settings` 映射并拒绝 `pause_menu.draft` 映射。
- dungeon stress 的 snapshot 等价比较已包含新增字段、source 与 abyss ordinal。
- 没有新增 dungeon -> platform 反向依赖；依赖方向仍为 platform 调用 dungeon。

## 顾虑

- 无已知功能顾虑。
- MSVC 构建必须加载 Visual Studio Developer Command Prompt；沙箱内现有 build 目录有写权限/PDB 锁限制，因此验证在获批的沙箱外串行执行。
- 构建日志中 `host_input_tests.cpp` 有既存 C4834 警告，本任务未修改该文件，测试仍全部通过。

## 审查修复：完整 host policy 调用守卫

### 发现与实现

- 原 guard 分别查找 `runtime.fixed_tick` 与 live-policy 片段，只验证位置顺序；`fixed_tick(step_movement, {})` 后放一个未使用的 live-policy 表达式即可通过。
- guard 现在先删除 host 源中的空白，再匹配完整调用：`runtime.fixed_tick(step_movement,loot_pickup_policy(live_settings.loot_filter_mode));`。
- 原有 fixed-tick 唯一路径计数和 host gate 顺序检查保持不变，没有放宽门禁。
- 完整 draft-policy 调用优先给出 `rejects draft loot policy in fixed_tick`，避免误报为缺少 live policy。
- self-test 新增两项 `HOST_OVERRIDE` mutation：draft policy；default `{}` policy + 未使用 live-policy 诱饵。
- mapper unit case 新增非法 `LootFilterMode(0xFF)` 回退到 `ItemRarity::normal` 的断言。

### 审查修复 RED

命令：

```powershell
ctest --preset windows-msvc-debug -R "^stage11b.settings_evidence_guard_self_test$" --output-on-failure
```

有效 RED：1/1 CTest 失败；旧 guard 错误接受 default-policy + live-policy 诱饵，self-test 报：

```text
evidence guard self-test accepted default policy with live-policy decoy
```

在调整 mutation 执行顺序前还确认：旧 guard 能拒绝 draft mutation，但错误报告 `requires live loot policy behind host gate`，未给出 draft 来源错误。

### 审查修复 GREEN

增量构建：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
```

结果：2 个增量步骤成功，`arpg_platform_tests.exe` 链接成功。

focused tests：

```powershell
ctest --preset windows-msvc-debug -R "^(platform.units|stage11b.settings_evidence_guard|stage11b.settings_evidence_guard_self_test|platform.input_latency_source|stage11c.architecture.hud_boundaries|stage11c.architecture.hud_boundaries_self_test)$" --output-on-failure
```

结果：6/6 CTests 通过，0 失败，总耗时 22.92 秒：

- `platform.units`：通过，包含 244 个 unit cases。
- `stage11b.settings_evidence_guard`：通过。
- `stage11b.settings_evidence_guard_self_test`：通过；共覆盖 13 个 mutation scenarios，其中新增 2 个 loot-policy mutations。
- `platform.input_latency_source`：通过。
- `stage11c.architecture.hud_boundaries`：通过。
- `stage11c.architecture.hud_boundaries_self_test`：通过。

### 审查修复文件

- `tests/platform/dungeon_runtime_tests.cpp`
- `tests/platform/stage11b_settings_evidence_guard_test.cmake`
- `tests/platform/stage11b_settings_evidence_guard_self_test.cmake`
- `.superpowers/sdd/task-4-report.md`

### 审查修复提交

主题：`test: harden host loot policy evidence guard`

### 审查修复自审与顾虑

- 完整调用匹配绑定了 `fixed_tick` 第二参数与 `live_settings`，单独诱饵表达式不能满足门禁。
- draft 与 decoy mutations 都 fail closed，并校验具体失败原因。
- 未修改生产 runtime/snapshot 行为。
- 无已知功能顾虑；mutation 的输入替换按当前生产调用格式构造，若格式漂移导致替换失效，self-test 会失败而不是静默通过。

---

# Stage 17 Task 4 报告：CombatWorld 技能运行态与拔刀斩

## 实现

- 新增固定容量 `ActiveSkillRuntime`：公开快照、每技能冷却和每怪物命中锁存均为固定数组。
- `CombatWorld::request_active_skill` 支持拔刀斩请求、冷却/无效技能/玩家不可用/普攻活动/另一技能活动结果，并在成功时锁定施放中心与朝向、立即开始 240 tick 冷却。
- 拔刀斩第 10 tick 以锁定中心和锁定朝向判定前方线性展宽区域；每个目标在单次施放中只会受击一次。
- 命中调用现有 `build_player_hit_packet`，沿用怪物护盾、HP、破韧、破甲窗口和 `ImpactKind::medium_hitstun` / 水平击退行为；没有加入 `AttackId` 连段。
- 施放期间移动输入继续驱动水平移动但不改朝向；J/L 不会被消费，K 仍保留在已有输入缓冲及过期逻辑中。
- `reset`、`load_wave`、死亡路径均清除技能施放态和冷却；战斗模块未引入 persistence 依赖。

## 变更文件

- `src/combat/active_skill_runtime.hpp`
- `src/combat/active_skill_runtime.cpp`
- `src/combat/combat_types.hpp`
- `src/combat/combat_world.hpp`
- `src/combat/combat_world.cpp`
- `src/combat/combat_snapshot.cpp`
- `src/combat/CMakeLists.txt`
- `tests/combat/draw_slash_skill_tests.cpp`
- `tests/combat/CMakeLists.txt`
- `tests/combat/combat_test_main.cpp`

## TDD 证据

### RED

先新增 `draw_slash_skill_tests.cpp`、测试 CMake 和测试入口，未创建运行态头文件时执行：

```powershell
$env:WindowsSDKVersion = '10.0.26100.0\'
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests'
```

实际失败：`fatal error C1083: 无法打开包括文件: “combat/active_skill_runtime.hpp”: No such file or directory`。

首次原始构建命令还因当前 shell 未加载 SDK 环境而在 CMake 门禁处报告 `Windows SDK ... detected ''`；加载 vcvars 并明确设置已安装的 10.0.26100.0 后，得到上述预期 API/头文件缺失的 RED。

### GREEN

```powershell
$env:WindowsSDKVersion = '10.0.26100.0\'
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests'
$env:ARPG_STAGE17_DRAW_SLASH_ONLY='1'
& out/build/windows-msvc-core-debug/bin/arpg_combat_tests.exe
Remove-Item Env:ARPG_STAGE17_DRAW_SLASH_ONLY
```

输出：`7 cases, 0 failures`。

## 回归

```powershell
$env:WindowsSDKVersion = '10.0.26100.0\'
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests'
ctest --test-dir out/build/windows-msvc-core-debug -R '^(combat\.units|architecture\.combat_no_persistence)$' --output-on-failure
```

输出：2/2 通过；`combat.units` 0.79 秒，`architecture.combat_no_persistence` 8.12 秒。

## 自审与顾虑

- 已核对技能不进入 `AttackId`、运行态无动态分配、`reset/load_wave` 清除瞬态，且架构边界测试通过。
- 旧训练场第三个靶是重甲；其未破韧前按既有管线不会接受击退，因此测试用两个非重甲目标验证水平击退，并仍验证三个前方目标均受到物理伤害。
- 暴风式在本任务范围外，当前有效但未实现的 `storm_swords` 请求返回 `invalid_skill`；Task 5 需扩展该分支和伤害/时序，不能把本任务的拔刀斩逻辑当作暴风式实现。

---

# Stage 17 Task 4 审查修复

## 修复

- 冷却推进从施放状态机拆出，在每次存活的 `CombatWorld::tick()` 开始时递减；不再被 hit-stop 或 hurt 暂停。
- 在 `hit_resolution.cpp` 抽取单一 `resolve_player_attack_hit` 命中管线。普攻与拔刀斩均通过该函数处理 `build_player_hit_packet`、护盾、HP、破韧、Impact、hit-stop 和事件；参数化攻击来源、物理伤害、破韧、Impact、击退、反馈。拔刀斩仅保留扇形目标选择，未进入 `AttackId` 连段。
- 真实 `apply_player_damage` 致死分支现在立即清除 `ActiveSkillRuntime`，包括施放快照、命中锁存与冷却。
- 技能状态机按世界 tick 推进，因此拔刀斩严格在第 10 tick 命中，随后第 11-24 tick 为 14 tick 后摇；自身命中 hit-stop 不再改变该时序。

## 审查 RED

先新增硬直冷却、精确时序/数值/边界、真实致死、零分配断言后执行：

```powershell
$env:WindowsSDKVersion = '10.0.26100.0\'
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests'
$env:ARPG_STAGE17_DRAW_SLASH_ONLY='1'
& out/build/windows-msvc-core-debug/bin/arpg_combat_tests.exe
Remove-Item Env:ARPG_STAGE17_DRAW_SLASH_ONLY
```

实际 RED：

- `cooldown advances during hurt`：240 world ticks 后冷却未归零。
- `lethal player damage cancels skill`：真实致死后 `active_skill.id` 仍是拔刀斩。
- 统一管线抽取后的第一次运行还暴露精确时序 RED：自身 medium hit-stop 暂停状态机，第 24 tick 未达到预期后摇末尾。

## 审查 GREEN 与回归

同一 focused 命令在最小修复后输出：`13 cases, 0 failures`。

```powershell
$env:WindowsSDKVersion = '10.0.26100.0\'
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests'
ctest --test-dir out/build/windows-msvc-core-debug -R '^(combat\.units|architecture\.combat_no_persistence)$' --output-on-failure
```

输出：2/2 通过，`combat.units` 0.80 秒，`architecture.combat_no_persistence` 7.91 秒。

## 自审

- 新增测试覆盖硬直期间 240 tick 冷却、第 10 tick 命中、14 tick 后摇、220 物理/36 破韧、5.0/3.2 边界、真实致死取消和请求/命中/恢复热路径零分配。
- 普攻完整回归通过，证明共享命中管线未改变既有普攻行为。
- 无已知功能顾虑；暴风式仍保持 Task 5 范围外。
