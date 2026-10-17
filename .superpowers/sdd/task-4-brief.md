### Task 4: 实现技能施放运行态和拔刀斩

**Files:**
- Create: `src/combat/active_skill_runtime.hpp`
- Create: `src/combat/active_skill_runtime.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/combat_snapshot.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Modify: `tests/combat/CMakeLists.txt`
- Create: `tests/combat/draw_slash_skill_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`

- [ ] **Step 1: 写拔刀斩 RED 测试**

  覆盖：接受有效请求；同技能冷却拒绝；攻击/硬直/死亡/另一技能中拒绝；前摇期间无伤害；命中帧前方宽区域内三个目标各命中一次；背后目标不命中；持续 tick 不重复；命中产生物理伤害和水平击退；冷却只减少不下溢；reset/load_wave 清施放和冷却。

- [ ] **Step 2: 运行 RED**

  ```powershell
  cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests
  $env:ARPG_STAGE17_DRAW_SLASH_ONLY='1'
  & out/build/windows-msvc-core-debug/bin/arpg_combat_tests.exe
  Remove-Item Env:ARPG_STAGE17_DRAW_SLASH_ONLY
  ```
  Expected: `request_active_skill` 和技能快照尚不存在。

- [ ] **Step 3: 加入固定容量运行态与公开结果**

  `combat_types.hpp` 增加：
  ```cpp
  enum class SkillCastResult : std::uint8_t {
      accepted, none, invalid_skill, cooling_down,
      player_unavailable, basic_attack_active, skill_active
  };
  enum class ActiveSkillPhase : std::uint8_t {
      none, startup, strikes, finisher, recovery
  };
  struct ActiveSkillSnapshot final {
      skills::ActiveSkillId id{skills::ActiveSkillId::none};
      ActiveSkillPhase phase{ActiveSkillPhase::none};
      std::uint16_t elapsed_ticks{};
      Vec3 locked_center{};
      std::uint8_t strike_index{};
  };
  ```
  `CombatSnapshot` 再含 `ActiveSkillSnapshot active_skill` 和 `std::array<std::uint16_t, skills::kActiveSkillCount> skill_cooldowns`。`CombatWorld` 公开：
  ```cpp
  [[nodiscard]] SkillCastResult request_active_skill(
      skills::ActiveSkillId skill) noexcept;
  ```

- [ ] **Step 4: 实现拔刀斩目录常量和状态机**

  在 `active_skill_runtime.hpp` 固定：
  ```cpp
  inline constexpr std::uint16_t kDrawSlashStartupTicks = 10U;
  inline constexpr std::uint16_t kDrawSlashRecoveryTicks = 14U;
  inline constexpr int kDrawSlashBasePhysical = 220;
  inline constexpr int kDrawSlashBreakDamage = 36;
  inline constexpr float kDrawSlashRange = 5.0F;
  inline constexpr float kDrawSlashHalfWidthAtEnd = 3.2F;
  inline constexpr float kDrawSlashKnockbackSpeed = 0.22F;
  ```
  请求成功时锁定 facing 并立刻启动 240 tick 冷却。第 10 tick 以“朝向轴向距离 `0..range` 且横向绝对距离不大于按距离线性展开的半宽”判定扇形；使用现有 `build_player_hit_packet`、护盾/HP/破韧和 `ImpactKind::medium_hitstun` 管线。运行态持有 `std::array<bool, kMonsterCapacity> hit_latch`，每次施放清零，保证单目标一次。

- [ ] **Step 5: 让基本攻击和技能互斥但不污染连段**

  技能活动时不消费 J/L 攻击动作；普攻活动时技能请求返回 `basic_attack_active`。移动输入仍按现有规则更新，但技能锁定 facing，技能结束恢复 idle/move。K 跳跃不修改；技能期间的跳跃请求只留在既有 buffer 并按原过期规则处理，不增设空中限制。

- [ ] **Step 6: 运行 GREEN、普攻和零分配回归**

  ```powershell
  cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests
  ctest --test-dir out/build/windows-msvc-core-debug -R '^(combat\.units|architecture\.combat_no_persistence)$' --output-on-failure
  ```
  Expected: 拔刀斩测试、原普攻/上挑/跳跃测试和模块边界全部通过。

- [ ] **Step 7: 提交拔刀斩任务卡**

  ```powershell
  git add src/combat tests/combat
  git commit -m "feat: implement draw slash active skill"
  ```

---

