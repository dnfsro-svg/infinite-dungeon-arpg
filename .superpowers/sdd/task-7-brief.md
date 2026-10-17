## Task 7: 怪物独立掉落、地面池与自动拾取

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Create: `tests/dungeon/dungeon_loot_drop_tests.cpp`
- Modify: `tests/dungeon/dungeon_wave_tests.cpp`
- Modify: `tests/dungeon/dungeon_lifecycle_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`

- [ ] **Step 1: 写固定 trace 与领取事务失败测试**

用可控击杀事件覆盖 2×96 序号、`wave*96+target_index`、每怪仅一次 1% roll、无额外房间上限、192 池容量、固定位置与固定内容。覆盖 claimed 后重载不再掉、未 claimed 重杀同序号得到同物品、新房 bits 清零、离房未拾取永久消失。

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_dungeon_tests
ctest --test-dir build-release -R dungeon.units --output-on-failure
```

- [ ] **Step 3: 定义固定地面池**

```cpp
inline constexpr std::size_t kGroundDropCapacity = 192;
inline constexpr float kPickupRadius = 1.5F;

struct GroundItem final {
    bool active{};
    std::uint16_t drop_ordinal{};
    combat::Vec3 position{};
    ItemInstance item{};
};
```

`DungeonSession` 使用 `std::array<GroundItem, 192>`，不使用 vector；快照只复制 active 项的有界视图或整个固定数组。

- [ ] **Step 4: 从击杀事件派生一次掉落**

在 relay `CombatEventKind::defeated` 时读取当前 wave index 和 `target_index`；检查 0..191、claimed、已生成 ordinal。由 room seed + ordinal + drop 域调用 `DeterministicRng::derive_stream()` 创建独立 RNG，只有 `next_bounded(100).value()==0` 才生成物品；slot 六等权，ilvl=`min(100, depth)`。

- [ ] **Step 5: 实现拾取请求**

```cpp
[[nodiscard]] RequestResult request_pickup(std::uint16_t drop_ordinal) noexcept;
void request_nearby_pickups(combat::Vec3 player_position) noexcept;
```

一次只允许一个 pending save；距离使用 X/Y 平方距离 `<= 2.25F`。committed 后移除地面项并设置 claimed；not_committed 保留；indeterminate faulted。新房 committed 后清池并清 bits，旧房未拾取内容不迁移。

- [ ] **Step 6: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_dungeon_tests
ctest --test-dir build-release -R dungeon.units --output-on-failure
git diff --check
```

- [ ] **Step 7: 提交**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: add deterministic ground equipment drops"
```

### 冻结的集成裁决

- `CombatWorld::load_wave()` 会清空并按 wave spawn 顺序重建怪物，因此 defeated event 的 `target_index` 就是当前 wave-local `spawn_index`。只在 `wave_index_ < wave_count` 且 `target_index < current_wave.spawn_count` 时处理掉落；`drop_ordinal = wave_index_ * 96 + target_index`。
- `claimed_drop_bits` 只表示已稳定领取。另在 `DungeonSession` 保存 192 位 room-local `rolled_drop_bits_`，保证同一 session 中每个 ordinal 只投一次（包括未掉落结果）；构造/重置当前房时清空。重载的新 session 只跳过 claimed ordinal，未 claimed ordinal 重杀会按同一确定性内容再生成。
- 使用互相独立且命名的 RNG 域：chance、slot、item-content、item-id。chance 流恰好调用一次 `next_bounded(100)`；slot 流调用一次 `next_bounded(6)`；不能因新增内容消费 chance 流。内容 seed 由 `room_seed + ordinal` 派生；非零 item ID 由 `root_seed + room_index + ordinal + item-id domain` 派生，零固定映射为 1，不重骰。
- 新生成 ID 与稳定物品表或当前 active ground item 冲突时进入 `DungeonFault::item_id_collision`；合法参数下生成失败进入 `invalid_item_state`。不得换域/换种子规避冲突。
- `item_level = min(100, depth)`；有效房间 depth 必须至少为 1。地面位置使用 defeated event 的 X/Y，Z 强制为 0。
- `ground_items_` 是按 ordinal 直接索引的 `std::array<GroundItem, 192>`；snapshot 把 active 项按 ordinal 升序紧凑写入现有 `ground_items[0..ground_item_count)`，其余保持零值。不得把完整背包复制进 snapshot。
- 保留现有 `PendingSaveKind::loot_pickup` 脚手架，不重命名；`PendingSave` 保存 pickup ordinal（非 pickup 使用 `UINT16_MAX` sentinel）与原 `resume_phase`。loot pickup 不创建 Task 6 的 pending item-build cache；cache-kind 一致性规则保持 equipment/recipe 专属。
- `request_pickup()` 允许 Task 6 同一组 item request phases，拒绝 committing/transitioning/faulted 或已有 pending。它必须从当前 combat snapshot 取得真实玩家位置并再次检查 X/Y 距离；`request_nearby_pickups(position)` 只按 ordinal 升序扫描候选并在 accepted/faulted 后停止，一 tick 最多创建一个 pending。
- pickup accepted：在 `noexcept` 内复制 stable state 一次，追加完整 item，设置 claimed bit，checked 增加 commit generation，再 move 到 pending；任何 allocation failure 返回 rejected 且 ground/stable/phase/pending 全不变。ID 冲突或非法稳定状态 faulted。
- pickup committed：receipt 完整匹配后以无分配路径发布 stable state、移除对应 ground item、恢复 `resume_phase`；不重算 combat build。not_committed 保留 ground 并恢复 phase；indeterminate 使用既有 fault。必须防止错误 ordinal 或 ground 已消失的 committed receipt 被发布。
- 在每次正常 dungeon tick 的末尾、退出请求之前，用 session 的当前玩家位置调用 nearby pickup；若进入 committing，不得同 tick 再发门/洞请求。
- transition 的 pending next state 必须把 `claimed_drop_bits` 清零；只有 transition committed 后立即清 `ground_items_`/`rolled_drop_bits_`。not_committed 保留旧房地面池与 claimed；新房构造再次清瞬时池。reset current room 清瞬时池但保留当前 stable claimed bits。
- 可增加测试访问器/测试支持文件和 `dungeon_test_main.cpp` 注册联动；`dungeon_progression.cpp` 只有在新房 claimed 清零无法在 transition/session 层完成时才允许定点修改。不得改 persistence/platform/raylib。
- TDD 必须覆盖：chance 非命中也只处理一次、两波 ordinal 边界 0/95/96/191、192 active 容量、固定 item bytes/位置、重复 defeated、claimed reload、unclaimed reload、pickup 距离边界、committed/not_committed/indeterminate、pickup copy/push/validation OOM 原子性、ID collision、transition committed/not_committed 清理语义，以及 committed pickup 发布无分配。

---
