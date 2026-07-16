# Stage 10 Task 6 报告：六条深渊战斗数值规则

## 状态

- 基线 HEAD：`e4f9ff2df9dbea147df64d32a00d793fd87dabec`
- 范围：只实现 combat 数值与 Dungeon config 接线；未实现环境 runtime、清房调用、奖励或 UI。
- 结果：六条规则已通过 `CombatEncounterConfig::abyss` 消费；CombatWorld 不读取 checkpoint、save、room seed 或奖励状态。

## TDD：RED 证据

1. 配置契约 RED：新增 `CombatEncounterConfig::abyss` 测试后，MSVC 明确报错 `CombatEncounterConfig` 没有成员 `abyss`。
2. 100% 行为基线 RED（145 cases, 5 failures）：
   - Heavy Steps：期望 `4.59`，实际仍为地面速度 `5.4`。
   - Swift Pursuit：期望移动位移 `1.15x`，实际仍为 `1.0x`。
   - Abyss Bulwark：期望 armor `1007`，实际仍为 Stage 9 的 `775`。
   - Abyss Fury：期望接触伤害 `65`，实际仍为 `45`。
   - Exhausted Recovery：期望初始 hp `700`，实际仍为 `1000`。
3. 资源 API RED：测试先引用 `restore_player_resources` 与 `clear_abyss_rule_preserving_resources`，编译明确因两个成员不存在而失败。
4. Dungeon exact-receipt RED：固定选中 `abyss_fury` 后，receipt 前缓存 config 的 `rule` 仍为 `none`。
5. 自审补强 RED：Fury 下 death-blast hazard 仍为 `120`，未达到 `174`。

## GREEN 与倍率组合顺序

统一顺序为：

1. 先完整求值 Stage 9 affix runtime profile。
2. 再顺序应用 Abyss basis points；不覆盖 Stage 9 结果。
3. 正整数资源/盾值需要上取整时使用统一 ceil helper；armor 与 damage 延续整数 floor。
4. tick 统一使用 ceil，非零 base 的结果至少为 1 tick。

统一基础 helper 位于 `combat_scaling.hpp`：`scale_basis_points`、`scale_ticks_ratio`、Stage 9 packet 缩放与 Abyss outgoing damage boundary；`monster_ai_common.hpp` 只保留 move/attack/cooldown 的 AI/profile 组合包装。三个 AI 文件不再各自复制 tick/packet 浮点逻辑。

## 六条规则精确证据

### Swift Pursuit

- chaos chaser 地面移动：`1.15x`。
- cooldown：`ceil(42 * 8500 / 10000) = 36`。
- 与 Stage 9 Swift M3 组合：Stage 9 cooldown `32`，再乘 Abyss 得 `ceil(32 * 0.85) = 28`。
- telegraph/recovery/active 不受 Swift 改动。

### Abyss Bulwark

- Armored M3：`floor(775 * 1.30) = 1007`；无 armor 时仍为 `0`。
- 基础 chaos chaser 实际 max hp `260`，额外盾 `ceil(260 * 0.30) = 78`；加入既有 fallback max shield 后为 `168`，current shield 增加 `78`。
- Mighty M3 先把实际 max hp 求为 `520`，再给额外盾 `156`，max/current 分别为 `246/156`。
- Shielding M2 的 Stage 9 max shield `91` 保留并叠加，得到 max/current `169/78`，没有把 Stage 9 shielding 免费激活或覆盖。

### Abyss Fury

- 接触：`floor(45 * 1.45) = 65`。
- projectile：生成时保留 Stage 9 packet `40`，direct-hit 最终边界得到 `floor(40 * 1.45) = 58`。
- native monster hazard：`floor(35 * 1.45) = 50`。
- death-blast affix hazard：`floor(120 * 1.45) = 174`；chain/burning 也在生成边界走同一函数。
- Stage 9 Frenzy M3 先算 `floor(45 * 1.5) = 67`，再算 `floor(67 * 1.45) = 97`。
- telegraph/active/recovery/cooldown：`7/4/9/21`（Frenzy M3 后再除 Fury 1.45）；active ticks 保持 `4`。
- damage 只乘一次：contact/projectile/bomber 在 direct-hit 最终边界乘；native/death/chain/burning hazard 因不走 direct-hit，在生成边界乘；environment 未接入该边界。

### Heavy Steps

- 仅非 airborne 的水平地面速度：`5.4 * 0.85 = 4.59`。
- jump impulse/弧线不变；空中横移仍为 `3.78`；逐 tick 比较攻击 ID/phase 完全相同。

### Exhausted Recovery

- 初次进入：hp `1000 -> 700`，barrier `101 -> 71`（ceil）。
- `reset_player_health=false` 跨 wave 保留受损后的当前 hp/barrier，不重复乘 `0.7`。
- 显式 restore amount：`1 -> 1`、`10 -> 7`；`reset_player_health=true` 先清零再走同一 restore API，恢复到 `700/71`。

### Life Sacrifice

- base max hp `1001` 经 ceil 得 `551`，进入时按同一 ratio mapping 保持满血比例。
- 退出前 `276/551`，调用 `clear_abyss_rule_preserving_resources()` 后为 `501/1001`（最近整数公式）。
- barrier `37/101` 退出后仍为 `37/101`，只 clamp，不 refill。
- clear API 只提供 Task 8 可测试接线点；本任务没有在清房时调用。

## Dungeon 接线与依赖

- `make_combat_encounter_config` 新增显式 `AbyssCombatConfig` 参数并原样缓存。
- normal room 显式传 `combat_config_for(none)`。
- abyss start 在 checkpoint selection 的 seed/depth/danger/rule/rules_version 全部匹配后，传 `combat_config_for(selection->rule)`；exact receipt 前只缓存，receipt 成功后才构造 CombatWorld。
- `arpg_combat` PUBLIC 链接 `arpg_abyss`；`arpg_abyss` 仍只 PRIVATE 链接 `arpg_core`，无反向 combat 依赖。

## 修改文件

- Combat public config/API/CMake：`combat_types.hpp`、`combat_world.hpp/.cpp`、`CMakeLists.txt`。
- 统一缩放与运行路径：`combat_scaling.hpp`、`monster_ai_common.hpp`、三个 monster AI、`monster_ai.cpp`、`monster_pool.hpp/.cpp`、`player_simulation.cpp`。
- Dungeon config/cached receipt 接线：`room_combat_template.hpp/.cpp`、`dungeon_session.cpp`。
- 测试：combat config/health/movement/melee/support/main，以及 Dungeon transaction/support。

## 最终验证

- `ctest --preset windows-msvc-debug -R "combat.units|dungeon.units" --output-on-failure`
  - `combat.units` passed，`dungeon.units` passed。
  - 2/2 tests，0 failures，fresh 总耗时 `143.32 sec`。
- `ctest --preset windows-msvc-debug -R '^abyss.units$|^architecture\.' --output-on-failure`
  - 19/19 tests，0 failures；包括 combat no-raylib/no-dungeon/no-persistence 与 persistence checkpoint-only。

## 疑虑与边界

- 无已知 Task 6 功能缺口。
- Task 7 的三条 environment runtime 仍未实现；当前 Fury boundary 不会误缩放未来 environment damage。
- Task 8 必须显式调用 `clear_abyss_rule_preserving_resources()`；本任务只提供并测试 API，没有提前接清房 lifecycle。

## 审查修复：Fury 最终伤害边界与 helper 下沉

### RED

- actual contact 使用 Blink Assault M1 把 Stage 9 contact packet 从 `45` 变为 `54`，再叠 Chilling M1：旧实现先 Fury 后 Chilling，结果为 `90`；期望先 `ceil(54 * 0.15) = 9` 合入 packet，再 final Fury，结果 `floor(63 * 1.45) = 91`。
- actual projectile 使用基础 lightning `40` 与 Chilling M1：旧顺序得到 `floor(40 * 1.45) + ceil(58 * 0.15) = 67`；期望先加入 `ceil(40 * 0.15) = 6`，再 final Fury 得 `floor(46 * 1.45) = 66`。
- Fury + Chaos Corrosion 三档旧实现仍写入原值 `20/30/45`；期望 final DoT `29/43/65`。
- RED 输出：`150 cases, 2 failures`，分别命中 contact `91` 断言与 Corrosion 三档断言。

### GREEN 与 exactly-once 审计

- `apply_monster_direct_hit` 先完整合入 Chilling 水伤，再对最终 packet 每个元素 floor 乘 `monster_damage_bp`；AI contact、projectile 与 bomber 不再提前乘 Fury。
- projectile runtime 中保存 Stage 9 packet，命中 direct-hit 后只乘一次；Chilling M1 测试锁定 packet `40`、最终玩家伤害 `66`。
- Corrosion 在最终写入 `PlayerStatusRuntime` 前 floor 乘一次：M1/M2/M3 为 `29/43/65`；相同测试同时锁定无 Fury 时仍为 `20/30/45`，排除无条件缩放与双乘。
- native monster hazard 以及 death/chain/burning affix hazard 不经过 direct-hit，继续只在生成边界乘 Fury；environment 仍未实现且未接入。
- GREEN 输出：`150 cases, 0 failures`。

### Minor Refactor

- 新增窄头 `src/combat/combat_scaling.hpp`，集中整数 basis-point、tick ratio、Stage 9 packet 与 outgoing packet 缩放。
- `monster_ai_common.hpp` 只保留 AI/profile 的 move/attack/cooldown 组合包装。
- `combat_world.cpp`、`monster_pool.cpp`、`player_simulation.cpp` 与 scaling 单测直接 include `combat_scaling.hpp`，不再 include AI common；没有复制 helper 实现。

### 审查修复后的 fresh 验证

- `ctest --preset windows-msvc-debug -R "combat.units|dungeon.units" --output-on-failure`
  - 2/2 tests，0 failures，总耗时 `143.81 sec`。
- `ctest --preset windows-msvc-debug -R '^abyss.units$|^architecture\.' --output-on-failure`
  - 19/19 tests，0 failures，总耗时 `53.55 sec`。
