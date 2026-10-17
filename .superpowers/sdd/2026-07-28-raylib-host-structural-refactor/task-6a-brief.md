### Task 6A: 抽取 Stage17 主动技能运行时验收

**BASE:** `7ae2744`

**Files:**
- Create: `src/platform/raylib/host_validation_stage17.hpp`
- Create: `src/platform/raylib/host_validation_stage17_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/host_input_source_test.cmake`
- Modify: `tests/platform/input_latency_source_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`
- Modify: `tests/platform/stage12_environment_render_tests.cpp`

**Interfaces:**

```cpp
struct Stage17SkillStonesValidationState final;
void observe_stage17_draw_runtime(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&, const dungeon::DungeonSnapshot&,
    const ActiveSkillDrawRuntimeStatus&) noexcept;
void observe_stage17_combat_event(Stage17SkillStonesValidationState*,
    const combat::CombatEvent&) noexcept;
PhysicalKeySnapshot inject_stage17_physical_edges(
    PhysicalKeySnapshot, const RaylibHostConfig&,
    const settings::SettingsData&, const dungeon::DungeonSnapshot&,
    Stage17SkillStonesValidationState&) noexcept;
void observe_stage17_submitted_actions(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&, const SubmittedFrameActions&) noexcept;
void observe_stage17_snapshot(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&,
    const dungeon::DungeonSnapshot&) noexcept;
void observe_stage17_inventory(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&, const InventoryRenderer&,
    const dungeon::DungeonSnapshot&) noexcept;
```

**Hard constraints:**
- Pure structural extraction. Do not change gameplay, skill behavior, input,
  timing, RNG, renderer behavior, assets, loadout transactions, cooldown
  drain, storm isolation, capture/report semantics, persistence, or public
  configuration.
- Move the Stage17 state and only the runtime observers/injector plus their
  private helpers into the new header/runtime source. Preserve bodies and all
  host-loop call sites and ordering against BASE `7ae2744`.
- Keep capture basename/path, capture completion mutation, completion
  predicate, loadout/report helpers, summary writer, `EndDrawing()` and actual
  screenshot lifecycle in `raylib_host.cpp`; these belong to Task 6B.
- The public header must not include or expose `raylib.h`, `Rectangle`,
  `Vector2`, or screen-query APIs. Any such implementation detail must remain
  in the runtime `.cpp`.
- Preserve the public input path, loadout transaction sequence, cooldown drain,
  storm target isolation/fail-closed rules, draw/storm renderer evidence,
  sampled-key ordering, event observation and the exact
  draw/submitted/snapshot/inventory observer order in the real host loop.
- Register the runtime source exactly once. Do not introduce a test-only path,
  duplicate implementation, fallback behavior, or a new translation-unit
  global with changed lifetime.
- Source guards must read both the real host loop and the runtime source, bind
  producer/consumer relationships to executable function scopes, and reject
  comment/string/forward-declaration/lambda/cross-scope/CMake decoys. Record
  named RED mutations before extraction and prove they turn GREEN afterward.
- Change exactly the eight files listed above. Work only in the isolated
  worktree, use `apply_patch`, build/test serially (`-j1`), and check for
  residual build processes. Do not touch controller checkout `E:\game`.
- The unrelated known `architecture.persistence_checkpoint_only` and
  Stage11D formal selector BASE failures remain out of scope.

- [x] **Step 1: migrate Stage17 runtime source guards and prove RED**

  Make `host_input_source_test.cmake` and `input_latency_source_test.cmake`
  read the actual host loop plus the new runtime source. Preserve and bind the
  loadout transaction, cooldown drain, storm isolation, public-input and
  renderer-evidence requirements. Prove the pre-extraction layout or named
  ownership/scope mutations fail before moving production code.

- [x] **Step 2: move Stage17 state and runtime verbatim**

  Move the state/enums required by both runtime and retained report code into
  the header. Move the six public runtime functions and only their private
  helpers into the runtime source. Keep every capture/report function and
  every host-loop call site/order unchanged.

- [x] **Step 3: serial build and targeted gate**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_input_source|platform\.input_latency_source|platform\.host_validation_sequence|stage17\.active_skill_material_asset_pipeline)$' --output-on-failure -j1
  git diff --check
  ```

- [x] **Step 4: independent review and commit**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "refactor: extract stage17 validation runtime"
  ```
