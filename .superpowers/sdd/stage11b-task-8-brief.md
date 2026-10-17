### Task 8: Host Integration and True Simulation Freeze

**Files:**
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/platform/raylib/host_launch_options.*`
- Create: `tests/platform/pause_host_gate_tests.cpp`
- Modify: `tests/platform/host_launch_options_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/input_latency_source_test.cmake`

**Interfaces:**
- `RaylibHostConfig` gains optional `settings_directory`; default is resolved save directory.
- Host owns `SettingsStore`, `PauseMenuState`, committed settings and one `PhysicalKeySnapshot` per presented frame.

**Required behavior:**
- Test 600 paused presented frames preserve fixed tick and frozen DungeonSnapshot; resume first frame has at most the normal fixed-step cap and no catch-up.
- Source guard requires `fixed_step.clear_accumulator()` on pause entry and forbids `fixed_step.advance` inside the paused branch.
- Esc closes inventory/passive before pause; death/recovery behavior remains unchanged.
- Frame order: sample once → recovery/death → inventory/passive close → pause state → gameplay mapping → fixed step → draw world → draw overlays → capture.
- Apply: validate draft → preview/readback → SettingsStore save → publish or rollback.
- Paused branch continues renderer/UI/screenshots but never calls `runtime.fixed_tick`.

**Verification:**
`ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.input_latency_source|stage11.death_formal.five_paths|stage11.death_evidence.mutation_self_test)$" --output-on-failure`

**Commit:** `feat: pause host and atomically apply settings`
