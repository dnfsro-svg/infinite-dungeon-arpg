### Task 5: 实现极·鬼剑术（暴风式）多段剑阵和终结

**Files:**
- Modify: `src/combat/active_skill_runtime.hpp`
- Modify: `src/combat/active_skill_runtime.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_snapshot.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Create: `tests/combat/storm_swords_skill_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`

- [ ] **Step 1: 写暴风式 RED 测试**

  覆盖：开始时中心固定在角色朝向前方；玩家随后移动/转向不改变中心；12 段每 6 tick 触发；同一段每目标一次、不同段可再次命中；普通段为物理伤害和轻牵制；终结段独立高伤并击飞/击退；死亡、reset、load_wave 取消剩余段且不留下伤害；1800 tick 冷却；快照 strike index/phase 准确。

- [ ] **Step 2: 运行 RED**

  ```powershell
  cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests
  $env:ARPG_STAGE17_STORM_SWORDS_ONLY='1'
  & out/build/windows-msvc-core-debug/bin/arpg_combat_tests.exe
  Remove-Item Env:ARPG_STAGE17_STORM_SWORDS_ONLY
  ```
  Expected: 暴风式只被目录识别但没有 strikes/finisher 行为，断言失败。

- [ ] **Step 3: 冻结暴风式节奏和范围常量**

  ```cpp
  inline constexpr std::uint16_t kStormStartupTicks = 24U;
  inline constexpr std::uint8_t kStormStrikeCount = 12U;
  inline constexpr std::uint16_t kStormStrikeIntervalTicks = 6U;
  inline constexpr std::uint16_t kStormRecoveryTicks = 24U;
  inline constexpr float kStormCenterForward = 3.5F;
  inline constexpr float kStormStrikeRadius = 2.8F;
  inline constexpr float kStormFinisherRadius = 3.8F;
  inline constexpr int kStormStrikeBasePhysical = 42;
  inline constexpr int kStormFinisherBasePhysical = 360;
  inline constexpr float kStormFinisherKnockbackSpeed = 0.26F;
  inline constexpr float kStormFinisherLaunchSpeed = 0.24F;
  ```
  终结在第 12 段后的下一个 6 tick 边界发生，总施放时序由这些常量计算，不写第二套总帧数。

- [ ] **Step 4: 实现每段命中闩锁与终结复用管线**

  每次进入新段清空固定 `hit_latch`，在该 tick 以锁定中心半径查询所有活动怪物；普通段用 `ImpactKind::light_hitstun`，终结用现有 `ImpactKind::launch` 和浮空/击退参数。所有伤害走 `build_player_hit_packet`，不能直接减 HP。事件携带技能 ID、strike index 和 finisher 标记，供渲染/音频消费；若现有 `CombatEvent` 只有 `AttackId`，新增独立字段 `ActiveSkillId skill`，保留 `attack=none`。

- [ ] **Step 5: 运行 GREEN 与确定性复跑**

  ```powershell
  cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests
  ctest --test-dir out/build/windows-msvc-core-debug -R '^combat\.units$' --repeat until-fail:3 --output-on-failure
  ```
  Expected: 三次一致通过；暴风式无随机目标选择、无运行时分配。

- [ ] **Step 6: 提交暴风式任务卡**

  ```powershell
  git add src/combat tests/combat
  git commit -m "feat: implement storm swords awakening skill"
  ```

---

