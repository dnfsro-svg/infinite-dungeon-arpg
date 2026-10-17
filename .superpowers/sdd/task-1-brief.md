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

  在根 `CMakeLists.txt` 中把 `add_subdirectory(src/skills)` 放在 `src/combat` 之前，把 `add_subdirectory(tests/skills)` 放在 `tests/combat` 之前。建立 `arpg_skills_tests`，测试：默认装配、两个 owned 位、25 个辅助 `none`、重复主动石拒绝、非 owned 石拒绝、非法 ID 拒绝、取出、装入、占用槽装入拒绝、槽间交换。

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
