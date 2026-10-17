### Task 5A: 抽取 Stage11D 状态、选择器和输入运行时

**BASE:** `3aea1ee`

**Files:**
- Create: `src/platform/raylib/host_validation_stage11d.hpp`
- Create: `src/platform/raylib/host_validation_stage11d_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/stage11d_loot_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11d_loot_evidence_guard_self_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`

**Interface:**

```cpp
struct Stage11DLootValidationState final;
PhysicalKeySnapshot inject_stage11d_physical_edges(
    PhysicalKeySnapshot, const RaylibHostConfig&,
    const settings::SettingsData&, const dungeon::DungeonSnapshot&,
    Stage11DLootValidationState&) noexcept;
```

**Hard constraints:**
- Pure structural extraction; no gameplay, validation-scenario, timing, input,
  RNG, asset, renderer, persistence, or public-config behavior changes.
- Move Stage11D state, selector/navigation, physical-input injection, and
  fixed-step runtime only. Presented/capture/report semantics remain in
  `raylib_host.cpp` for Task 5B.
- Preserve every fixed-capacity field, monster ordinal, receipt generation,
  abyss-claim flag, input ordering, and fixed-step counter exactly.
- Reuse the shared input/navigation primitives extracted in Task 1; do not
  duplicate logical action injection or create test-only runtime paths.
- Stage11D guard must scan the new runtime source for moved seams and continue
  scanning the real `run_raylib_host` loop for sample-map-pause-submit order
  and abyss-claim submission. It must not exclude the whole host file.
- Tests must contain named RED mutations that prove the old guard fails and
  the new guard rejects comment/string/lambda/cross-scope decoys for both the
  moved source and retained host-loop obligations.
- Work only in this isolated worktree, use `apply_patch`, build/test `-j1`,
  and monitor duplicate processes/resource usage. Do not touch `E:\game`.

- [x] **Step 1: extend guards to the new runtime source and prove RED**

  Move the state/selector/physical-input/fixed-step seam expectations to the
  new source while retaining the host input-order and abyss-claim checks.

- [x] **Step 2: move the Stage11D runtime implementation verbatim**

  Extract the current equivalents of original ranges 376–430, 629–694,
  1721–1818, and 1937–2144. Keep all presentation/report code in the host.

- [x] **Step 3: serial build and exact headless gate**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_sequence|stage11d\.architecture\.|stage11d\.loot_evidence_guard)' --output-on-failure -j1
  git diff --check
  ```

  First exact-seven run: 5/7 passed. The only failures were
  `stage11d.architecture.loot_boundaries_self_test` and
  `stage11d.loot_evidence_guard_self_test`, both terminated by their legacy
  300-second CTest timeout at 300.04 and 300.03 seconds respectively, without
  an assertion diagnostic. The evidence self-test passes directly in 483.1
  seconds. Task 5A therefore updates only their CTest metadata budgets to 600
  and 900 seconds; test assertions, mutations, selection, and production code
  remain unchanged. Do not treat the timeout-only first run as a logic failure.
  The final serial exact-seven rerun passed 7/7 in 1802.84 seconds; the two
  corrected self-test budgets were validated by 305.58-second and 445.55-second
  successful runs.

- [x] **Step 4: independent review and commit**

  Independent review, the final gate, and commit `6566026` are complete.

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "refactor: extract stage11d validation runtime"
  ```
