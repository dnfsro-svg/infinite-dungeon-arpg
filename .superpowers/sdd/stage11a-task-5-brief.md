# Stage 11-A Task 5：安全加载 pending death 与公开只读快照

## 基线与范围

- 基线：`a0b624fa8f8a7a90613278b47536bc4f76dd49df`。
- 主文件：`src/dungeon/dungeon_types.hpp`、`dungeon_session.hpp/.cpp`、`dungeon_snapshot.cpp`、`dungeon_progression.cpp`、新 `tests/dungeon/dungeon_death_lifecycle_tests.cpp` 及 suite/CMake 注册。
- 规格遗漏的最小前置：在稳定 `checkpoint::TransitionKind` 新增 `death_retreat`，并只做 Persistence enum 接受/round-trip 回归；因此允许最小修改 `src/dungeon/dungeon_checkpoint.hpp`、`src/persistence/checkpoint_codec.cpp` 和对应 persistence test。不得实现 PendingSaveKind::death_retreat、事务、继续请求或 UI。
- 不修改 Combat 行为，不提前实现 Task 6+。

## 公共状态

- `RoomPhase` 新增 `death_pending`，保持 `faulted` 为最终故障状态。
- 新增：

```cpp
struct DeathSnapshot final {
    checkpoint::DeathCheckpoint checkpoint{};
    bool saving{};
    bool can_continue{};
};
```

- `DungeonSnapshot` 增加 `std::optional<DeathSnapshot> death`。
- 已提交 pending death：phase=`death_pending`，death 存在、`saving=false`、`can_continue=true`，内容只来自稳定 checkpoint。
- 准备保存死亡时（以后 Task6 的 pending next state）：投影 `pending_save.next_state.death`，`saving=true`、`can_continue=false`。不得从 Combat 临时对象拼装 UI 数据。
- death lifecycle none：snapshot.death 为空且现有房间构造行为不变。

## 构造顺序

- `construct_current_room()` 在 encounter/abyss/Combat/装备 build 之前首先分流 death lifecycle。
- pending 时先验证稳定状态；合法则清空所有 transient：Combat、ground pool、rolled bits、encounter/wave、pending room exp、确认态和临时 pending caches；不生成 encounter/Combat，不发 room-enter/combat-start 事件，进入 `death_pending`。
- 非法 pending 进入明确 fault，且不留下 Combat/ground/可玩 phase。

## Session 级校验

新增内部 `validate_death_checkpoint_for_session(state, rules)`（命名可等价），至少完成：

1. 调用 header-only structural validator，并要求 lifecycle pending。
2. `commit_generation` 必须可安全减 1，`death_sequence>0`；使用死亡锚点房、`commit_generation-1`、`death_sequence` 作为 next sequence 调用 Task3 deterministic validator。不得使用已递增代数直接重抽。
3. 稳定 current_room 是死亡房历史锚点：depth/floor/ecology 与 death 摘要一致；seed/index/entry/hole 保留；稳定 `current_room.is_abyss=false`。仅在校验摘要时用局部 copy 将 `is_abyss` 恢复为 `death_was_abyss`。
4. `biases` 全零、`last_transition=death_retreat`、`last_direction=none`。
5. target 必须与 Task3 重新生成逐字段一致，且 depth=`max(1, death_depth-1)`、floor0、initial、zero-bias generation、non-abyss；目标不重骰。
6. 来源目录：monster id、attack/projectile canonical detail、hazard kind、monster affix id、abyss rule id、unknown sentinel/detail 均由拥有目录的 Dungeon 层验证。
7. 普通死亡允许合法普通历史状态，但不能伪造仍在进行的 abyss；深渊死亡必须是已原子失败的 abyss checkpoint + 合法 failed LastAbyssResolution，room seed/rule/total/count 关系与死亡锚点一致且无奖励发布。不能把普通死亡伪造成深渊失败或反之。
8. `same_run_state()` 必须比较 death_sequence 与 DeathCheckpoint 每个字段；若 Task3 已实现，增加回归证明，不重复实现。

## 输入拒绝矩阵

在 `death_pending`：

- `queue_action` false；tick/movement 不推进游戏状态或生成 Combat。
- descent/door/hole/reset/passive allocate/refund 全拒绝。
- pickup/nearby pickup/equip/unequip/recipe 全拒绝，且不创建 pending save、不改稳定物品/星盘/进度。
- 无 Combat、ground count=0、has_active_room=false、exits closed、无 pending transition。

## TDD 与测试

先写 RED 并真实运行，证明缺少 `death_pending`/snapshot/validation。测试至少覆盖：

- 合法普通 pending load；合法第1层夹紧；合法深渊 failed pending load；none 正常构造。
- 逐项 mutation：target regenerated mismatch（seed/ecology/hole/index）、depth、entry、floor、is_abyss、death sequence zero、generation underflow、last transition、last direction、biases、稳定 current room abyss 标记、death summary anchor mismatch。
- 每种 source kind 的合法/非法目录边界，unknown canonical。
- abyss lifecycle/resolution mismatch、普通/深渊标记互相伪造。
- 全输入拒绝矩阵与完整 DeathSnapshot 字段。
- TransitionKind::death_retreat 经 V6 encode/decode 接受，旧版本回归不变。

## 验证与交付

- build `arpg_dungeon_tests` 与 `arpg_persistence_tests`。
- 直跑 Dungeon（默认 1 MiB 栈）和 Persistence；CTest `dungeon.units|persistence.units`。
- 跑 `architecture.persistence_checkpoint_only`、`architecture.dungeon_no_persistence`、`architecture.persistence_no_raylib`。
- `git diff --check`、范围审计。
- 报告：`.superpowers/sdd/stage11a-task-5-report.md`（excluded unique name）。
- 提交建议：`feat: load pending death sessions safely`。

