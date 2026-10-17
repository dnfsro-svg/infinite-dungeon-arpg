### Task 1: 抽取跨 Stage 的无状态输入与导航原语

**Files:**
- Create: `src/platform/raylib/host_validation_navigation.hpp`
- Create: `src/platform/raylib/host_validation_navigation.cpp`
- Create: `src/platform/raylib/host_validation_input.hpp`
- Create: `src/platform/raylib/host_validation_input.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_architecture_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_architecture_guard_self_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_self_test.cmake`

**Interfaces:**

```cpp
namespace arpg::platform::host_validation {
void inject_validation_pressed(PhysicalKeySnapshot&,
    settings::StableKey) noexcept;
void inject_validation_action(PhysicalKeySnapshot&,
    const settings::SettingsData&, settings::SettingAction,
    bool pressed) noexcept;
void inject_validation_movement(PhysicalKeySnapshot&,
    const settings::SettingsData&, combat::MovementInput) noexcept;
[[nodiscard]] combat::MovementInput validation_route_fire_movement(
    combat::Vec3 player, combat::Vec3 target,
    combat::MovementInput requested) noexcept;
[[nodiscard]] const combat::MonsterSnapshot* nearest_living_monster(
    const combat::CombatSnapshot&) noexcept;
[[nodiscard]] combat::MovementInput validation_movement_toward(
    combat::Vec3 from, combat::Vec3 to) noexcept;
[[nodiscard]] combat::Vec3 validation_door_position(
    dungeon::ExitDirection) noexcept;
[[nodiscard]] combat::MovementInput validation_exit_movement(
    combat::Vec3 player, dungeon::ExitDirection) noexcept;
[[nodiscard]] bool validation_attack_lane(
    const combat::CombatSnapshot&,
    const combat::MonsterSnapshot&) noexcept;
[[nodiscard]] dungeon::ExitDirection validation_direction(
    const RaylibHostConfig&) noexcept;
}
```

- [ ] **Step 1: 扩展结构守卫并确认 RED**

  要求三个输入定义只存在于 `host_validation_input.cpp`、七个导航定义只存在于 `host_validation_navigation.cpp`，头中只有声明，两个 `.cpp` 均登记 `arpg_raylib`，且 `raylib_host.cpp` 不再定义旧 helper。当前应因新文件缺失而失败。

- [ ] **Step 2: 创建具名私有命名空间和声明**

  input 头只包含 `PhysicalKeySnapshot`、settings 和 movement 所需声明；navigation 头只包含 combat/dungeon 值类型与 `RaylibHostConfig` 所需声明。两者均不得包含 `<raylib.h>`、renderer、persistence 或测试头。

- [ ] **Step 3: 逐函数原样移动实现**

  input 文件移动原 696–703、1857–1884 行，并把私有误名 `inject_stage11b_pressed`、`inject_stage11c_binding`、`inject_stage11c_movement` 统一为上面的三个通用名字；11B/11C/11D/17 全部显式调用同一实现。navigation 文件移动原 739–757、1693–1718、1820–1850、2606–2624 行。不合并重复分支、不改变浮点常量、不重排遍历。

- [ ] **Step 4: 迁移受影响的 Stage11C 守卫锚点**

  driver 仍从 `inject_stage11c_physical_edges` 开始检查，物理 action/movement helper 改从共享 input 源检查；mutation self-test 必须能分别拒绝“直接逻辑 action 队列”和“跳过 stable binding”的变异。

- [ ] **Step 5: 编译并验证 GREEN**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_raylib arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_sequence|architecture\.|stage11c\.architecture\.|stage11c\.hud_evidence_guard)' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 6: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/platform/host_validation_sequence_guard_test.cmake tests/platform/stage11c_hud_architecture_guard_test.cmake tests/platform/stage11c_hud_architecture_guard_self_test.cmake tests/platform/stage11c_hud_evidence_guard_test.cmake tests/platform/stage11c_hud_evidence_guard_self_test.cmake
  git commit -m "refactor: extract host validation input and navigation"
  ```

---

