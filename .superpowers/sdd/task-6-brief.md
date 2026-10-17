### Task 6: 将数字键 1～5 路由到当前装配槽

**Files:**
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/platform/raylib/host_input.hpp`
- Modify: `src/platform/raylib/host_input.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Create: `tests/dungeon/dungeon_skill_cast_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `tests/platform/active_skill_input_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

- [ ] **Step 1: 写输入和槽解析 RED 测试**

  用 `PhysicalKeySource` 依次模拟 `KEY_ONE`～`KEY_FIVE`，断言单帧 pressed 精确映射；held 不重复触发；同帧可表示多个按键但 host 按槽号升序只接受首个成功请求。地下城测试断言槽 1/2 解析为两技能，空槽返回 `none`，非稳定房间阶段拒绝，装配交换后按键立即跟随新槽位。

- [ ] **Step 2: 运行 RED**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_dungeon_tests
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|dungeon\.units)$' --output-on-failure
  ```
  Expected: `PhysicalKeySnapshot` 无数字键字段且会话无槽施放入口。

- [ ] **Step 3: 扩展平台输入快照**

  在两个结构中增加：
  ```cpp
  std::array<bool, skills::kActiveSkillSlotCount> active_skill_slots{};
  ```
  `sample_physical_keys` 仅在中央采样点读取 `KEY_ONE + index` 的 pressed 状态；`map_host_frame_input` 只复制该数组，禁止在 renderer/host 其他位置直接调用 `IsKeyPressed`。把技能按键计入 `input.keys.attack`，从而沿用暂停、失焦和死亡输入门禁。

- [ ] **Step 4: 增加槽位施放入口并接入主循环**

  `DungeonSession` 增加：
  ```cpp
  [[nodiscard]] combat::SkillCastResult request_active_skill_slot(
      std::uint8_t slot) noexcept;
  ```
  它验证槽号、phase、combat 存在，读取 stable loadout；空槽返回 `SkillCastResult::none`；非空才调用 `combat_->request_active_skill(id)`。`submit_frame_actions` 返回新结构：
  ```cpp
  struct SubmittedFrameActions final {
      std::array<bool, 3> combat{};
      std::array<combat::SkillCastResult,
          skills::kActiveSkillSlotCount> skills{};
  };
  ```
  Raylib host 每 presented frame 只 submit 一次，不在 fixed-step 子循环重复数字键 pressed。

- [ ] **Step 5: 运行 GREEN 和输入延迟守卫**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_dungeon_tests
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|dungeon\.units|platform\.input_latency_source|platform\.host_input_source)$' --output-on-failure
  ```
  Expected: 五键映射、交换后路由和既有直接输入毒化/低延迟守卫全部通过。

- [ ] **Step 6: 提交输入任务卡**

  ```powershell
  git add src/dungeon src/platform/raylib tests/dungeon tests/platform
  git commit -m "feat: route number keys to active skill slots"
  ```

---

