# Stage 18 性能基础优化验证

## 范围

本阶段只减少固定 tick 和高频状态查询中的完整快照构造，并增加可选的生产存档提交基准。未修改玩法、输入、V8 存档格式、双槽恢复、公开渲染快照、raylib 6.0 集成或事务时序。

## 基线

在提交 `8dada68` 创建独立工作树后运行：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
```

结果：构建 345/345；CTest 102/102 通过，0 失败，总测试时间 1049.33 秒。其中 `dungeon.units` 为 198.57 秒。测试自动改写的 9 个历史证据文件在确认来源后恢复到工作树基线，没有进入提交。

## 改动

- `CombatWorld::player_position()` 直接读取角色位置。
- `CombatWorld::living_monster_count()` 扫描固定怪物池，只计 `active && hp > 0`。
- `DungeonSession::phase()` 直接读取房间阶段。
- 固定 tick、显式物品/材料拾取、剩余目标统计和 `DungeonRuntime::state()` 不再为单个字段构造完整快照。
- 公开地下城快照仍在 `dungeon_snapshot.cpp` 构造一次完整战斗快照，供渲染和公开状态消费者使用。

静态审计：

```powershell
rg -n "combat_->snapshot\(\)\.player\.position|const combat::CombatSnapshot state = combat_->snapshot\(\)|session_->snapshot\(\)\.phase" src/dungeon src/platform/raylib/dungeon_runtime.cpp
```

结果：0 个匹配。目标范围内剩余的 `combat_->snapshot()` 只有 `src/dungeon/dungeon_snapshot.cpp` 的公开快照构造。

## TDD 证据

- 战斗 RED：`combat_query_tests.cpp` 先编译失败，MSVC C2039 明确报告 `player_position` 与 `living_monster_count` 不存在。
- 战斗 GREEN：`combat.units` 输出 `230 cases, 0 failures`。
- 地下城 RED：排除一次裸 CMake 缺 SDK 环境和一次测试枚举拼写后，MSVC C2039 只报告 `DungeonSession::phase` 不存在。
- 地下城 GREEN：`dungeon.units` 与 `platform.units` 2/2 通过，0 失败；Debug `dungeon.units` 为 124.24 秒。

基线与修改后的 Debug 地下城耗时不是严格隔离的微基准，只作为重复大快照移除后的辅助观测，不据此承诺固定百分比提升。

## Release 存档基准

性能工具默认关闭；本地测量时显式启用：

```powershell
cmake -S . -B out/build/windows-msvc-release -DARPG_BUILD_PERFORMANCE_PROBES=ON
cmake --build out/build/windows-msvc-release --target arpg_save_commit_probe
.\out\build\windows-msvc-release\bin\arpg_save_commit_probe.exe 101
```

工具使用生产 `SaveStore`、生产初始地下城状态和可执行文件旁的唯一目录，先预热一次，再测量 101 次双槽原子提交。结果：

| 指标 | 毫秒 |
| --- | ---: |
| min | 0.305200 |
| median | 0.392500 |
| P95 | 0.719900 |
| max | 1.140100 |
| average | 0.434012 |

边界参数 `0` 与 `10001` 均打印用法并返回退出码 2。

P95 仅占 16.67 ms 帧预算约 4.3%，最大值约 6.8%。本阶段结论：不引入异步保存；现有本机证据不足以证明线程、退出同步和事务排队复杂度值得增加。若未来真实玩家存档显著增大或慢盘采样超过帧预算，再单独设计。

## 最终验证

Debug：

- `combat.units`、`dungeon.units`、`persistence.units`、`platform.units`、Stage16 真实 raylib：5/5 通过，0 失败，总计 131.69 秒。
- Stage17 技能石真实 raylib、证据校验与反向自检：3/3 通过，0 失败，总计 11.20 秒。

Release：

- 完整构建 278/278 成功。构建日志保留一个原有测试警告：`pause_menu_view_tests.cpp:353` 的 MSVC C4127；本阶段没有新增警告。
- `combat.units`、`dungeon.units`、`persistence.units`、`platform.units`、Stage16 真实 raylib：5/5 通过，0 失败，总计 54.41 秒；`dungeon.units` 为 51.77 秒。
- Stage17 技能石真实 raylib、证据校验与反向自检：3/3 通过，0 失败，总计 7.53 秒。

提交后完成最终复验：完整 Release CTest 为 102/102 通过、0 失败，总计
718.95 秒；`git diff --check` 无错误；目标快照调用审计仍为 0 个匹配。
再次运行 101 次 Release 基准得到 P95 0.651600 ms、最大 0.937600 ms、
平均 0.434437 ms，异步保存决策不变。
