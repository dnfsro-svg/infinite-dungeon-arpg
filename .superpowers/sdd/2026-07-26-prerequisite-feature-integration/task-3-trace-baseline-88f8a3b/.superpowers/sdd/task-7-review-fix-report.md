# Task 7 review fix report

基线：`c43e02d`。

## 修复范围

- `DungeonSession::relay_combat_events()` 仅在 `defeated.reward_eligible` 为真时派生普通玩家掉落和怪物经验；事件仍照常转发。
- `CombatWorld::tick()` 对已 defeated 的遭遇怪物跳过词缀资源、主动词缀和 AI。legacy dummy 仅保留原有的命中停顿与复活倒计时。
- 死亡爆破创建失败时，验证非死亡 owner hazard 被清理、没有 death blast/死亡预警，且 defeated 事件仍存在。

## TDD 证据

- combat RED：`defeated monster remains static` 在基线 42 tick 后发现 blink warning，证明 defeated runtime 仍执行主动词缀。
- dungeon RED：临时撤回奖励门禁后，完整 dungeon suite 在 `reward-ineligible defeats do not reward player` 失败于 `after.ground_item_count == 0U`，证明自爆会错误掉落。

## 验证

- release combat build 通过。
- `arpg_combat_tests.exe`：131 cases, 0 failures。
- `arpg_dungeon_tests.exe`：123 cases, 0 failures（恢复奖励门禁后）。
- `git diff --check` 通过。
