## Task 4: 实现一次机会生命周期与原子启动/失败门禁

**Files:**

- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/dungeon/dungeon_transaction_tests.cpp`
- Modify: `tests/dungeon/dungeon_lifecycle_tests.cpp`
- Modify: `tests/combat/player_health_tests.cpp`
- Modify: `tests/persistence/dungeon_save_integration_tests.cpp`

**Produces:** `available/started/failed` 状态机、进入前预计算、死亡/R/异常退出失败、故障矩阵。

**Consumes:** V5 `AbyssCheckpoint` 和现有 `PendingSave`/`SaveStore::commit`。

- [ ] 扩展 `PendingSaveKind`：

```cpp
enum class PendingSaveKind : std::uint8_t {
    transition,
    passive_tree,
    loot_pickup,
    equipment,
    recipe,
    abyss_start,
    abyss_fail,
    abyss_clear,
    abyss_reward_materialized,
    abyss_reward_claim,
    abyss_abandon,
};
```

同时在 `DungeonFault` 增加 `invalid_abyss_state`、`abyss_generation_failed`、`abyss_reward_collision`、`abyss_reward_revision_overflow`；这些故障都必须进入 faulted，不能降级为普通房或重投随机数。

- [ ] 增加 18 个事务测试，逐项覆盖：普通门原行为、深渊门目标先以 `available` 完成普通房间切换、available 房自动排队 `started`、started 提交前无战斗副作用、not_committed/indeterminate 均 fault、receipt mismatch fault、成功后才构造战斗、R 先存 failed、玩家死亡先存 failed、失败后同 seed 普通房、load-started 自动提交 failed、load-available 自动启动、自动失败提交失败 fault、initial/descent 旧非法标记需要迁移提交。

- [ ] 先运行测试，预期深渊门仍走普通 `transition`、R 立即重建房间，新增断言失败。

- [ ] `prepare_transition` 仍先用现有 transition 事务提交目标房；若门预告为深渊，其 `next_state.current_room.is_abyss=true` 且 lifecycle 为 `available`。transition receipt 成功后进入目标房，但不生成怪物、不应用规则。

- [ ] `construct_current_room` 看到 `available` 时先完整预计算并验证 `AbyssSelection`、强化遭遇和奖励 profile；全部有效后创建 `PendingSave{kind=abyss_start}`，把同一房 lifecycle 改为 `started`。started receipt 成功后才封门、应用规则、加载怪物并进入 combat；失败调用现有 `enter_fault`。这样严格保持 `available → started` 两个稳定检查点。

- [ ] 给战斗增加 `CombatEventKind::player_defeated`：生命可降到 0，只发一次事件。普通房收到该事件执行现有普通重置；`started` 深渊房创建 `abyss_fail` 保存请求，保存成功后清除 `is_abyss`、将 lifecycle 置 `failed`，再调用同一个普通重置内部函数。

- [ ] 将 `reset_current_room()` 改为返回 `RequestResult`。普通房仍立即重置；started 深渊房只排队 `abyss_fail`，成功 receipt 前保持 `committing`，杜绝 R 绕过一次机会。

- [ ] `DungeonRuntime::initialize` 载入 `started` 后不构造挑战，先把同 seed 状态转为 failed 并调用 `SaveStore::commit`；只有成功发布后创建普通房 session。载入 V5 `available` 时由 session 自动执行 started 门禁。若 `SaveLoadResult::migrated`，先调用 dungeon migration helper 并原子提交合法 V5 结果；提交成功后才创建 session，即使结果为 available 也不能把迁移提交和 started 提交合并。迁移发布或自动失败发布失败时 runtime 为 `faulted`。

- [ ] 运行故障矩阵：

```powershell
ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
```

预期：每个 `SaveFaultPoint` 下都只有“旧稳定状态”或“新稳定状态”，不存在挑战已开始但存档仍 available 的可玩状态。

- [ ] 提交：

```powershell
git add src/dungeon src/combat src/platform/raylib/dungeon_runtime.cpp tests/dungeon tests/persistence
git commit -m "feat: gate abyss attempts behind atomic saves"
```

---

