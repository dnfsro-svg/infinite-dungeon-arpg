# Stage 18 性能基础优化设计

## 目标

在不改变玩法、输入、存档格式和渲染快照格式的前提下，减少固定 tick 与高频请求中的大对象构造，并建立可重复的存档提交耗时测量方法。本阶段只处理已确认的热路径，不展开全项目重构。

## 已选方案

采用“轻量只读查询 + 独立基准工具”。`CombatWorld` 直接暴露角色位置和存活怪物数；`DungeonSession` 直接暴露当前房间阶段。固定 tick、掉落拾取和 `DungeonRuntime::state()` 使用这些查询，不再为了读取一个字段而构造 `CombatSnapshot` 或 `DungeonSnapshot`。

没有选择以下方案：

- 快照缓存或双缓冲：能够进一步减少渲染快照成本，但会引入失效时机、额外常驻内存和状态同步风险，超出本阶段需要。
- 立即异步保存：会改变事务调度和生命周期。先用真实双槽原子提交路径测量，再决定是否值得承担线程与退出同步复杂度。

## 接口与数据流

`CombatWorld` 新增：

```cpp
[[nodiscard]] Vec3 player_position() const noexcept;
[[nodiscard]] std::size_t living_monster_count() const noexcept;
```

`player_position()` 直接返回 `player_.position`。`living_monster_count()` 扫描固定容量怪物池，只计入 `active && hp > 0` 的对象，语义与原 `DungeonSession::remaining_targets()` 完全一致；它不会构造或复制包含怪物、投射物、危害和兼容假人的完整快照。

`DungeonSession` 新增：

```cpp
[[nodiscard]] RoomPhase phase() const noexcept;
```

具体替换：

- `DungeonSession::tick()` 使用一次 `player_position()`，服务出口、深渊确认与自动拾取。
- `request_pickup()` 和 `request_material_pickup()` 使用 `player_position()` 做距离判定。
- `remaining_targets()` 使用 `living_monster_count()`。
- `DungeonRuntime::state()` 使用 `DungeonSession::phase()`。
- `DungeonSession::snapshot()` 仍只在渲染、回执捕获和公开状态读取时构造完整快照；其字段与序列化边界不变。

## 存档耗时测量

新增一个不进入默认 CTest 的 `arpg_save_commit_probe` 工具，使用生产 `SaveStore`、生产初始地下城状态和独立构建目录执行预热与多次双槽原子提交，输出样本数、最小值、中位数、P95、最大值和平均值（毫秒）。它只测量，不设置易受机器负载影响的硬阈值。

决策规则：

- 若本机 Release 的 P95 明显低于一个 60 FPS 帧预算（16.67 ms），本阶段不引入异步保存。
- 若 P95 接近或超过 16.67 ms，再单独设计后台保存工作线程；不得在本阶段临时改变事务语义。

## 错误与兼容性

- 所有新查询均为 `noexcept`，不分配内存。
- 存档基准遇到无效初始状态、目录错误或提交失败时返回非零退出码并打印失败阶段。
- 不修改 V8 存档布局、双槽恢复、提交校验、掉落判定、怪物死亡时序或 raylib 6.0 集成。
- 不删除兼容别名，不拆分 `DungeonSession`、`raylib_host.cpp` 或 codec。

## 验证

按 TDD 增加行为测试，先让新接口因不存在而编译失败，再实现最小代码：

- 战斗测试验证初始/移动后的角色位置查询与公开快照一致。
- 战斗测试验证存活怪物查询排除生命为零的已击败对象。
- 地下城测试验证阶段查询覆盖进入战斗与故障状态。
- 现有地下城、平台、存档、Stage 16/17 和架构测试保持通过。
- 静态审计确认目标热路径不再调用完整快照。
- Debug 与 Release 均构建；Release 运行存档基准并据实记录异步化决策。

## 非目标

本阶段不做全局快照缓存、异步存档、存档格式迁移、`DungeonSession` 拆分、raylib host 测试代码搬迁、背包索引重构或全量 CMake 测试整理。
