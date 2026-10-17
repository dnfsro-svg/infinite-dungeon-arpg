# Stage 11-A Task 5 实现报告

## 范围

- 安全加载 `pending_continue`，在 encounter/abyss/Combat/build 前分流。
- 公开只读 `DeathSnapshot` 与 `death_pending` phase。
- `death_pending` 全 gameplay 输入拒绝与冻结。
- 稳定 `TransitionKind::death_retreat` 的 V6-only codec 支持。
- V6 在 `death_retreat + direction none` 条件下支持 current floor 0；V1-V5 不放宽。
- 未实现 `PendingSaveKind::death_retreat`、死亡事务、继续请求或 UI。

## TDD 证据

### 初始 RED

- Dungeon 编译失败：缺少 `TransitionKind::death_retreat`、`RoomPhase::death_pending`、`DungeonSnapshot::death`。
- Persistence 编译失败：缺少 `TransitionKind::death_retreat`。

### Anchor entry RED

- 新增非法 current-room entry mutation 后，`sequence generation and anchor mutations fail closed` 失败。
- 最小修复：Session pending validator 增加 `EntrySide` 上界校验。

### Floor 0 codec RED

- `v6 pending death floor zero round trips` 在 `encoded.has_value()` 失败。
- `v6 continued target floor zero round trips` 在 `encoded.has_value()` 失败。
- 最小修复：仅 V6 且 `last_transition=death_retreat`、`last_direction=none` 时允许 current floor 0。

## 实现摘要

- `construct_current_room()` 首先识别 pending death，清空 Combat、ground、rolled bits、encounter/wave、pending caches、确认态和房间临时进度。
- Session 校验使用 `commit_generation - 1`、当前 `death_sequence` 和局部恢复 `is_abyss` 的死亡锚点重生成 target。
- 校验 stable anchor、零 biases、transition/direction、六类来源目录、普通/深渊 failed 关系和 LastAbyssResolution。
- 合法 pending load 不构造 Combat、不发 room-enter/combat-start，snapshot 只复制 stable checkpoint。
- snapshot 为 future pending-next death 预留 `saving=true/can_continue=false` 投影，不读取 Combat 临时死亡对象。
- `death_pending` 下 action、移动/tick、descent、reset、星盘和全部物品请求 fail closed。

## 验证

- Debug build：`arpg_dungeon_tests`、`arpg_persistence_tests` 成功。
- 最新直跑：Dungeon `232 cases, 0 failures`。
- 最新直跑：Persistence `79 cases, 0 failures`。
- Focused CTest：5/5 通过：
  - `dungeon.units` 182.81 秒
  - `persistence.units` 0.29 秒
  - `architecture.persistence_no_raylib`
  - `architecture.dungeon_no_persistence`
  - `architecture.persistence_checkpoint_only`
- `git diff --check`：exit 0。
- 默认 1 MiB 测试栈保持，无 `/STACK` 兜底。

## 备注

- `same_run_state()` 的 death_sequence 与 DeathCheckpoint 全字段比较来自 Task3，Task5 mutation/快照测试复用并回归。
- 无私有测试注入，无 Task6+ 事务或 UI 提前实现。

## 复审修复：首代数伪造 pending death

- 独立 RED 构造 `commit_generation=1`、`death_sequence=1`，并从 pre-generation 0 生成逐字段匹配的退层 target；修复前 Session 错误进入 `death_pending`。
- 最小生产修复把 pending death 的代数门槛从 `generation != 0` 收紧为 `generation >= 2`，避免不存在的 generation 0 被当作已提交前态。
- 最新完整直跑：Dungeon `233 cases, 0 failures`，Persistence `79 cases, 0 failures`。
- 最新 focused CTest：5/5 通过；`dungeon.units` 182.09 秒，Persistence 与三项 architecture 全过。
