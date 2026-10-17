### Task 3: 抽取 Stage11B 设置验收

**Files:**
- Create: `src/platform/raylib/host_validation_stage11b.hpp`
- Create: `src/platform/raylib/host_validation_stage11b.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/stage11b_architecture_guard_test.cmake`
- Modify: `tests/platform/stage11b_settings_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11b_settings_evidence_guard_self_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`

**Interfaces:**

```cpp
struct Stage11BValidationState final;  // 完整定义仅在私有 Stage 头中
PhysicalKeySnapshot inject_stage11b_physical_edges(
    PhysicalKeySnapshot, const RaylibHostConfig&,
    Stage11BValidationState&) noexcept;
bool stage11b_validation_complete(
    const RaylibHostConfig&, const Stage11BValidationState&,
    const PauseMenuState&) noexcept;
std::uint64_t stage11b_snapshot_hash(
    const dungeon::DungeonSnapshot&) noexcept;
void write_stage11b_validation_summary(
    const RaylibHostConfig&, const Stage11BValidationState&,
    const PauseMenuState&) noexcept;
```

- [ ] **Step 1: 将 11B 守卫改成双表面检查并确认 RED**

  算法 token、summary 字段和按键注入只从新 Stage 源读取；`EndDrawing -> LoadImageFromScreen`、pause capture、fixed tick 和输入链仍从主循环读取。三个 mutation self-test 改为复制/变异正确的源文件。

- [ ] **Step 2: 原样移动状态与 1541–1691 行实现**

  `load_status` 初始化仍取自同一次 `SettingsStore::load()`；暂停前后 tick/hash 采集点和重绑帧号保持不变。

- [ ] **Step 3: 编译并运行 11B 守卫/单元测试**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests arpg_stage11b_settings_formal -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_sequence|stage11b\.architecture\.|stage11b\.settings_evidence_guard)' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 4: 运行正式 11B 证据链**

  ```powershell
  ctest --test-dir out/build/windows-msvc-debug -R '^stage11b\.settings_(formal|evidence_validator)$' --output-on-failure -j1
  ```

- [ ] **Step 5: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "refactor: extract stage11b host validation"
  ```

---

