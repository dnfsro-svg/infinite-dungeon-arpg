### Task 0: 冻结主机验收基线并修复已漂移的 Stage11C 守卫

**Files:**
- Create: `docs/validation/raylib-host-refactor-baseline.md`
- Create: `tests/platform/host_validation_sequence_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_self_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- 新守卫锁定三组次序：输入注入链、固定步移动优先级、摘要写入顺序。
- Stage11C 守卫从真实引用别名和真实采集赋值建立 capture surface，不再依赖早已删除的 `Stage11CHudValidationState stage11c_validation_state{};` 文本。
- 守卫自测继续证明直接覆盖 `HudViewModel`、伪造 snapshot hash、绕过物理输入或伪造 `result=pass` 会失败。

- [ ] **Step 1: 记录基线规模、符号归属和当前失败**

  记录 `raylib_host.cpp` 行数、六组状态定义区间、各 Stage 函数区间、主循环调用点、分支哈希和工作树状态。直接运行现有 Stage11C 守卫，保存上述已知错误；不得把它描述成重构引入。

  ```powershell
  cmake "-DSOURCE_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07" -P tests/platform/stage11c_hud_evidence_guard_test.cmake
  ```

- [ ] **Step 2: 写入顺序特征守卫**

  在 sanitize 掉注释后，分别查找并比较下列 token 的唯一位置；任何缺失、重复或调序均 `FATAL_ERROR`：

  ```text
  sample_physical_keys -> inject_stage11b_physical_edges -> inject_stage11c_physical_edges
  -> inject_stage11d_physical_edges -> inject_stage17_physical_edges -> map_host_frame_input

  stage11_validation_input -> stage10_validation_input -> step_movement = movement;

  write_stage11b_validation_summary -> write_stage11c_hud_validation_summary
  -> write_stage11d_loot_validation_summary -> write_stage17_validation_summary
  ```

- [ ] **Step 3: 修复 Stage11C 守卫并保留变异自测**

  用 `Stage11CHudValidationState& stage11c_validation_state =` 作为当前 runtime 起点；将 driver、runtime 和 summary 分段检查，保留“模型只赋值一次、hash 只能来自 `stage11c_production_snapshot_hash(current)`、真实截图在 `EndDrawing()` 后读取”的全部断言。增加一个把引用别名改成独立值的 mutation，要求守卫拒绝。

- [ ] **Step 4: 配置并验证基线守卫 GREEN**

  ```powershell
  $env:CMAKE_BUILD_PARALLEL_LEVEL='1'
  cmake --preset windows-msvc-debug
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.host_validation_sequence|stage11c\.hud_evidence_guard|stage11c\.hud_evidence_guard_self_test)$' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 5: 独立审查并提交**

  ```powershell
  git add docs/validation/raylib-host-refactor-baseline.md tests/platform
  git commit -m "test: freeze raylib host validation baseline"
  ```

---
