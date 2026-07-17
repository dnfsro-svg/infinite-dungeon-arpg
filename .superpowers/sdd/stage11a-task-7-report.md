# Stage 11-A Task 7 实现报告

## 范围

- 基线：`763ff5e412dcd32e6e2811257ffcf9b6f7496b10`。
- 仅将正式深渊死亡合并进唯一 `death_retreat`；未实现 `death_continue`、死亡 UI 或 Task 8+。
- 测试通过现有正式 `abyss_start` pending、committed receipt 和 `CombatWorld::apply_player_damage` 致死路径推进；未直接写 death snapshot、稳定 death 或 pending save。

## RED

先新增“正式深渊启动事务提交后，Combat 死亡只产生一个原子 death_retreat”用例，并在 MSVC DevShell 中真实运行。生产实现未改时明确失败：

```text
[FAIL] dungeon_death_lifecycle.abyss death prepares one atomic retreat:
pending->kind == PendingSaveKind::death_retreat
(tests/dungeon/dungeon_death_lifecycle_tests.cpp:461)
```

实际仍为 Stage 10 的 `PendingSaveKind::abyss_fail`，证明测试命中了待替代路径。

## 实现

- 抽取无状态 `apply_abyss_failure_resolution(next, previous)`，从正式 started challenge 验证 room seed/rule/danger/rules version，并规范写入 failed lifecycle 与完整 `LastAbyssResolution`。
- 深渊死亡不再调用 `prepare_abyss_failure()`；`prepare_death_retreat()` 在同一个局部 next 中一次写入 generation、death sequence、死亡摘要、确定性退层目标、普通房锚点、failed abyss 与 failed resolution。
- failed resolution 保留死亡前 started challenge 的 seed/rule/total，固定 generated=0、claimed=0、abandoned=total，并清零所有 reward masks/revision。
- `pending_death_cache_consistent()` 从冻结 Combat death snapshot 与稳定 started abyss 重建完整 exact next，覆盖 deep abyss failed state 和 resolution。
- `not_committed` 保留 started abyss 的冻结 Combat 与稳定状态，retry 的 expected generation、完整 next 和 death snapshot 不变；committed 后才清 Combat、ground、pending rewards、rolled bits 和 room experience。
- 手动 `abyss_fail` 复用同一纯投影，同时保留原有单独 `abyss_fail` 事务、not_committed fault 和同房重建行为。
- 启动期 started-save 修复未改；合法 pending death 的 lifecycle 已是 failed，不会进入旧 started 修复。

## 测试覆盖

- 单一原子 next：`death_was_abyss`、死亡房 anchor、target、failed lifecycle、完整 failed resolution、stable 提交前不变。
- not_committed：冻结 Combat、hp=0、started abyss stable 不变、retry 完整 next/death snapshot 相同。
- committed + 重建 Session：进入 `death_pending`，无 Combat/ground/pending reward/room exp，重建后同一 DeathCheckpoint 且无二次 pending。
- receipt 防御：started lifecycle、resolution seed、death_was_abyss、current_room abyss 四类篡改全部 `save_receipt_mismatch` fail closed。
- Stage 10 旧深渊死亡断言更新为正式 `death_retreat`，手动失败、清理、奖励和放弃路径仍保留原覆盖。

## 验证

- `dungeon.units`: 1/1 passed，247 cases，520.12s。
- `stage10.validation_fixture.real_abyss_transactions`: passed。
- `persistence.units`: passed。
- `platform.units`: passed。
- `architecture.*`: 18/18 passed，95.42s。
- `git diff --check`: clean。
- `rg -n '/STACK' CMakeLists.txt cmake src tests docs .superpowers`: 无匹配。

## 已知疑虑

- 工程没有公开命名为 `request_abyss_start` 的 API；正式路径由 available abyss checkpoint 构造 Session 后自动生成 `abyss_start` pending。测试完整提交该生产事务后才造成 Combat 死亡，没有伪造 started state。
- public API 无法篡改内部 death cache；因此 cache exact 重建由代码路径和逐字段相同 retry 覆盖，receipt 篡改通过公开 API 覆盖，未增加测试专用生产接口或私有 pending/death 注入。
