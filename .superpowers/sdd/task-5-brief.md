### Task 5: 实现 DungeonSession 的清房加点/退款事务和下一房构筑投影

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/room_combat_template.hpp`
- Modify: `src/dungeon/room_combat_template.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Create: `tests/dungeon/dungeon_passive_tree_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/dungeon/dungeon_test_support.hpp`

**Interfaces:**
- Consumes: Task 2 allocation APIs，Task 3 `PlayerCombatBuild`，Task 4 的稳定 checkpoint 状态。
- Produces: `request_passive_allocation()`、`request_passive_refund()`、`pending_save()`、`resolve_pending_save()` 和包含星盘位图的 `DungeonSnapshot`；platform 之后只通过这些接口请求事务。

- [ ] **Step 1: 写阶段门控和保存收据的 RED 测试**

在 `dungeon_passive_tree_tests.cpp` 先写入完整的有点数 Session helper，再写测试：

```cpp
dungeon::DungeonSession session_with_passive_points(std::uint8_t points) noexcept {
    auto built = dungeon::make_initial_run_state(0x51515151ULL, dungeon::DungeonRules{});
    built.state.progression = {static_cast<std::uint8_t>(points + 1U), 0U, points, points};
    return dungeon::DungeonSession(dungeon::DungeonRules{}, built.state);
}

bool clear_to_awaiting_exit(dungeon::DungeonSession& session) noexcept {
    arpg::test::EventSummary events{};
    if (!arpg::test::drive_until_cleared(session, events)) return false;
    if (session.snapshot().phase == dungeon::RoomPhase::cleared) session.tick({});
    return session.snapshot().phase == dungeon::RoomPhase::awaiting_exit;
}
```

随后写入：

```cpp
arpg::test::Failure passive_mutation_is_only_available_after_clear() noexcept {
    dungeon::DungeonSession session = session_with_passive_points(4U);
    ARPG_REQUIRE(!session.request_passive_allocation(8U));
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    ARPG_REQUIRE(session.request_passive_allocation(8U));
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == dungeon::PendingSaveKind::passive_tree);
    ARPG_REQUIRE(!session.request_descent(true));
    return {};
}

arpg::test::Failure not_committed_passive_mutation_keeps_old_state() noexcept {
    dungeon::DungeonSession session = session_with_passive_points(4U);
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    ARPG_REQUIRE(session.request_passive_allocation(8U));
    const auto pending = *session.pending_save();
    session.resolve_pending_save({dungeon::SaveDisposition::not_committed,
        pending.next_state.commit_generation, pending.next_state});
    ARPG_REQUIRE(session.snapshot().passive_tree.allocated_bits == 1ULL);
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::awaiting_exit);
    return {};
}
```

再加 committed 后位图/未分配点更新、indeterminate 进入 fault、断链退款拒绝、下一房的 `combat.player` 具有已保存的 barrier/抗性/附加元素构筑，以及旧 `pending_transition()` 只在 transition 时返回值的测试。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
out\build\windows-msvc-debug\bin\arpg_dungeon_tests.exe
```

Expected: 编译失败，因为星盘命令、`PendingSaveKind`、`pending_save()` 和 snapshot 字段尚不存在。

- [ ] **Step 3: 用统一 pending save 建立事务边界**

在 `dungeon_types.hpp` 增加：

```cpp
enum class PendingSaveKind : std::uint8_t { transition, passive_tree };
struct PendingSave final {
    PendingSaveKind kind{PendingSaveKind::transition};
    std::uint64_t expected_generation{};
    DungeonRunState next_state{};
    TransitionKind transition{TransitionKind::none};
    ExitDirection direction{ExitDirection::none};
};
struct PendingSaveResult final {
    SaveDisposition disposition{SaveDisposition::indeterminate};
    std::uint64_t generation{};
    DungeonRunState verified_state{};
};
```

`DungeonSession` 用单一 `std::optional<PendingSave> pending_save_` 取代只服务门的 `pending_`，并把旧 `TransitionSaveResult` 统一改名为 `PendingSaveResult`（修改现有 dungeon、platform 和测试的调用点）。保留 `pending_transition()` 兼容包装：只有 `kind == transition` 时返回 `PendingTransition`；新 `pending_save()` 供 runtime 使用。

`request_passive_allocation(id)` 与 `request_passive_refund(id)` 必须仅在 `awaiting_exit`、没有 pending、拥有 combat 时复制 `stable_state_`，调用 Task 2，更新副本的 progression 和 passive tree，安全递增 generation，放入 `PendingSaveKind::passive_tree`。在收到 committed 且 `same_run_state(verified, next)` 前不能改变 `stable_state_`。not_committed 清 pending、停在 `awaiting_exit` 并递增现有保存失败诊断；indeterminate/收据不匹配进入 fault。

房门与下坠改为创建 `PendingSaveKind::transition`；它们在任何 pending 时拒绝。`construct_current_room()` 调用 `passives::evaluate_passive_tree(stable_state_.passive_tree)`，把结果转换为 `CombatEncounterConfig::player_build`。`DungeonSnapshot` 添加只读 `PassiveTreeState passive_tree`、`bool passive_save_pending` 和最近 `PassiveTreeError`。

- [ ] **Step 4: 验证 dungeon 生命周期**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "dungeon.units|architecture.dungeon" --output-on-failure
git diff --check
```

Expected: dungeon suite 既有门/下坠黄金回放保持通过，新增星盘事务案例通过，dungeon 不可达 persistence 的边界仍通过。

- [ ] **Step 5: 提交 Task 5**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: transact passive tree changes between rooms"
```

