### Task 3: 通过地下城保存事务原子修改五个主动槽

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/CMakeLists.txt`
- Create: `tests/dungeon/dungeon_skill_loadout_transaction_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

- [ ] **Step 1: 写真实 SaveStore 成功/失败事务测试**

  测试默认快照、取出槽 1、装入槽 5、交换槽 2/5；每次先断言请求仅产生 pending save 且 stable snapshot 未变，再提交成功并断言一次性发布。注入现有保存失败结果，断言 stable state、owned bits 和五槽字节级不变；请求越界、重复石、战斗瞬态或已有 pending save 时拒绝。

- [ ] **Step 2: 运行 RED**

  ```powershell
  cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests
  $env:ARPG_STAGE17_SKILL_LOADOUT_ONLY='1'
  & out/build/windows-msvc-core-debug/bin/arpg_dungeon_tests.exe
  Remove-Item Env:ARPG_STAGE17_SKILL_LOADOUT_ONLY
  ```
  Expected: 缺少装配请求 API 或 pending kind 导致编译/断言失败。

- [ ] **Step 3: 增加快照、结果和会话入口**

  `DungeonSnapshot` 增加只读 `skills::SkillLoadoutState skill_loadout`。`PendingSaveKind` 增加 `skill_loadout`。公开 API 固定为：
  ```cpp
  [[nodiscard]] RequestResult request_remove_active_skill(
      std::uint8_t slot) noexcept;
  [[nodiscard]] RequestResult request_equip_active_skill(
      skills::ActiveSkillId skill, std::uint8_t slot) noexcept;
  [[nodiscard]] RequestResult request_swap_active_skill_slots(
      std::uint8_t left, std::uint8_t right) noexcept;
  ```
  槽编号在领域/会话内一律 0-based，只有 UI 文案显示 1-based。

- [ ] **Step 4: 复用候选状态保存链**

  每个请求：检查当前没有 pending save、房间不是 `committing_transition`/死亡保存/重建状态、复制 `stable_state_` 到 reusable candidate、在 candidate 上调用纯函数、整体 validate、以 `PendingSaveKind::skill_loadout` 调用现有 `prepare_item_save`。保存成功由 `commit_pending_save` 一次发布；失败只清 pending candidate，不写 stable。不得先改 `stable_state_` 再尝试回滚。

- [ ] **Step 5: 运行 GREEN 与装备/强化事务回归**

  ```powershell
  cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests
  ctest --test-dir out/build/windows-msvc-core-debug -R '^(dungeon\.units|stage16\.crafting_transaction\.units)$' --output-on-failure
  ```
  Expected: 新装配事务通过，Stage 16 装备/材料事务不变。

- [ ] **Step 6: 提交事务任务卡**

  ```powershell
  git add src/dungeon tests/dungeon
  git commit -m "feat: add atomic active skill loadout transactions"
  ```

---

