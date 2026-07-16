# Task 8 Report: 危险分经验与直接装备掉落

## 实现

- 新增纯函数：`affix_drop_chance_bp`、`affix_item_level`、`affix_experience`。
- 击杀奖励只使用事件中的 `monster_id`、`spawn_ordinal`、`affix_score`、`reward_eligible`；不再读取死亡后的 combat snapshot。
- 分数为 0 时保持 Stage 8 的 `next_bounded(100) == 0` 掉落 trace；正分数使用既有掉落 chance 域的 `next_bounded(10000) < chance_bp`。
- 掉落序号直接采用事件序号，并以领取位防止重复事件、已领取事件和未命中事件重复奖励。
- 经验按危险分缩放并用 64 位 checked/saturating 算术累加；生成物品等级按危险分公式封顶 100。

## TDD 证据

- RED：新增 reward 测试后，`arpg_dungeon_tests` 编译失败，原因是三个 `affix_*` 接口尚不存在。
- GREEN：新增 3 个 dungeon reward 测试后，测试主程序的硬编码总数由 123 维护为 126。

## 验证

使用 VS 2022 x64 开发环境和 `WindowsSDKVersion=10.0.26100.0`：

```powershell
cmake --build build-release --target arpg_dungeon_tests arpg_persistence_tests
ctest --test-dir build-release -R "dungeon.units|persistence" --output-on-failure
git diff --check
```

结果：`dungeon.units` 126 cases, 0 failures（137.38 秒）；`persistence.units` 48 cases, 0 failures；匹配的 persistence 架构边界测试全部通过；`git diff --check` 无输出。
