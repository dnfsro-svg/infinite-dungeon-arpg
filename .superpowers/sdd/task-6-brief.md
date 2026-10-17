### Task 6: 让 DungeonRuntime 服务所有 pending save 并覆盖 A/B 槽恢复

**Files:**
- Modify: `src/platform/raylib/dungeon_runtime.hpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes: Task 5 的 `pending_save()` 和 `resolve_pending_save()`，现有 `SaveStore::commit()`。
- Produces: `DungeonRuntime::service_pending_save()`；host 和所有 runtime 测试不再假定只有 transition 会写存档。

- [ ] **Step 1: 写 Runtime 事务 RED 测试**

在现有 runtime helper 基础上补入：

```cpp
arpg::test::Failure committed_passive_save_survives_runtime_restart() noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory));
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(*runtime.session()));
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(8U));
    runtime.service_pending_save();
    const auto saved = runtime.session()->snapshot();
    ARPG_REQUIRE(saved.passive_tree.allocated_bits == ((1ULL << 0U) | (1ULL << 8U)));
    platform::DungeonRuntime resumed(config_for(directory, 999U));
    ARPG_REQUIRE(resumed.initialize());
    ARPG_REQUIRE(resumed.session()->snapshot().passive_tree.allocated_bits
        == saved.passive_tree.allocated_bits);
    return {};
}
```

增加 before_publish 返回 not_committed 时保持旧位图、after_publish indeterminate 时进入 fault、passive pending 时 Door/Descent 均被拒绝三例。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Expected: 编译失败，因为 `service_pending_save()` 与新 Session 接口未定义。

- [ ] **Step 3: 泛化 Runtime 的存档服务**

把 `service_pending_transition()` 重命名为 `service_pending_save()`，并将 `to_session_result()` 返回类型改为 `dungeon::PendingSaveResult`；它从 `session_->pending_save()` 取得下一状态，调用一次 `store_.commit()`，再把转换后的收据交给 `resolve_pending_save()`。保存指示器在开始时为 `saving`，committed 为 `saved`，not_committed 为 `error`，indeterminate 后 runtime 状态为 `faulted`。不要在 host/runtime 中重算点数、位图或 generation。

```cpp
void DungeonRuntime::service_pending_save() noexcept {
    if (state() != DungeonRuntimeState::running || !session_.has_value()) return;
    const auto pending = session_->pending_save();
    if (!pending.has_value()) return;
    status_.indicator = SaveIndicator::saving;
    const persistence::SaveCommitResult result = store_.commit(pending->next_state);
    sync_commit_status(result);
    session_->resolve_pending_save(to_session_result(result));
    if (session_->snapshot().phase == dungeon::RoomPhase::faulted) {
        state_ = DungeonRuntimeState::faulted;
    }
}
```

- [ ] **Step 4: 验证 Runtime 与存档集成**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "platform.view_math|persistence.units" --output-on-failure
git diff --check
```

Expected: 平台 runtime suite 与 persistence 双槽 fault suite 全部通过；重复调用 `service_pending_save()` 在无 pending 时无副作用。

- [ ] **Step 5: 提交 Task 6**

```powershell
git add src/platform/raylib/dungeon_runtime.* tests/platform/dungeon_runtime_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "feat: persist passive mutations through runtime"
```

