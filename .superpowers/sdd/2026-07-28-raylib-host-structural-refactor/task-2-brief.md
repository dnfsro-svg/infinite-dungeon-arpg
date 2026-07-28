### Task 2: 抽取 Stage10 与 Stage11 状态、路线和完成判定

**Files:**
- Create: `src/platform/raylib/host_validation_stage10_11.hpp`
- Create: `src/platform/raylib/host_validation_stage10_11.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/dungeon/stage10_evidence_guard_test.cmake`
- Modify: `tests/dungeon/stage11_death_evidence_guard_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`

**Interfaces:**

```cpp
struct Stage10ValidationState final {
    bool entered_abyss{};
    bool reset_requested{};
    bool descent_warning_seen{};
    std::uint32_t chaos_presented_frames{};
};
struct Stage11ValidationState final {
    bool entered_abyss{};
    bool saw_depth_two{};
    bool continue_requested{};
    std::uint32_t target_presented_frames{};
};
combat::MovementInput stage10_validation_input(
    dungeon::DungeonSession&, const dungeon::DungeonSnapshot&,
    const RaylibHostConfig&, Stage10ValidationState&) noexcept;
combat::MovementInput stage11_validation_input(
    dungeon::DungeonSession&, const dungeon::DungeonSnapshot&,
    const RaylibHostConfig&, Stage11ValidationState&) noexcept;
bool stage10_validation_reached(
    const dungeon::DungeonSnapshot&, const RaylibHostConfig&,
    const Stage10ValidationState&) noexcept;
bool stage11_validation_reached(
    const dungeon::DungeonSnapshot&, const RaylibHostConfig&,
    const Stage11ValidationState&) noexcept;
```

- [ ] **Step 1: 先更新证据守卫的目标源并确认 RED**

  Stage10/11 守卫分别扫描 `raylib_host.cpp` 的正式调用次序和 `host_validation_stage10_11.cpp` 的路线/完成算法；在新源缺失时失败。禁止删除任何真实 Session 请求、物理移动和截图次序断言。

- [ ] **Step 2: 原样移动两个状态结构和四个公开给主机的函数**

  同时移动它们独占的 `has_environment_visual` 等叶子帮助函数。场景枚举仍留在 `raylib_host.hpp`，状态类型进入具名私有命名空间。

- [ ] **Step 3: 保持主循环判定和移动优先级**

  `validation_continue`、`continue_requested`、presented-frame 计数和 `stage10_validation_captured` 的更新位置不变；只替换类型/函数限定名。

- [ ] **Step 4: 运行聚焦回归**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_dungeon_tests arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.host_validation_sequence|stage10\..*|stage11\..*|dungeon\.units|platform\.units)$' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 5: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/dungeon tests/platform/host_validation_sequence_guard_test.cmake
  git commit -m "refactor: extract stage10 and stage11 host validation"
  ```

---
