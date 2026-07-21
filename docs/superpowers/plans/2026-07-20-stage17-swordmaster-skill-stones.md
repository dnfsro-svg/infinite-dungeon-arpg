# Stage 17 Swordmaster Skill Stones Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 增加五个可自由取出、装入和交换的主动技能石槽，并交付可由 `1`、`2` 实际施放的“拔刀斩”和“极·鬼剑术（暴风式）”，同时保留每槽五个只读空辅助槽。

**Architecture:** 新建无平台依赖的 `arpg_skills` 领域库，持有技能 ID、目录、五槽装配规则和默认状态；`DungeonRunState` 持久化装配并通过现有候选状态保存事务发布；`CombatWorld` 只接收明确技能 ID 并持有房间瞬态施放/冷却；Raylib 层仅完成数字键映射、HUD/暂停页投影和渲染。辅助槽只进入领域状态、存档和只读 UI，不进入战斗计算。

**Tech Stack:** C++17、raylib 6.0、CMake 3.25、MSVC 19.44、Windows SDK 10.0.26100.0、CTest、现有固定容量/零异常生产路径。

## Global Constraints

- 以 `docs/superpowers/specs/2026-07-20-stage17-swordmaster-skill-stones-design.md` 为唯一功能基线。
- 不改 J/K/L/WASD 语义，不把技能塞进既有普攻 `AttackId` 连段，不增加辅助技能石效果、掉落或编辑。
- 不使用 DNF 美术、音频或文本资产；只使用当前程序化几何、现有音频路由和两个已批准的技能中文名。
- 新档和 V1～V7 迁移统一为：槽 1 拔刀斩、槽 2 暴风式、槽 3～5 空；两块主动石均归角色所有；全部 25 个辅助位为空。
- 已装备主动石仍属于角色所有权集合；五个槽仅保存引用。同一主动石不能同时出现在两个槽，不能复制或销毁。
- 装配变更必须候选态校验、保存成功后一次性发布；保存失败时所有权、槽位和 UI 选择状态都保持原值。
- 冷却、施放帧、命中去重和剑阵中心是房间瞬态，禁止写入检查点；死亡、重建房间或房间转换立即清除。
- 所有新增数组固定容量，游戏循环与渲染热路径不得新增堆分配；公开快照只传值，不让 UI 持有运行态指针。
- 每张任务卡按 RED→GREEN→回归→提交执行；不得把多个任务卡揉成一个大提交。
- 当前完整图形测试存在早于 Stage 17 的 Stage 9 栈溢出基线异常；Stage 17 必须用自己的标签测试、真实 Raylib 证据和非图形完整回归区分新回归与既有异常。

---

## File responsibility map

| Area | Files | Responsibility |
|---|---|---|
| 技能领域 | `src/skills/active_skill_types.hpp`, `active_skill_catalog.hpp/.cpp`, `skill_loadout.hpp/.cpp` | ID、目录常量、默认装配、所有权、取出/装入/交换和验证 |
| 存档 | `src/dungeon/dungeon_checkpoint.hpp`, `src/persistence/checkpoint_codec.hpp/.cpp` | V8 编解码、V1～V7 默认迁移、非法状态拒绝 |
| 地下城事务 | `src/dungeon/dungeon_types.hpp`, `dungeon_session.hpp/.cpp`, `dungeon_transition.cpp`, `dungeon_snapshot.cpp` | 装配候选态、保存提交/回滚、槽位施放入口和只读快照 |
| 战斗 | `src/combat/combat_types.hpp`, `combat_world.hpp/.cpp`, `active_skill_runtime.hpp/.cpp`, `combat_snapshot.cpp` | 房间冷却、施放状态机、两技能命中与取消 |
| 输入 | `src/platform/raylib/host_input.hpp/.cpp` | 单帧采样 `1`～`5`，转为槽位请求 |
| HUD/暂停页 | `src/platform/raylib/active_skill_view.hpp/.cpp`, `active_skill_renderer.hpp/.cpp`, `inventory_view_math.*`, `inventory_renderer.*`, `raylib_host.cpp` | 五槽 HUD、冷却遮罩、暂停装卸交互、25 个空辅助位 |
| 验收 | `tests/skills`, `tests/combat`, `tests/dungeon`, `tests/persistence`, `tests/platform`, `docs/validation` | 单元、事务、真实 Raylib 证据和交付记录 |

---

### Task 1: 建立技能石领域、目录与五槽不变量

**Files:**
- Create: `src/skills/CMakeLists.txt`
- Create: `src/skills/active_skill_types.hpp`
- Create: `src/skills/active_skill_catalog.hpp`
- Create: `src/skills/active_skill_catalog.cpp`
- Create: `src/skills/skill_loadout.hpp`
- Create: `src/skills/skill_loadout.cpp`
- Create: `tests/skills/CMakeLists.txt`
- Create: `tests/skills/skills_test_main.cpp`
- Create: `tests/skills/skill_loadout_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 先注册独立测试目标并写失败测试**

  在根 `CMakeLists.txt` 中把 `add_subdirectory(src/skills)` 放在 `src/combat` 之前，把 `add_subdirectory(tests/skills)` 放在 `tests/combat` 之前。建立 `arpg_skills_tests`，测试：默认装配、两个 owned 位、25 个辅助 `none`、重复主动石拒绝、非 owned 石拒绝、非法 ID 拒绝、取出、装入、占用槽替换、槽间交换。

- [ ] **Step 2: 运行 RED，确认失败原因仅为技能领域尚不存在**

  Run:
  ```powershell
  cmake --preset windows-msvc-core-debug
  cmake --build --preset windows-msvc-core-debug --target arpg_skills_tests
  ```
  Expected: 配置或编译失败，错误指向缺少 `src/skills` 类型/目标；不得因现有模块失败。

- [ ] **Step 3: 实现稳定类型与不变量接口**

  `active_skill_types.hpp` 使用以下公开形状：
  ```cpp
  namespace arpg::skills {
  enum class ActiveSkillId : std::uint8_t {
      draw_slash = 0,
      storm_swords = 1,
      count = 2,
      none = 0xFF,
  };
  enum class SupportSkillId : std::uint8_t {
      count = 0,
      none = 0xFF,
  };
  inline constexpr std::size_t kActiveSkillCount = 2U;
  inline constexpr std::size_t kActiveSkillSlotCount = 5U;
  inline constexpr std::size_t kSupportSlotsPerActive = 5U;

  struct ActiveSkillSlot final {
      ActiveSkillId active{ActiveSkillId::none};
      std::array<SupportSkillId, kSupportSlotsPerActive> supports{{
          SupportSkillId::none, SupportSkillId::none,
          SupportSkillId::none, SupportSkillId::none,
          SupportSkillId::none,
      }};
  };
  struct SkillLoadoutState final {
      std::array<ActiveSkillSlot, kActiveSkillSlotCount> slots{};
      std::uint64_t owned_active_bits{};
  };
  }
  ```
  禁止把辅助数组写成裸 `{}`，因为枚举值 `0` 是 `count` 而不是空位；默认成员初始化和 `empty_active_skill_slot()` 都必须显式填充五个 `SupportSkillId::none`。

- [ ] **Step 4: 实现目录和纯函数装配 API**

  `active_skill_catalog.hpp` 固定以下数值，后续战斗测试直接引用同一目录，禁止在战斗代码复制魔法数：
  ```cpp
  struct ActiveSkillDefinition final {
      ActiveSkillId id{ActiveSkillId::none};
      const char* display_name{};
      std::uint16_t cooldown_ticks{};
  };
  inline constexpr std::uint16_t kDrawSlashCooldownTicks = 240U;
  inline constexpr std::uint16_t kStormSwordsCooldownTicks = 1800U;
  [[nodiscard]] const ActiveSkillDefinition* active_skill_definition(
      ActiveSkillId id) noexcept;
  ```
  `skill_loadout.hpp` 提供：
  ```cpp
  enum class SkillLoadoutError : std::uint8_t {
      none, invalid_slot, invalid_active_id, invalid_support_id,
      active_not_owned, duplicate_active, destination_occupied
  };
  [[nodiscard]] SkillLoadoutState default_skill_loadout() noexcept;
  [[nodiscard]] SkillLoadoutError validate_skill_loadout(
      const SkillLoadoutState&) noexcept;
  [[nodiscard]] SkillLoadoutError remove_active_skill(
      SkillLoadoutState&, std::size_t slot) noexcept;
  [[nodiscard]] SkillLoadoutError equip_active_skill(
      SkillLoadoutState&, ActiveSkillId, std::size_t slot) noexcept;
  [[nodiscard]] SkillLoadoutError swap_active_skill_slots(
      SkillLoadoutState&, std::size_t left, std::size_t right) noexcept;
  ```
  `equip_active_skill` 只允许目标空槽；把一块已装备石移动到另一空槽时由调用方先 `remove` 再 `equip` 到同一候选副本，保证单步函数语义明确。占用槽之间只用 `swap_active_skill_slots`。

- [ ] **Step 5: 运行 GREEN 和领域回归**

  Run:
  ```powershell
  cmake --build --preset windows-msvc-core-debug --target arpg_skills_tests
  ctest --test-dir out/build/windows-msvc-core-debug -R '^skills\.units$' --output-on-failure
  ```
  Expected: `skills.units` 通过，`arpg_skills` 不依赖 dungeon、persistence、combat 或 raylib。

- [ ] **Step 6: 提交领域任务卡**

  ```powershell
  git add CMakeLists.txt src/skills tests/skills
  git commit -m "feat: add active skill stone loadout domain"
  ```

---

### Task 2: 将装配写入 V8 检查点并迁移 V1～V7

**Files:**
- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/CMakeLists.txt`
- Modify: `tests/persistence/CMakeLists.txt`
- Create: `tests/persistence/checkpoint_v8_skill_loadout_tests.cpp`
- Modify: `tests/persistence/persistence_test_main.cpp`

- [ ] **Step 1: 写 V8 往返、迁移和拒绝测试**

  覆盖：任意五槽排列往返；空槽；owned 位往返；V7 字节流迁移为默认装配；V1 固定样本同样迁移；主动 ID `2`、辅助 ID `0`、重复主动石、槽中非 owned 主动石均返回 `CodecError::invalid_state` 或 `invalid_enum`；V7 既有 golden 字节不改变。

- [ ] **Step 2: 运行 RED**

  ```powershell
  cmake --build --preset windows-msvc-core-debug --target arpg_persistence_tests
  ctest --test-dir out/build/windows-msvc-core-debug -R '^persistence\.units$' --output-on-failure
  ```
  Expected: 新 V8 测试因版本仍为 7 或状态未编码而失败。

- [ ] **Step 3: 扩展检查点状态和固定布局**

  在 `DungeonRunState` 末尾增加：
  ```cpp
  skills::SkillLoadoutState skill_loadout{
      skills::default_skill_loadout()};
  ```
  版本常量改为：
  ```cpp
  inline constexpr std::uint32_t kSeventhCheckpointFormatVersion = 7U;
  inline constexpr std::uint32_t kCheckpointFormatVersion = 8U;
  inline constexpr std::size_t kV8SkillLoadoutPayloadSize = 40U;
  inline constexpr std::size_t kV8BasePayloadSize = 756U;
  inline constexpr std::size_t kV8BaseEncodedCheckpointSize = 788U;
  ```
  40 字节按顺序为：`owned_active_bits` 8 字节 little-endian、5 个主动 ID、25 个辅助 ID、2 个保留零字节。物品可变记录仍紧随 base payload；不要改变 V7 的 716 字节解析偏移。

- [ ] **Step 4: 实现 V8 编解码与旧档默认迁移**

  编码前调用 `validate_skill_loadout`；解码逐个验证 enum，再验证整体不变量和两个保留字节为零。V1～V7 的成功路径在完成各自原有迁移后统一赋值 `default_skill_loadout()` 并保持 `migrated=true`。把 persistence 公开链接到 `arpg_skills`，不要让 skills 反向依赖 persistence。

- [ ] **Step 5: 运行 GREEN、V7 golden 和边界回归**

  ```powershell
  cmake --build --preset windows-msvc-core-debug --target arpg_persistence_tests
  ctest --test-dir out/build/windows-msvc-core-debug -R '^(persistence\.units|architecture\.persistence_checkpoint_only)$' --output-on-failure
  ```
  Expected: V8 新测试、全部旧版本迁移和 persistence 边界测试通过。

- [ ] **Step 6: 提交存档任务卡**

  ```powershell
  git add src/dungeon/dungeon_checkpoint.hpp src/persistence tests/persistence
  git commit -m "feat: persist skill stone loadout in checkpoint v8"
  ```

---

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

### Task 7: 增加五槽 HUD、技能特效和暂停页装卸界面

**Files:**
- Create: `src/platform/raylib/active_skill_view.hpp`
- Create: `src/platform/raylib/active_skill_view.cpp`
- Create: `src/platform/raylib/active_skill_renderer.hpp`
- Create: `src/platform/raylib/active_skill_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/inventory_view_math.hpp`
- Modify: `src/platform/raylib/inventory_view_math.cpp`
- Modify: `src/platform/raylib/inventory_renderer.hpp`
- Modify: `src/platform/raylib/inventory_renderer.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `tests/platform/active_skill_view_tests.cpp`
- Create: `tests/platform/active_skill_loadout_view_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

- [ ] **Step 1: 写纯 view/model RED 测试**

  在无窗口测试中断言：HUD 恒为五槽；显示 1～5；空槽模型明确；冷却比例 clamp 到 `[0,1]`；拔刀/暴风名称与目录一致；暂停技能页显示五个主槽、当前选中槽的五个辅助空位、未装备石库存；点击命中区域在 1280×720、1600×900、1920×1080 均不重叠；动作命令只有 select/remove/equip/swap，不产生辅助编辑命令。

- [ ] **Step 2: 运行 RED**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_platform_tests
  ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
  ```
  Expected: 新 view/model 类型和几何函数不存在。

- [ ] **Step 3: 实现只读 HUD 模型和布局**

  公开模型固定为：
  ```cpp
  struct ActiveSkillHudSlot final {
      std::uint8_t key_number{};
      skills::ActiveSkillId id{skills::ActiveSkillId::none};
      std::array<char, 48> name{};
      float cooldown_ratio{};
      bool empty{};
  };
  struct ActiveSkillHudModel final {
      std::array<ActiveSkillHudSlot,
          skills::kActiveSkillSlotCount> slots{};
  };
  ```
  HUD 锚定屏幕底部中央，五个 58×58 槽，8 px 间距；冷却用自下而上的半透明深色遮罩和剩余秒数；空槽画石槽轮廓。所有文字走现有 `HudFont`，不得用 raylib 默认字体。

- [ ] **Step 4: 实现技能原生几何表现**

  拔刀斩：按 snapshot facing 和命中 tick 绘制 0.12 秒白蓝扇形弧光，使用 `DrawTriangleFan`/线段，不加载外部纹理。暴风式：围绕 locked center 以固定 12 个角度绘制半透明剑形；当前 strike 高亮；finisher 绘制中心落剑、圆形冲击波和短屏闪。表现只消费快照/事件，不能改变伤害或触发时机。

- [ ] **Step 5: 实现暂停页主动槽操作**

  在现有暂停→背包页面增加“技能石”子页按钮，不改变装备/材料页默认行为。交互规则固定为：单击主槽选中；选中已装备槽后点“取出”发 remove；选中一个库存石后点击空主槽发 equip；依次点击两个已装备主槽发 swap；保存 pending 时禁用全部装配按钮并显示“正在保存”；失败显示现有保存错误通知且保持原模型。五个辅助框只画“空”，不响应点击。

- [ ] **Step 6: 运行 GREEN、字体和三分辨率布局回归**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_platform_tests
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|stage11c\.hud_stress\.zero_alloc_100k)$' --output-on-failure
  ```
  Expected: 五槽/暂停页模型通过，现有 HUD 字体、背包命中和 100k 零分配测试不回归。

- [ ] **Step 7: 提交界面任务卡**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "feat: add active skill hud and loadout screen"
  ```

---

### Task 8: 完成真实 Raylib 验收、文档和整体验证

**Files:**
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `tests/platform/stage17_skill_stones_game_validation.cpp`
- Create: `tests/platform/stage17_skill_stones_validator.ps1`
- Create: `tests/platform/stage17_skill_stones_validator_self_test.ps1`
- Create: `docs/validation/stage17-swordmaster-skill-stones.md`
- Modify: `README.md`

- [ ] **Step 1: 先注册 Stage 17 正向和负向证据测试**

  注册：
  - `stage17.skill_stones.real_raylib`
  - `stage17.skill_stones.evidence_validator`
  - `stage17.skill_stones.evidence_validator_self_test`

  第一个运行真实 raylib 6.0/OpenGL、真实 `SaveStore` 和生产 `DungeonRuntime`；第二个校验新鲜文件；第三个复制证据并分别破坏 PNG、状态文件字段和时间戳，断言 validator 拒绝。

- [ ] **Step 2: 运行 RED，确认因验证场景尚未实现而失败**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_stage17_skill_stones_game_validation
  ctest --test-dir out/build/windows-msvc-debug -L stage17 --output-on-failure
  ```
  Expected: 缺少正式场景或证据文件，不能假通过。

- [ ] **Step 3: 实现生产路径正式场景**

  场景固定 1280×720，并通过公开输入/事务路径依次完成：
  1. 新档截图五槽默认状态；
  2. 用数字键 1 施放拔刀斩并在命中帧截图；
  3. 用数字键 2 施放暴风式并在剑阵与终结各截图；
  4. 暂停页取出槽 1、把拔刀斩装入槽 5、交换槽 2/5；
  5. 关闭并重启真实 runtime，截图并核验交换后装配仍在；
  6. 输出 UTF-8 状态文件，包含 V8、五槽 ID、owned bits、25 个辅助 none、两技能命中计数、暴风式中心不随玩家移动、重启后装配。

- [ ] **Step 4: 实现证据验证器**

  必须检查 5 张非空 PNG 均为 1280×720、修改时间晚于本次 run marker、像素内容互不相同；状态文件所有字段精确匹配；不得仅检查文件存在。所有清理限定在测试提供的 `stage17-run` 子目录，先解析绝对路径并验证仍位于 evidence root 内。

- [ ] **Step 5: 运行 Stage 17 专项 GREEN**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_stage17_skill_stones_game_validation arpg_platform_tests arpg_dungeon_tests arpg_combat_tests arpg_persistence_tests arpg_skills_tests
  ctest --test-dir out/build/windows-msvc-debug -L stage17 --output-on-failure
  ```
  Expected: 正向真实 Raylib、validator 和 validator self-test 全通过。

- [ ] **Step 6: 运行 Debug 非图形完整回归和相关图形套件**

  ```powershell
  ctest --test-dir out/build/windows-msvc-debug -LE 'graphics|formal-game' --output-on-failure
  ctest --test-dir out/build/windows-msvc-debug -R '^(stage16\.loot_reinforcement\.real_raylib|stage17\.skill_stones\.)' --output-on-failure
  ```
  Expected: 所有非图形测试及 Stage 16/17 真实 Raylib 通过；若旧 Stage 9 图形基线仍失败，仅按已有记录列出，不能算作 Stage 17 通过项。

- [ ] **Step 7: 运行 Release 构建和 Stage 17 测试**

  ```powershell
  cmake --preset windows-msvc-release
  cmake --build --preset windows-msvc-release
  ctest --test-dir out/build/windows-msvc-release -L stage17 --output-on-failure
  ctest --test-dir out/build/windows-msvc-release -LE 'graphics|formal-game' --output-on-failure
  ```
  Expected: Release 编译成功，Stage 17 与非图形完整回归通过。

- [ ] **Step 8: 更新验证记录并做规格自审**

  `docs/validation/stage17-swordmaster-skill-stones.md` 记录准确命令、通过数量、耗时、五张绝对证据路径、V8 迁移、两技能数值和已知基线异常。README 把当前阶段改为 Stage 17。逐条对照设计文档，确认：5 主槽、每槽 5 空辅助位、全槽可取出/替换/交换、默认 1/2、空 3～5、原子保存、冷却不持久化、两技能战斗行为都已覆盖。

- [ ] **Step 9: 扫描占位实现、非法依赖和测试后门**

  ```powershell
  rg -n "TODO|TBD|placeholder|not implemented|test injection|private injection" src tests docs/validation/stage17-swordmaster-skill-stones.md
  rg -n "IsKeyPressed|IsKeyDown|GetKeyPressed" src/platform/raylib -g '!host_input.cpp' -g '!raylib_input.cpp' -g '!stable_key_raylib.cpp'
  ctest --test-dir out/build/windows-msvc-debug -R '^architecture\.' --output-on-failure
  git diff --check
  ```
  Expected: 新增文件无占位行为、数字键没有旁路采样、模块边界通过、无空白错误。

- [ ] **Step 10: 提交最终验收任务卡**

  ```powershell
  git add README.md src/platform/raylib/raylib_host.hpp src/platform/raylib/raylib_host.cpp tests/platform docs/validation/stage17-swordmaster-skill-stones.md
  git commit -m "test: validate stage17 skill stones in raylib"
  ```

---

## Final completion checklist

- [ ] `git status --short` 为空，没有混入用户既有或无关改动。
- [ ] `1` 施放拔刀斩，`2` 施放暴风式，`3`～`5` 空槽无副作用。
- [ ] 五个主动槽均可取出、装入、交换，保存失败不半更新。
- [ ] 每个主槽恰好显示五个只读空辅助槽，战斗不读取辅助数组。
- [ ] 拔刀斩每目标一次、明确击退；暴风式锁定中心、12 段、终结击飞并可被死亡/换房取消。
- [ ] V8 往返、V1～V7 迁移、非法 ID/重复石拒绝全部有测试。
- [ ] Debug/Release 的 Stage 17、新增单元和非图形完整回归均有新鲜结果。
- [ ] 真实 1280×720 Raylib 五张截图和状态证据通过内容验证及破坏性自测。
