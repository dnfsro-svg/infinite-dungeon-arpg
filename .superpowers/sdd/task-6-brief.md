## Task 6: Dungeon 背包、换装与三合一原子事务

### Frozen dungeon transaction decisions

- 新增：

```cpp
enum class RequestResult : std::uint8_t { accepted, rejected, faulted };
```

公开 item request 返回该 enum；`accepted` 仅表示已构造唯一 pending checkpoint，尚未发布。参数/等级/材料不合法、已有 pending、分配失败返回 rejected 且状态不变；稳定 ID collision、稳定状态/build 不合法或 sequence/commit generation 溢出进入 faulted。
- item 请求允许在有当前房间和 combat 的 `locked/combat/wave_delay/cleared/awaiting_exit` 阶段；`committing/transitioning/faulted` 拒绝。接受后保存原 `RoomPhase resume_phase` 到 `PendingSave` 并进入 committing。not_committed 恢复 resume_phase；committed 的 equipment/recipe 也恢复 resume_phase；indeterminate 进入 faulted。既有 transition committed→transitioning、passive 恢复 awaiting_exit 语义保持。
- `PendingSave` 增加 resume phase；所有 transition/passive 构造点显式设置。`DungeonSnapshot` 增加 `inventory_count:uint32`（仅未装备物品）、`equipped_ids[6]`、`optional<PendingSaveKind> pending_save_kind`。禁止把 item vector 放进 snapshot。
- 为后续固定地面视图建立零状态类型但本任务不生成掉落：`kGroundDropCapacity=192`，snapshot 含 `ground_item_count:uint16` 与固定 `GroundItemSnapshot[192]`；snapshot 只存 ordinal/position/item_id/slot/rarity，不复制 `ItemInstance`。Task 6 全部为零，Task 7 负责填充。
- `request_equip(id)`：ID 必须存在、item/ownership 合法、required_level <= `room_progression_.level`，按 base slot 替换该槽 ID；已在同槽装备、错槽/悬空/重复引用或无状态变化拒绝。被替换旧装备仍留在唯一 item vector。
- `request_unequip(slot)`：slot 合法且当前非零才接受，只清引用，不删除 item。
- `request_recipe(ids)`：三 ID 非零唯一、都存在且合法、都未装备、同 slot/rarity；产物使用 Task 3 `generate_recipe_item(root_seed,next_item_sequence,...)`。survivors 保持原获得顺序，删除三材料后把产物追加末尾；sequence checked +1。产物 ID 与完整当前 item 表任一 ID 冲突时进入新增 `DungeonFault::item_id_collision`，sequence/commit overflow 使用明确 fault；分配失败 rejected，不消费材料/sequence。
- 为非法加载/组合 build 增加 `DungeonFault::invalid_item_state`；分配失败不是 fault。允许修改 `src/dungeon/dungeon_rules.hpp` 加上述 fault enum。
- 所有 accepted request 在 `noexcept` 内仅复制一次 stable `DungeonRunState`，catch `std::bad_alloc`；构造完成后以 move 放入 pending。任何异常不得逃逸。stable item state 在 committed 前不改变，`item_state()` 始终返回 stable const reference。
- `same_run_state()` 必须从本任务起按获得顺序比较完整 item records（含 reserved/rolls）、equipment、claimed bits、sequence；错误 verified item state 必须触发 receipt mismatch。使用无分配、noexcept 字段比较，不得排序。
- `build_for` 用固定 `std::array<Modifier,256>`：先原子 append passives，再复制 `project_equipment.modifiers[0..count)`；只调用一次 `evaluate_player_modifiers`，再设置 weapon/local fields。以 `combat::build_player_hit_packet(0,build).has_value()` 作为现有公开完整 build validation gateway；失败返回 nullopt，不复制背包。
- `construct_current_room` 必须使用 `build_for(stable_state_)`，并把 room seed 通过命名固定域派生为 `CombatEncounterConfig::evasion_seed`。equipment committed 后立即 `combat_->apply_player_build`；recipe 因材料不得装备，仍重新 build/apply 以锁定统一发布路径。passive mutation继续保持 Stage 7 语义：当前房不热更新，下一房由 construct 应用。
- committed equipment/recipe 更新 `room_progression_` 与 stable progression的一致性但不得重置 combat；当前 hp/barrier 由 Task 4 apply 只向下 clamp、不回复。not_committed 不应用 build。无 combat 的非法构造进入 invalid_item_state fault。
- `pending_save()` 的既有按值查询暂保留给 runtime；本任务不得在普通 tick/snapshot 调用它。Task 8 将把 runtime 保存服务收敛为无额外大 vector 复制的 view 路径。

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/room_combat_template.hpp`
- Modify: `src/dungeon/room_combat_template.cpp`
- Create: `tests/dungeon/dungeon_item_transaction_tests.cpp`
- Modify: `tests/dungeon/dungeon_passive_tree_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`

- [ ] **Step 1: 写事务失败测试**

覆盖装备等级不足、错槽、不存在 ID、重复装备、卸下；committed 才发布 build；not_committed 保持旧状态/build；indeterminate faulted。生命/护盾不回复。三合一覆盖三 ID 唯一、未装备、同 slot/rarity、材料删除+产物追加+sequence 增加原子性和失败重试等价。

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_dungeon_tests
ctest --test-dir build-release -R dungeon.units --output-on-failure
```

- [ ] **Step 3: 增加请求和稳定查询接口**

```cpp
enum class PendingSaveKind : std::uint8_t {
    transition, passive_tree, loot_pickup, equipment, recipe
};

[[nodiscard]] RequestResult request_equip(std::uint64_t item_id) noexcept;
[[nodiscard]] RequestResult request_unequip(items::ItemSlot slot) noexcept;
[[nodiscard]] RequestResult request_recipe(
    const std::array<std::uint64_t, 3>& item_ids) noexcept;
[[nodiscard]] const items::ItemOwnershipState& item_state() const noexcept;
```

`DungeonSnapshot` 只增加 `inventory_count`、6 个 equipped IDs、ground drop 固定快照和 pending kind；禁止每帧复制 vector。

- [ ] **Step 4: 实现唯一组合 build 路径**

增加私有函数：

```cpp
[[nodiscard]] std::optional<combat::PlayerCombatBuild> build_for(
    const checkpoint::DungeonRunState& state) const noexcept;
```

它把 `append_passive_modifiers` 与 `project_equipment` 合并进 256 固定缓冲并只 evaluate 一次。房间加载和 equipment/recipe committed 后都使用该函数；无 combat 的阶段只更新稳定状态。

- [ ] **Step 5: 实现 pending checkpoint 事务**

所有 request 先拒绝 faulted、pending、非法等级/引用/材料，再复制一次稳定状态构造 `PendingSave`；捕获分配失败并返回 rejected。只有 `on_save_committed()` 交换稳定状态和应用 build；not_committed 丢弃 next state；indeterminate 调用既有 faulted 路径。

- [ ] **Step 6: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_dungeon_tests
ctest --test-dir build-release -R "dungeon.units|combat.units|items.units" --output-on-failure
git diff --check
```

- [ ] **Step 7: 提交**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: add atomic equipment and recipe transactions"
```

---
