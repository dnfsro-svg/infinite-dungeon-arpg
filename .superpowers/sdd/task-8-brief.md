## Task 8: Runtime 保存服务、重启一致性与故障门控

**Files:**
- Modify: `src/platform/raylib/dungeon_runtime.hpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Modify: `tests/persistence/dungeon_save_integration_tests.cpp`
- Modify: `tests/persistence/save_store_fault_tests.cpp`

- [ ] **Step 1: 写 runtime 故障矩阵失败测试**

对 pickup/equip/unequip/recipe 各验证 committed、not_committed、indeterminate；pending 时拒绝新拾取、换装、合成、门、洞。重启后比较 item bytes、equipment、claimed、sequence 和角色 build。

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_platform_tests arpg_persistence_tests
ctest --test-dir build-release -R "platform.units|persistence" --output-on-failure
```

- [ ] **Step 3: 泛化保存服务但保持单事务**

`DungeonRuntime::service_pending_save()` 继续只处理一个 `PendingSave`，不按 kind 分叉编码；把 `SaveStore::commit()` 结果精确映射到 session 的 committed/not_committed/indeterminate。所有物品请求都通过 runtime 的窄接口转发，raylib host 不直接改 session state。

- [ ] **Step 4: 验证大背包不会进入帧快照**

构造 65,535 件合法最小物品状态，调用 snapshot 多次并通过 allocation probe 断言无 vector 复制/分配；UI 使用稳定 `item_state()` const 引用，只在背包打开且 stable generation 改变时重建可见索引。

- [ ] **Step 5: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_platform_tests arpg_persistence_tests
ctest --test-dir build-release -R "platform.units|persistence" --output-on-failure
git diff --check
```

- [ ] **Step 6: 提交**

```powershell
git add src/platform/raylib/dungeon_runtime.* tests/platform/dungeon_runtime_tests.cpp tests/persistence
git commit -m "feat: integrate equipment save transactions"
```

### 冻结的集成裁决

- 在 `DungeonSession` 增加 `[[nodiscard]] const PendingSave* pending_save_view() const noexcept`；它只返回内部 pending 的临时只读 view，无 pending 时为 `nullptr`，不得分配/复制。现有按值 `pending_save()` 暂留给旧测试兼容，但 `DungeonRuntime` 和新增大背包测试禁止调用它。允许定点修改 `src/dungeon/dungeon_session.hpp/.cpp` 及相关机械测试联动。
- `DungeonRuntime::service_pending_save()` 每次只读取一次 `pending_save_view()`，对所有 `PendingSaveKind` 调用同一 `SaveStore::commit(pending->next_state)`，不得按 kind 分叉编码或提交。store 调用期间 view 有效；resolve 后不得再解引用。
- `to_session_result`/service 必须在同步 render status 后 move `SaveCommitResult::verified_state` 到一个局部 `PendingSaveResult`，再 resolve；不得额外复制动态 item vector。committed/not_committed/indeterminate 必须一一映射，不把任何失败猜成 committed。
- `service_pending_transition()` 暂保留为只调用 generic service 的兼容 wrapper；本任务不修改 raylib host 的非物品输入路径。
- Runtime 新增窄物品接口：`request_pickup(uint16_t)`、`request_equip(uint64_t)`、`request_unequip(ItemSlot)`、`request_recipe(array<uint64_t,3>)`，返回 `RequestResult`；只有 runtime state 为 running 且 session 存在时委托 session，否则返回 rejected。接口不得直接修改存档、stable state 或 combat build。
- Runtime 提供只读 `const ItemOwnershipState* item_state() const noexcept`（无 session 返回 nullptr），供 Task9 UI 使用稳定引用。Task9 负责背包打开与可见索引缓存；本任务只冻结读取接口和 generation 来自现有 snapshot `commit_generation`，不提前实现 UI。
- pending save 存在时，所有新 pickup/equip/unequip/recipe 通过 runtime 返回 rejected；session 原有 committing phase 继续阻止门、洞、reset 和排队攻击。indeterminate 后 runtime/session 都 faulted，所有窄请求拒绝，generic service 不再二次提交。
- Runtime fault matrix 至少覆盖四类请求 × 三种 disposition。committed 发布并显示 saved；not_committed 保留 stable/ground/equipment/materials/phase 且 runtime 仍 running、indicator error；indeterminate runtime faulted，重启后以磁盘实际 winner 为准，不允许当前 faulted session 继续请求。
- pickup 测试可复用 `tests/dungeon/dungeon_test_support.hpp` 的 friend 注入/真实 relay；platform tests 已能 include 该支持文件。equip/unequip/recipe 用合法 catalog items，不伪造非法字节。
- 单事务门控测试必须在一个 accepted pending 期间尝试另外三类物品请求、descent、门移动、reset、queue_action，并证明 generation/stable/ground/materials 未变；随后分别 resolve disposition。
- 重启一致性必须逐字段/逐 byte 比较：item acquisition order 与完整 40-byte records（不能只比 ID）、6 equipment IDs、3 claimed words、next_item_sequence、commit_generation；并比较重建 session 的 `PlayerCombatBuild`（modifier values、weapon physical、local attack speed）与保存前 committed build。
- persistence 集成测试保持 `persistence -> checkpoint` 边界，不依赖 platform/raylib；用真实 `SaveStore` + `DungeonSession` 验证 committed/not_committed/indeterminate 后重启 winner。platform runtime 测试验证窄接口、状态与 indicator。
- 65,535 item 测试：在计数前完成合法 state/vector 与 session 构造；随后反复调用 `snapshot()`、`item_state()`、`pending_save_view()`（无 pending）并断言 allocation count 不变、snapshot 只含 inventory_count/equipped IDs/ground fixed view。再创建一个大状态 pending 时，单独断言 `pending_save_view()` 本身零分配；不要在注入/计数区调用旧按值 `pending_save()`。
- 如果 65,535 合法 item 测试运行时间过长，可只做一次完整 ownership/session 构造和多次只读调用，不得降低 item_count。不要把整个 item vector 放入 `DungeonSnapshot` 或 runtime render status。
- 允许修改 `tests/platform/CMakeLists.txt` 或测试 main 的机械注册，以及为 allocation probe 链接所需的最小测试构建文件；不得修改 raylib host/renderer/UI（Task9），不得改变 V4 ABI（Task5 已冻结）。
- TDD/mutation 证据至少证明：runtime 若改回按值 pending view 会被大背包 allocation test 杀死；verified_state 若复制而不是 move 会被分配计数/大状态测试杀死；kind-specific service 分叉、错误 disposition 映射、pending 门控遗漏、重启 item bytes/build 漂移均有失败测试。

---
