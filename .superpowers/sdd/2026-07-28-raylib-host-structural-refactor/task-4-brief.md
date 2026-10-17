### Task 4: 抽取 Stage11C HUD 验收

**Files:**
- Create: `src/platform/raylib/host_validation_stage11c.hpp`
- Create: `src/platform/raylib/host_validation_stage11c.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/stage11c_hud_architecture_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_architecture_guard_self_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_self_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`

**Interfaces:**

```cpp
struct Stage11CHudValidationState final;  // 拥有 HudViewModel/Notice/Layout 值
PhysicalKeySnapshot inject_stage11c_physical_edges(
    PhysicalKeySnapshot, const RaylibHostConfig&,
    const settings::SettingsData&, const dungeon::DungeonSnapshot&,
    Stage11CHudValidationState&) noexcept;
std::uint64_t stage11c_production_snapshot_hash(
    const dungeon::DungeonSnapshot&) noexcept;
bool stage11c_hud_validation_reached(
    const dungeon::DungeonSnapshot&, Stage11CHudValidationScenario,
    const Stage11CHudValidationState&, bool draw_debug) noexcept;
void write_stage11c_hud_validation_summary(
    const RaylibHostConfig&, const Stage11CHudValidationState&) noexcept;
```

- [ ] **Step 1: 扩展 Task 0 守卫到新 Stage 源并确认 RED**

  driver 和 summary 从 `host_validation_stage11c.cpp` 读取；主循环 capture surface 独立检查真实 renderer model/notices/layout/hash 的唯一赋值和真实 presented-frame 截图。

- [ ] **Step 2: 原样移动状态和 1857–1936、2145–2299 行**

  低血量夹具、debug visibility、CJK font readiness、target presented-frame 计数与 hash 采样时机不变。

- [ ] **Step 3: 编译并运行所有 11C headless 门禁**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests arpg_stage11c_hud_stress arpg_stage11c_hud_formal -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_sequence|stage11c\.architecture\.|stage11c\.hud_evidence_guard|stage11c\.hud_stress)' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 4: 运行正式 11C HUD 证据链**

  ```powershell
  ctest --test-dir out/build/windows-msvc-debug -R '^stage11c\.hud_(formal|evidence_validator)' --output-on-failure -j1
  ```

- [ ] **Step 5: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "refactor: extract stage11c host validation"
  ```

---

