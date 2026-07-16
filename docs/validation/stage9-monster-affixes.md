# Stage 9 怪物词缀验收（Task 10 证据修复）

本页只记录可重跑证据。`src/persistence` 未修改；本 Task 不实现 Stage 10 内容。

## 1000 房：计划 Trace 与真实 Combat/奖励链

`dungeon_affix_stress.1000 room affix trace is deterministic` 保留 1000 房计划 Trace：两份同根种子 session 对比房间 seed、怪物 ID/位置、词缀 ID/等级/危险分、累计经验、掉落、领取位和物品摘要；每 37 房执行 V4 encode/decode 后重建 session。

此前它为每只怪直接注入 `CombatEvent::defeated`，不能证明实战链。现在同一 Trace 逐只调用真实 `CombatWorld::defeat_monster`，经 `DungeonSession::tick()` 的 `relay_combat_events()` 进入奖励逻辑；逐字段记录实际 defeat payload（kind/tick/attack/target/feedback/位置/monster/ordinal/危险分/资格）、经验、掉落及领取。真实掉落自动领取产生的 pending-save 也按正式 runtime 提交。守卫 `stage9.evidence.no_injected_defeat_trace` 会拒绝再次出现 `relay_all_defeats`、`relay_defeated` 或 fixture 的硬编码一致性输出。

```text
ARPG_STAGE9_AFFIX_STRESS_ONLY=1 ARPG_TEST_TRACE=1 build-release/bin/arpg_dungeon_tests.exe
[RUN] dungeon_affix_stress.1000 room affix trace is deterministic
[RUN] dungeon_affix_stress.saturated high risk affix world allocates nothing
2 cases, 0 failures
```

第二条是独立的高压 CombatWorld Trace：96 怪、384 弹体、96 区域与 MULTI-M3/BURN-M3/CHAIN-M3 连续 600 tick，断言零分配。它是极端池压力证据，不冒充 1000 房实战逐怪压力。

## 深 40 真实掉落、领取与 V4 fixture

命令（完整输出写入仓库证据文件）：

```powershell
build-release/bin/arpg_stage9_validation_fixture.exe |
  Tee-Object docs/validation/evidence/stage9/task10-fixture-full.log
```

以下是**摘录**；完整逐怪计划、词缀、真实 defeat payload、掉落、领取位和比较行见 [task10-fixture-full.log](evidence/stage9/task10-fixture-full.log)。fixture 从深40固定根种子选择真实有词缀且命中掉落的一例，调用 `CombatWorld::defeat_monster`、领取后再 V4 encode/decode 并重建 `DungeonSession`。只有 state、计划及词缀、掉落和领取位都比较成功时，最后一行才为 `consistent=1`。

```text
deep40 root=2 room_index=39 room_seed=2972536108632698802 depth=40
defeat kind=5 tick=2 target=2 monster=6 ordinal=2 score=17 reward_eligible=1
drop ordinal=2 item_id=12564799626640217584 item_level=46 rarity=0
claim ordinal=2 claimed_bits=4,0,0 item_count=1
v4 state_equal=1 plan_and_affixes_equal=1 drop_equal=1 claim_equal=1 consistent=1
```

## 正式玩法窗口与 Step 6

`arpg_stage9_formal_game_validation` 直接调用正式 `arpg::platform::run_raylib_host`（即 `arpg_game` 使用的公开 host/renderer），固定 seed 2，在**已提交的第 60 帧**自动退出和导出。它使用专用目录 `build-release/bin/stage9-formal-game-validation/`，不使用默认存档，也不修改正式地下城规则。`validation_exit_after_presented_frames` 默认为 0，仅该测试入口设置为 60。

截图 [03-formal-game-submitted-frame.png](evidence/stage9/03-formal-game-submitted-frame.png) 是正式游戏的已提交帧，不是静态 12 卡窗口；验收脚本检查 1280x720、至少 24 种采样色、200 个非背景采样点和 12 个亮采样点。本次实际值为 `colors=31 non_background=6339 bright=64`。旧 `02-formal-game-initial.png` 可能为白画布，**不作为内容证据**。

| Step 6 项目 | 结果 | 可重跑证据 |
| --- | --- | --- |
| 正式玩法渲染帧 | PASS | `stage9.formal_game.capture_after_present`；上方正式 host 截图及非白画布检查。 |
| 浅层 M1 | PASS（自动数据） | `monster_affix_generation.frozen depth bands` 冻结深度 1～3 的 M1-only 权重；正式 host 截图证明同一正式 renderer 可提交内容，但不把截图当成该概率的证明。 |
| 深40 M2/M3 | PASS（真实 session） | fixture 完整计划包含 M2/M3；见完整日志及 `stage9.validation_fixture.real_v4_reward_reload`。 |
| 多重投射、燃烧、连锁、闪现、死亡爆破预警 | PASS（自动 Combat） | `monster_affix_triggers.multishot fanned projectiles`、`burning ground periodic hazard`、`chain lightning direct-hit warning`、`blink warning clamp empower`、`death blast cleans owner transients and persists`。 |
| L 上挑强壮怪 | PASS（自动 Combat） | `monster_affix_runtime.mighty horizontal launch only`，验证只缩放水平击退而不改变上挑垂直轨迹。 |
| 高危死亡掉落 | PASS（真实 Combat→Dungeon） | fixture `defeat … score=17`、真实 `drop ordinal=2` 与物品字段；不是注入事件。 |
| 领取后的重载 | PASS（V4） | fixture 的 `state_equal/plan_and_affixes_equal/drop_equal/claim_equal=1`，以及 1000 房 Trace 每 37 房重建。 |
| 极端组合与满池 | PASS（自动 Combat） | `dungeon_affix_stress.saturated high risk affix world allocates nothing`；96/384/96 与三高危组合 600 tick。 |

测试专用固定 12 词缀窗口仍由 `stage9.validation_game.capture_after_present` 覆盖，只用于目录展示，不替代正式玩法证据。

## 本次最小验证集

```text
ctest --test-dir build-release -R "^stage9\\." --output-on-failure
# 4/4 passed: fixture、反注入守卫、固定展示窗、正式 host 捕获

ARPG_STAGE9_AFFIX_STRESS_ONLY=1 ARPG_TEST_TRACE=1 build-release/bin/arpg_dungeon_tests.exe
# 2 cases, 0 failures
```
