### Task 5B: 抽取 Stage11D 呈现语义与报告

**BASE:** `a9e34cd`

**Files:**
- Create: `src/platform/raylib/host_validation_stage11d_report.cpp`
- Modify: `src/platform/raylib/host_validation_stage11d.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/stage11d_loot_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11d_loot_evidence_guard_self_test.cmake`
- Modify: `tests/platform/stage11d_renderer_integration_guard_test.cmake`
- Modify: `tests/platform/stage11d_renderer_integration_guard_self_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`

**Interfaces:**

```cpp
bool stage11d_target_visible(
    const RaylibHostConfig&, const dungeon::DungeonSnapshot&,
    const PauseMenuState&, const DungeonRenderStatus&,
    settings::LootFilterMode, const GroundLootView&, HudNoticeView,
    Stage11DLootValidationState&) noexcept;
void stage11d_record_semantics(
    Stage11DLootValidationState&, const dungeon::DungeonSnapshot&,
    const items::ItemOwnershipState*, const GroundLootView&,
    HudNoticeView) noexcept;
void write_stage11d_loot_validation_summary(
    const RaylibHostConfig&, const Stage11DLootValidationState&,
    const PauseMenuState&) noexcept;
```

**Hard constraints:**
- Pure structural extraction. Do not change gameplay, validation scenarios,
  timing, input, RNG, renderer behavior, assets, persistence, public config,
  evidence fields, or pass/fail criteria.
- Move only Stage11D semantic recording, target-visible evaluation, scenario
  naming, and summary/report implementation to the new report source.
- Preserve every rarity count, item-id set, pickup notice rectangle, receipt,
  abyss, exception, and `result=pass/fail` condition field-for-field.
- Keep the real host-loop creation of `GroundLootView`, HUD notice sampling,
  semantic call ordering, reached/capture decisions, `EndDrawing()`/capture
  result ordering, and captured-state mutation in `raylib_host.cpp`.
- Register the new source exactly once. Do not introduce a test-only path or
  duplicate the moved implementation.
- Guards must prove ownership in the report source and retained integration in
  the actual `run_raylib_host` scope. Add named RED mutations that reject
  comment/string/forward-declaration/lambda/cross-function and ordering decoys.
- Work only in this isolated worktree, use `apply_patch`, run build/tests
  serially (`-j1`), and check for duplicate/residual processes. Do not touch
  the controller checkout `E:\game`.
- The known unrelated `architecture.persistence_checkpoint_only` baseline
  failure remains out of scope. Do not repair or hide it.

- [x] **Step 1: migrate presented/capture/report guards and prove RED**

  Move implementation-owned Stage11D seam expectations to the report source.
  Keep renderer-integration proof in the host for real view/HUD/presentation/
  capture ordering and prove the pre-extraction layout is rejected.

- [x] **Step 2: move semantics, visibility tail, and summary verbatim**

  Extract the current `evidence_semantics` implementation. Keep host call
  sites and all reached/visible-capture/captured/summary ordering unchanged.

- [x] **Step 3: serial build and complete 11D headless gate**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests arpg_stage11d_loot_formal -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_sequence|stage11d\.architecture\.|stage11d\.renderer_integration_guard|stage11d\.loot_evidence_guard|stage11d\.loot_root_safety)$' --output-on-failure -j1
  git diff --check
  ```

  Parent build passed in 217 seconds. The exact headless selection passed 5/5
  in 126.64 seconds: `platform.units` 17.43s,
  `stage11d.loot_root_safety` 0.08s,
  `platform.host_validation_sequence` 59.59s,
  `stage11d.renderer_integration_guard` 33.28s, and
  `stage11d.loot_evidence_guard` 15.89s.

- [x] **Step 4: run the formal Stage11D evidence chain — BASE-proven red, deferred**

  ```powershell
  ctest --test-dir out/build/windows-msvc-debug -R '^stage11d\.loot_(formal|evidence_validator)' --output-on-failure -j1
  ```

  The formal test failed in 0.03 seconds because current `--select-only`
  returns exit 4; the dependent validator did not run. A detached rebuild of
  BASE `a9e34cd` reproduced the same `--select-only` exit 4. Task 5B does not
  modify the formal source or the core/dungeon/items/combat selector
  dependencies, so this pre-existing red is recorded and deferred instead of
  being changed in a pure structural card.

- [x] **Step 5: independent review and commit**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "refactor: extract stage11d validation reporting"
  ```

  Independent review is complete with no open P1/P2/P3. The P3
  `[[nodiscard]]` and P2 producer/summary binding findings are closed. Task 5B
  was committed as `18fb820`.
