# Task 1 报告：冻结八种怪物的新基础数值

## 实现内容

- 在 `kCatalog` 中仅更新八种怪物的 `max_hp` 与单一元素
  `contact_damage`，其余目录字段保持不变。
- 新增目录表测试，逐项验证八个 ID 的生命、伤害元素和其余元素槽均为零。
- 将战斗测试总用例数由 241 更新为 242。
- 同步所有由这组基础生命或伤害直接派生的断言：词条投影、深渊护盾、
  投射物、多重射击、冰冷附加伤害、黄金回放、玩家攻击和死亡来源场景。
  未改动词条定义、词条公式或生命药代码。

## 修改文件

- `src/combat/monster_catalog.cpp`
- `tests/combat/monster_catalog_tests.cpp`
- `tests/combat/monster_affix_runtime_tests.cpp`
- `tests/combat/monster_melee_tests.cpp`
- `tests/combat/combat_test_main.cpp`
- `tests/combat/break_stress_tests.cpp`
- `tests/combat/monster_affix_trigger_tests.cpp`
- `tests/combat/player_build_tests.cpp`
- `tests/combat/player_death_snapshot_tests.cpp`

后四个测试文件不在简报的初始 Files 列表中，但完整战斗套件证明其精确
断言直接依赖本任务替换的生命或伤害基线；为满足简报要求的完整套件绿色，
仅同步这些派生期望。

## RED

命令：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-core-debug
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure
```

结果：预期失败，`242 cases, 9 failures`。失败包含新增的
`monster_catalog.catalog matches approved health and damage table`，以及旧的
bulwark 词条投影、深渊护盾、chaos chaser 接触伤害、lightning shooter
投射物、chaos hazard 地面伤害和冷却词条组合断言。

说明：首次将简报代码原样放入匿名命名空间时，`modifiers` 需要在本仓库中
限定为 `arpg::modifiers`，否则为编译错误而非有效 RED；修正限定名后重新
执行上述命令并得到上述预期数值失败。

## GREEN

命令：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-core-debug
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure
```

结果：通过，`242 cases, 0 failures`；CTest 显示 `1/1 combat.units Passed`。

## 自审

- `git diff --check` 通过，没有空白错误。
- 已逐项核对目录精确表：
  `100/36`、`190/27`、`315/21`、`135/0`、`110/12`、`125/18`、
  `120/14`、`145/11`，元素分别为火、火、水、水、雷、雷、混沌、混沌。
- 目录外没有生产代码改动；没有改动词条深度、词条定义或生命药逻辑。

## 关注项

- 配置阶段会打印若干 `ARPG_DIRECT_INPUT_POISON_*_COMPILES - Failed` 探测项；
  它们是既有配置探测结果，构建、链接和 `combat.units` 测试均成功。

## 二次修复：保留死亡快照玩家夹具

提交：`c738053 test: preserve player death snapshot fixtures`

审查发现前一版为了让新版怪物伤害下的死亡来源测试继续致死，未经授权地将：

- `death_blast_reports_defeated_owner_source` 的
  `player_health_more_bp` 从 `2000` 改为 `1500`；
- `delayed_corrosion_keeps_the_affix_owner_source` 的玩家当前 HP 从 `65`
  改为 `30`。

本修复已将两项玩家输入完全恢复为 `2000` 和 `65`，没有改动生产玩家伤害、
防御、生命或生命药规则。

### RED

恢复玩家输入后运行：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-core-debug
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure
```

结果：`242 cases, 2 failures`。

- death blast 场景中，fire bomber 新接触伤害为 36，m1 death blast 为 120，
  合计不足原 200 HP 夹具，`tick_until_defeated(world, 180)` 失败；
- corrosion 场景中，chaos chaser 新接触伤害为 14，m1 corrosion 在 120 tick
  内仅两次 20 点伤害，未由延迟腐蚀完成致死，死亡来源断言失败。

### GREEN

- death blast 测试仅把该怪物的 death blast 场景设为 m2；36 接触伤害后，
  190 点 m2 爆炸保持原有“死亡爆炸归属为怪物词条”的断言语义。
- corrosion 测试仅把该怪物的 corrosion 场景设为 m2，并在首次成功施加腐蚀后
  冻结怪物 AI 240 tick；这样原 65 HP 夹具由两次 30 点延迟腐蚀致死，而非被
  后续接触攻击抢先致死。断言仍验证词条来源、拥有者、detail id、30 点原始伤害
  与混沌元素。

再次执行同一命令：`242 cases, 0 failures`，CTest 为 `1/1 combat.units Passed`。

二次自审：补丁只改 `tests/combat/player_death_snapshot_tests.cpp` 的怪物词条
层级和场景时序；`git diff --check` 通过，无生产代码改动。
