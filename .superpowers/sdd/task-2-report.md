# Stage 11-A Task 2 实现报告

## Status

完成。CombatWorld 已接入稳定伤害来源、资源上限裁剪后的逐类型实际承伤、5 秒历史、最后一击死亡快照与死亡后的模拟冻结。

## RED

首次直接执行简报命令时，当前 PowerShell 未加载 VS Developer 环境，CMake 前置检查报告 `Stage 0 requires Windows SDK 10.0.26100.0; detected ''`，因此该次不作为有效 RED。

加载本机 VS 2022 BuildTools 环境后执行：

```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build --preset windows-msvc-debug --target arpg_combat_tests'
```

关键输出：

```text
combat_test_support.hpp(31): error C2039: "PlayerDamageSource": 不是 "arpg::combat" 的成员
player_death_snapshot_tests.cpp(48): error C2039: "death_snapshot": 不是 "arpg::combat::CombatWorld" 的成员
ninja: build stopped: subcommand failed.
```

失败原因正是新来源类型、来源参数和死亡快照接口尚未实现，符合预期 RED。

## GREEN 与最终验证

最终执行：

```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build --preset windows-msvc-debug --target arpg_combat_tests && out\build\windows-msvc-debug\bin\arpg_combat_tests.exe && ctest --preset windows-msvc-debug -R "^combat\.units$" --output-on-failure'
```

关键输出：

```text
188 cases, 0 failures
1/1 Test #3: combat.units ... Passed
100% tests passed, 0 tests failed out of 1
```

## 来源矩阵

| 实际生产路径 | kind | monster | detail_id |
|---|---|---|---|
| 怪物接触/近战/冲锋 | `monster_attack` | 有效 slot 的稳定 `MonsterId` | `0` |
| 怪物投射物 | `projectile` | projectile owner handle 解析出的 `MonsterId` | `0` |
| 原生怪物危险区 | `ground_hazard` | hazard owner handle 解析出的 `MonsterId` | `HazardKind` |
| burning/chain/death hazard | `monster_affix` | hazard owner 的 `MonsterId` | 对应 `MonsterAffixId` |
| 延迟腐蚀 tick | `monster_affix` | 状态施加时冻结的 `MonsterId` | `chaos_corrosion` |
| 深渊环境 | `abyss_environment` | `MonsterId::count` | 当前 `AbyssRuleId` |
| owner 无法解析 | `unknown` | `MonsterId::count` | `0` |

AI 文件无需直接修改：近战和特殊 AI 已统一进入 `apply_monster_direct_hit`，远程 AI 创建的 projectile 保留 owner handle；来源在这些真实调用链的统一伤害交付点解析。没有伪造默认怪物。

## 资源裁剪与死亡快照

- `apply_player_damage` 使用 uint64 的 `min(resolved.total, max(0, barrier) + max(0, hp))`。
- 按 resolved 五类型比例向下分配，余数按物理、火、水、电、混沌补给仍有容量的类型；测试覆盖 1000→50 和 2→1 的确定性余数。
- 仅 resource-capped `actual_by_type` 写历史；闪避、无敌、零伤害和过量部分不写入。
- 先扣护盾再扣生命；死亡快照保存原始正分量饱和和、实际损失、最近五类型历史、扣除后的防御字段和确定性 primary type。
- 第一笔致死伤害立即锁定 snapshot，后续同 tick 伤害入口直接拒绝。

## 冻结与零分配证据

- 600 tick 测试验证：外层 tick 仅递增；玩家位置/状态计时、怪物位置/AI、投射物 lifetime、危险区 lifetime、输入诊断、历史和 death snapshot 均不变化。
- 事件测试验证 `player_defeated` 只发一次，死亡后队列动作不接受也不执行。
- 完整深渊环境致死 tick 前后 `allocation_count()` 相等；实现未引入 `new`、容器扩容或堆路径。
- 固定历史使既有 MSVC Debug launcher 测试函数栈帧达到约 932 KiB，超过测试进程默认 1 MiB 调用栈；仅将 `arpg_combat_tests` 的 MSVC 栈设为 2 MiB，生产代码仍保持固定内存和零热路径分配。

## 文件列表

- `src/combat/combat_types.hpp`
- `src/combat/combat_world.hpp`
- `src/combat/combat_world.cpp`
- `tests/combat/player_death_snapshot_tests.cpp`
- `tests/combat/combat_test_main.cpp`
- `tests/combat/combat_test_support.hpp`
- `tests/combat/player_defense_tests.cpp`
- `tests/combat/CMakeLists.txt`

未修改 Dungeon、Persistence、raylib、AI 实现文件或词缀/深渊目录；Combat 源文件没有这些层的 include。

## 疑虑

- 无功能性阻塞。
- 测试栈提升仅针对 MSVC Combat 测试二进制，原因和证据见上；若未来继续显著扩大 `CombatWorld` 固定成员，应单独评估对象布局，而不是在热路径改用堆分配。

---

## 审查修复追加（2026-07-16，以本节为准）

### 修复 RED

先将死亡快照 suite 改为真实 tick producer，并加入只读怪物内部状态检查；未修改生产代码时执行：

```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build --preset windows-msvc-debug --target arpg_combat_tests && set ARPG_TEST_TRACE=1&& out\build\windows-msvc-debug\bin\arpg_combat_tests.exe'
```

关键失败：

```text
[FAIL] player_death_snapshot.melee immediate death freeze:
monster_contact_resolved(world, 0U) == contact_before
[FAIL] player_death_snapshot.bomber immediate death freeze:
after.hp == before.hp
[FAIL] player_death_snapshot.canonical malformed sources:
actual.kind == PlayerDamageSourceKind::unknown
196 cases, 3 failures
```

这三项分别证明：近战致死返回后仍写 contact 状态、自爆怪仍将自身置零并继续 defeat 流程、入口未 canonicalize 畸形来源。

### 生产修复

- `apply_player_damage` 入口统一 canonicalize：
  - `monster_attack`/`projectile` 要求有效 `MonsterId` 并强制 `detail_id=0`；
  - `ground_hazard`/`monster_affix` 的 monster 无效时整体变为 `unknown,count,0`；
  - `abyss_environment` 要求合法 `AbyssRuleId`，并强制 monster 为 `count`；
  - `unknown` 和非法 kind 固定为 `unknown,count,0`。
- `monster_ai.cpp` 在真实直接命中致死后立即返回，不再清除 blink empowered。
- `monster_ai_melee.cpp` 在 `resolve_monster_contact_attack` 致死后立即返回，不再写 contact/phase tick。
- `monster_ai_special.cpp` 的 bomber、charger/dasher 接触路径在致死后立即返回，不再写 contact、HP、phase 或调用 `defeat_monster`。

### 真实 producer 覆盖

- 近战、charger、bomber：实际 AI phase/tick 驱动到致死；检查 HP、reaction、AI phase、contact、AI tick、blink 和 monster defeated 事件不变。
- projectile：真实 shooter 投射物致死，锁定 owner `MonsterId` + `detail_id=0`。
- native hazard：真实 chaos hazard 致死，锁定 owner + `HazardKind::native`。
- burning ground：真实 affix fixed tick 生成 hazard 并致死。
- chain lightning：真实 direct hit 生成 chain hazard，延迟命中致死。
- death blast：真实 bomber 自爆/defeat 生成持久 hazard，延迟命中致死并保留已败 owner。
- abyss：真实环境 hazard 致死，monster 固定 `count`、detail 为 rule。
- invalid projectile owner：通过公开 `destroy_monster` 使 owner/投射物失效，验证无伤害、无 death snapshot、projectile 清零。
- 600 tick 冻结：先由真实 corrosion contact 建立状态，再由真实 thunderstorm 致死；随后验证玩家状态、怪物、projectile、monster hazard、environment hazard、输入诊断、历史和 snapshot 全部不变。
- 同 tick 双亡：真实玩家攻击击败最后一个 dummy 后，同 tick 真实 thunderstorm 致死；monster defeated event tick 与 death snapshot tick 相等。

### 测试注入与栈修复

- 删除本任务新增的 `set_player_status`、hazard owner 篡改和自定义 hazard 注入。
- 数值裁剪仍通过既有核心伤害入口测试；来源、即时冻结和长冻结均由真实 producer 致死。
- 移除 `/STACK:2097152`。首次移除后 trace 在既有多 `CombatWorld` 测试处异常结束；将 `dummy_reaction` launcher 三段和 `monster_melee` abyss-fury 五条路径拆为单 World 测试后，默认 Windows 1 MiB 栈完整通过，不再保留 linker workaround。

### 修复 GREEN 与最终验证

最终执行：

```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build --preset windows-msvc-debug --target arpg_combat_tests && out\build\windows-msvc-debug\bin\arpg_combat_tests.exe && ctest --preset windows-msvc-debug -R "^combat\.units$" --output-on-failure'
git diff --check
```

关键输出：

```text
203 cases, 0 failures
1/1 Test #3: combat.units ... Passed
100% tests passed, 0 tests failed out of 1
git diff --check: exit 0
```

### 审查修复文件

- `src/combat/combat_world.cpp`
- `src/combat/monster_ai.cpp`
- `src/combat/monster_ai_melee.cpp`
- `src/combat/monster_ai_special.cpp`
- `tests/combat/player_death_snapshot_tests.cpp`
- `tests/combat/combat_test_support.hpp`
- `tests/combat/dummy_reaction_tests.cpp`
- `tests/combat/monster_melee_tests.cpp`
- `tests/combat/combat_test_main.cpp`
- `tests/combat/CMakeLists.txt`

### 修复后疑虑

- 无功能性阻塞。
- 旧报告中“保留 2 MiB 测试栈”的疑虑已消除；当前默认栈完成 203 个 Combat 用例。

## 审查修复：来源 detail 域校验

### RED

先为两个来源分别加入 `MonsterId` 有效、`detail_id=0xFFFF` 的 canonical 测试，并将预期用例数从 203 更新为 205；未修改生产代码时执行：

```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build --preset windows-msvc-debug --target arpg_combat_tests && set ARPG_TEST_TRACE=1&& out\build\windows-msvc-debug\bin\arpg_combat_tests.exe'
```

关键输出：

```text
[FAIL] player_death_snapshot.invalid ground hazard detail:
actual.kind == PlayerDamageSourceKind::unknown
[FAIL] player_death_snapshot.invalid monster affix detail:
actual.kind == PlayerDamageSourceKind::unknown
205 cases, 2 failures
```

### GREEN

仅在 `canonical_player_damage_source()` 中补充 detail 域约束：ground hazard 限定为 `HazardKind::native` 至 `HazardKind::chaos_expansion`，monster affix 限定为小于 `MonsterAffixId::count`；monster 或 detail 任一无效均 canonicalize 为 `{unknown, MonsterId::count, 0}`。

最终执行：

```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build --preset windows-msvc-debug --target arpg_combat_tests && out\build\windows-msvc-debug\bin\arpg_combat_tests.exe && ctest --preset windows-msvc-debug -R "^combat\.units$" --output-on-failure'
git diff --check
```

关键输出：

```text
205 cases, 0 failures
1/1 Test #3: combat.units ... Passed
100% tests passed, 0 tests failed out of 1
git diff --check: exit 0
```
