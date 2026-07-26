# Visible-Set 240 FPS and Eight-Core Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a native maximum 240 FPS presentation path with lower input latency, deterministic use of up to eight CPU execution lanes, and only measured OpenGL 3.3 GPU migrations, while keeping 60 Hz gameplay authority and the 1,125-monster room population semantically unchanged.

**Architecture:** Production presentation stops copying full `DungeonSnapshot` values and instead consumes versioned, camera-bounded frame/render views. Input is sampled every presentation frame and latched until one 60 Hz authority tick consumes each edge exactly once. Pure CPU draw preparation may use one main-thread lane plus at most seven persistent frame workers, while the square-room save worker performs immutable encode/I/O under the same seven-token non-main execution budget. Raylib/OpenGL, authority, RNG, audio, checkpoint capture/result application, and stable command submission remain on the main thread. A bounded deadline guard may reuse two compatible complete presentation frames for at most 8.33 ms, but it never fabricates authority or masks save/disk stalls. GPU work remains OpenGL 3.3 and is accepted only when measurement proves that batching shared-atlas environment or adjacent compatible draw runs is worthwhile.

**Tech Stack:** C++17, raylib 6.0, rlgl/OpenGL 3.3, CMake 3.25+, Ninja, MSVC, Win32 performance counters, fixed-step 60 Hz authority, CTest.

## Global Constraints

- Do not begin until both `2026-07-26-prerequisite-feature-integration.md` and `2026-07-26-square-hundredfold-room.md` are complete, reviewed, pushed, and the integration worktree is clean.
- Work only in `E:/game/.worktrees/integration-large-square-rooms` on `codex/integration-large-square-rooms`; never modify the dirty control checkout at `E:/game`.
- Gameplay authority remains exactly 60 Hz. Damage, collisions, AI decisions, RNG consumption, drops, death, room progress, persistence, and skill timing must be byte/event deterministic across presentation rates and lane counts.
- Presentation is capped at 240 FPS only when VSync is disabled. VSync-enabled output follows the display refresh rate; neither mode may busy-loop without a limiter.
- The frame path is bounded by the square-room contracts: at most 128 resident monsters, 512 projectiles, 128 hazards, 128 environment objects, and the fixed visible loot capacities. A total room population up to 1,125 must not cause a per-frame full-room scan.
- The main thread owns every raylib, rlgl, OpenGL, Win32 input, audio, RNG, checkpoint capture/result application, and final draw call. Frame workers receive immutable value inputs and emit fixed-capacity CPU commands only; the save worker receives one immutable checkpoint slot and returns only a revision-tagged exact result.
- Use at most eight simultaneously executing lanes: the main thread plus seven `ExecutionLaneBudget` tokens. Frame workers and the save worker acquire one token only while executing and release it before condition-variable sleep; an active save leaves at most six frame workers, so main + save + six frame workers is exactly eight. No `std::async`, per-frame thread creation, busy wait, unbounded queue, or parallel AI/authority update is permitted.
- After warm-up, input sampling, frame-view publication, cache-hit paths, frame preparation, command merge, and ordinary rendering add zero heap allocations.
- Keep the material hard cap at 268,435,456 bytes. The existing transition peak is 261,010,720 bytes, leaving only 7,424,736 bytes; add zero texture bytes and cap new vertex/index buffers at 3,145,728 bytes.
- Do not adopt OpenGL 4.3, Compute Shader, SSBO, indirect GPU-driven gameplay, GPU readback, optical flow, AI frame generation, a render thread, extra full-resolution render targets, reduced visual quality, reduced monster density, or shorter effects.
- Every cache, parallel path, or GPU path must improve CPU-active P99 by at least 10% or reduce its measured stage by at least 0.50 ms without a representative-scene regression. Otherwise remove it or keep it disabled by default and record the result.
- Before every long build or 10,000-frame benchmark, record CPU, memory, disk, GPU, and top processes. Do not benchmark while another build, game, launcher, browser automation, cloud sync, or unrelated high-load process is active; never kill an unrelated process merely to improve a score.
- Build and benchmark serially. Use `CMAKE_BUILD_PARALLEL_LEVEL=1` for correctness builds; the game's frame-prep workers are tested only after compilation ends.
- Every created `.cpp`, test `.cpp`, and CMake guard is added to its owning `src/*/CMakeLists.txt` or `tests/*/CMakeLists.txt`; every new custom test body is declared and invoked by the owning `*_test_main.cpp` in the same task.

---

### Task 0: Instrument and freeze the pre-optimization square-room baseline

**Files:**
- Create: `src/platform/raylib/frame_profiler.hpp`
- Create: `src/platform/raylib/frame_profiler.cpp`
- Create: `src/platform/raylib/performance_scenario.hpp`
- Create: `src/platform/raylib/performance_scenario.cpp`
- Modify: `src/platform/raylib/host_launch_options.hpp`
- Modify: `src/platform/raylib/host_launch_options.cpp`
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `src/app/main.cpp`
- Create: `tests/platform/frame_profiler_tests.cpp`
- Create: `tests/platform/performance_scenario_tests.cpp`
- Modify: `tests/platform/host_launch_options_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`
- Create: `scripts/RunPerformanceBenchmark.ps1`
- Create: `docs/validation/240fps-baseline.md`

**Interfaces:**
- Produces a 16,384-sample fixed ring with main-thread stages `input`, `authority`, `checkpoint_capture`, `save_enqueue`, `save_result_apply`, `view_publish`, `cache`, `frame_prep`, `merge`, `draw_submit`, `end_drawing`, `limiter_wait`, `cpu_active`, and `wall_interval`; worker `encode`, `write`, `readback_decode`, and `verify` are recorded in a separate save-job channel and never added to main-thread CPU-active time.
- Produces average FPS, 1% low, P50/P95/P99/max, allocation count, draw commands/runs, iteration counts, worker wait, save queue/result state, and hard-stall count without per-frame formatting or allocation.
- Produces deterministic CLI scenarios with fixed seed, camera route, input trace, resident/visible-count target, warm-up/measured-frame counts, VSync, resolution, and CSV/JSON output paths.

- [ ] **Step 1: Write profiler, CLI, and registration RED tests**

Use synthetic nanosecond samples to prove percentile indices, 1% low (`1 / mean(slowest 1% intervals)`), ring wrap, warm-up exclusion, CPU-active versus wait/I/O separation, and zero hot-path allocation. Parse all performance flags through `src/app/main.cpp -> HostLaunchOptions -> raylib_host`; unknown/malformed flags fail clearly and never enter ordinary gameplay. Add each source/test/guard to the exact owning CMake list and test main before expecting RED to execute.

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
```

- [ ] **Step 2: Implement four reproducible Release scenarios**

Create normal crowded (300 total), normal horde (750 total), abyss horde (1,125 total), and abyss horde plus both active-skill peak effects and saturated visible loot. Each follows the same fixed camera route and proves active monsters never exceed 105/128. Accept `--perf-warmup=2000`, `--perf-frames=10000`, `--perf-vsync=0`, `--perf-width`, `--perf-height`, `--perf-seed`, `--perf-resident-target`, `--perf-csv`, and `--perf-json`.

- [ ] **Step 3: Add machine-load preflight**

Before launch, record processor, logical-core count, RAM, GPU/driver, display refresh, power mode, top ten CPU/working-set/I/O processes, existing `arpg_*`, compiler, linker, CMake, CTest, and Codex child processes. Refuse measurement when another game/build process is active; record unrelated load instead of terminating it.

- [ ] **Step 4: Capture and commit the immutable before numbers**

On the clean square-room completion commit, build Release serially and run 1920×1080 and 2560×1440 with VSync off for 2,000 warm-up plus 10,000 measured frames per scenario. This occurs before bounded-view, 240 Hz, cache, worker, deadline, or GPU changes, so it is the true before baseline. Run the 144 Hz VSync-on visual sample separately.

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
. ./scripts/Configure.ps1 -Preset windows-msvc-release -Fresh
cmake --build --preset windows-msvc-release --target arpg_game arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-release -R '^platform\.units$' --output-on-failure
./scripts/RunPerformanceBenchmark.ps1 -BuildDirectory out/build/windows-msvc-release
git add src/app/main.cpp src/platform/raylib tests/platform scripts/RunPerformanceBenchmark.ps1 docs/validation/240fps-baseline.md
git commit -m "perf: freeze square room presentation baseline"
```

---

### Task 1: Replace production full snapshots with bounded frame views

**Files:**
- Create: `src/dungeon/dungeon_frame_state.hpp`
- Create: `src/dungeon/dungeon_frame_state.cpp`
- Modify: `src/dungeon/dungeon_render_snapshot.hpp`
- Modify: `src/dungeon/dungeon_render_snapshot.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/platform/raylib/dungeon_runtime.hpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Create: `src/platform/raylib/presentation_view_storage.hpp`
- Create: `src/platform/raylib/presentation_view_storage.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Create: `tests/dungeon/dungeon_frame_state_tests.cpp`
- Modify: `tests/dungeon/dungeon_render_snapshot_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Create: `tests/platform/production_snapshot_source_guard_test.cmake`
- Create: `tests/platform/presentation_view_storage_tests.cpp`
- Create: `tests/platform/presentation_stack_guard_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces `DungeonFrameState`, a fixed-capacity control/HUD value containing session/tick/topology versions, room phase/progress, player summary, death/save gates, input-relevant UI counts, and no full inventory or full-room authority arrays.
- Consumes the square-room `WorldViewQuery` and extends its existing `DungeonRenderSnapshot` with complete render-version keys while preserving its 128-resident/512-projectile/128-hazard/128-environment and visible-drop bounds.
- Produces allocation-free out-parameter `DungeonSession::write_frame_state(DungeonFrameState&) const noexcept` and retains the existing `write_render_snapshot(const WorldViewQuery&, DungeonRenderSnapshot&) const noexcept` contract.
- Produces heap-owned `PresentationViewStorage` for previous/current frame states and render snapshots. The host owns one `std::unique_ptr`, stores no large view arrays automatically, and enforces computed resident bytes at or below 8 MiB plus a small host/handle `sizeof` gate.
- Retains `DungeonSession::snapshot()` only for tests, save/debug tooling, and explicit non-frame diagnostics.

- [ ] **Step 1: Write RED value/limit tests**

Require `DungeonFrameState` to exclude `ItemOwnershipState`, full room-monster state, and full authoritative drop arrays. Fill the room to all authority capacities and require `DungeonRenderSnapshot` to emit only camera-intersecting records in stable `(kind,ordinal)` order. Resident monster/projectile/hazard/environment capacity overflow is a fault; label-only loot overflow uses the approved stable rarity/distance/ordinal selection, raises a presentation diagnostic, and never deletes or marks an authoritative drop claimed.

- [ ] **Step 2: Add a production source guard**

The guards must fail while the production loop contains `DungeonSnapshot current/previous/presented`, `session->snapshot()`, an assignment of a complete `DungeonSnapshot`, or automatic/local `DungeonFrameState`, `DungeonRenderSnapshot`, or presentation-storage arrays. They may whitelist formal validation helpers and test-only translation units explicitly by path; they may not use a broad regex exclusion for `raylib_host.cpp`.

- [ ] **Step 3: Verify RED**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon|platform)\.units$|^performance\.(production_snapshot_source_guard|presentation_stack_guard)$' --output-on-failure
```

- [ ] **Step 4: Implement frame and render publications**

Write fields individually from session authority. Reuse caller-owned storage, publish monotonically increasing versions, and never copy `RoomMonsterField`, full drop authority, inventory storage, or save buffers. `write_render_snapshot` queries the existing resident/visible-set indices and must not scan 1,152 monsters or all authoritative drops.

- [ ] **Step 5: Convert the host and renderer**

Allocate `PresentationViewStorage` once after successful window/runtime initialization and store previous/current values only inside it; partial allocation failure cleans up and aborts renderer initialization explicitly rather than falling back to large stack values. Validation showcases may mutate a dedicated heap-backed render-view slot, never authority. Inventory opening requests its existing inventory-specific view once; closed inventory cannot force an inventory copy every frame. Remove production hot-loop `session->snapshot()` calls, including the descent range check and post-request refreshes. After initialization, publication/interpolation performs zero heap allocation.

- [ ] **Step 6: Prove semantic parity and commit**

Run the existing dungeon/platform unit suites plus Stage 10, 11c, 11d, 12, and 17 focused gates. Compare old test-only snapshot projections with the new views for 4,096 fixed states.

```powershell
cmake --build --preset windows-msvc-debug -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon|platform)\.units$|^performance\.(production_snapshot_source_guard|presentation_stack_guard)$|^(stage10\.|stage11c\.|stage11d\.|stage12\.material_|stage17\.)' --output-on-failure
```

```powershell
git add src/dungeon src/platform/raylib tests/dungeon tests/platform
git commit -m "refactor: publish bounded presentation views"
```

---

### Task 2: Add 240 Hz presentation, exact input latching, and interpolation

**Files:**
- Create: `src/platform/raylib/host_input_latch.hpp`
- Create: `src/platform/raylib/host_input_latch.cpp`
- Create: `src/platform/raylib/presentation_interpolation.hpp`
- Create: `src/platform/raylib/presentation_interpolation.cpp`
- Create: `src/platform/raylib/presentation_pacer.hpp`
- Create: `src/platform/raylib/presentation_pacer.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `src/core/fixed_step.hpp`
- Modify: `src/core/fixed_step.cpp`
- Modify: `src/platform/raylib/host_input.hpp`
- Modify: `src/platform/raylib/host_input.cpp`
- Modify: `src/platform/raylib/raylib_input.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/window_settings.hpp`
- Modify: `src/platform/raylib/window_settings.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `tests/core/fixed_step_tests.cpp`
- Modify: `tests/core/test_main.cpp`
- Create: `tests/platform/host_input_latch_tests.cpp`
- Create: `tests/platform/presentation_interpolation_tests.cpp`
- Create: `tests/platform/presentation_pacer_tests.cpp`
- Modify: `tests/platform/window_settings_tests.cpp`
- Modify: `tests/platform/input_latency_source_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces `HostInputLatch::sample(PhysicalKeySnapshot, InputGateState)`, `input_for_tick(bool first_tick)`, `consume_first_tick_edges()`, `on_focus_lost()`, and `rearm_after_release()`; `InputGateState` explicitly includes `save_committing` in addition to the existing gameplay gates.
- Produces `PresentationDiscontinuity` flags for room change, teleport, reload, death, continue, and entity-generation replacement.
- Produces interpolation functions that read previous/current render snapshots and `alpha` but cannot write authority.
- Produces `PresentationPacer::apply(window_ready,vsync_enabled)`, a transition-aware setting seam: VSync off applies the 240 FPS software target, VSync on removes that target and delegates pacing to the swap interval, and unchanged state performs no API call.

- [ ] **Step 1: Write exhaustive latch RED tests**

Cover zero-tick, one-tick, and multi-tick presentation frames; short taps between ticks; held/released states; WASD, J/K/L, E, Escape, inventory/passive controls, and numpad 1-5 skills. Each press/release edge reaches exactly the first eligible tick once, while held state reaches all ticks. Pause, inventory, death, recovery, focus loss, rebind capture, and `save_committing` consume or discard edges under their existing gates and never replay an old attack after reopening gameplay. Entering an exact save commit clears pending gameplay edges; input sampled while it is active cannot enter the gameplay queue; leaving it requires physical release/rearm before another attack edge is eligible.

- [ ] **Step 2: Write frame-cap and fixed-step RED tests**

Add a fake-backend seam proving initial off applies target 240 after a ready window; initial on delegates to VSync; runtime off→on removes the software target; on→off reapplies 240; preview cancel/rollback restores the prior pacing mode; repeated application of the same state emits no API call; and headless/unit code without a ready fake window emits zero real raylib calls. For identical 10-second input traces at 60/120/144/240 presentation Hz, require the same 600 authority ticks, combat hash, event order, skill duration, room progress, and save bytes.

- [ ] **Step 3: Implement the input latch before changing the cap**

Sample physical input on every presentation frame. Do not submit one-shot gameplay actions on a zero-tick frame; retain them until the first eligible authority tick. If a frame runs multiple catch-up ticks, consume edges after the first and pass held movement to every tick. Focus loss and entry into `save_committing` clear pending gameplay latches and require a physical release before rearming; while `save_committing` is true, no gameplay edge or held state is queued for later replay.

- [ ] **Step 4: Apply the presentation cap and background pacing**

After `InitWindow()` succeeds, route initial settings and every committed preview/rollback notification through `PresentationPacer`. VSync-off applies `SetTargetFPS(240)`; VSync-on applies the backend's no-software-limit state and uses the display swap interval. Calls occur only on an effective state transition, so runtime settings cannot leave an uncapped busy loop or a stale 240 limiter. Minimized/unfocused presentation skips render preparation and sleeps through the existing 60 Hz background authority cadence; measured minimized process CPU must remain at or below 2% on the validation machine.

- [ ] **Step 5: Complete interpolation and discontinuity snapping**

Interpolate player, camera, resident monsters, projectiles, hazards, and continuous visual transforms between authority ticks. Sprite animation frames, attacks, hit flashes, visibility, depth order, materials, HUD values, collisions, damage positions, and save positions use discrete current authority. Any discontinuity snaps previous to current for that entity/room.

- [ ] **Step 6: Run latency/determinism gates and commit**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_core_tests arpg_dungeon_tests arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(core|dungeon|platform)\.units$|input_latency' --output-on-failure
git add src/core src/platform/raylib tests/core tests/platform
git commit -m "feat: present and sample input at up to 240 fps"
```

---

### Task 3: Capture the bounded-view and 240 Hz comparison point

**Files:**
- Modify: `scripts/RunPerformanceBenchmark.ps1`
- Modify: `docs/validation/240fps-baseline.md`
- Review only: `src/platform/raylib/frame_profiler.*`
- Review only: `src/platform/raylib/performance_scenario.*`
- Review only: `src/platform/raylib/host_launch_options.*`

**Interfaces:**
- Consumes the byte-identical Task 0 instrumentation and scenario definitions.
- Produces an immutable after-Task-2 comparison beside the true square-room before baseline; it never relabels the after result as the before baseline.

- [ ] **Step 1: Prove instrumentation identity and semantic comparability**

Require the profiler/scenario/CLI source hashes and all scenario seeds, routes, input traces, resident targets, resolutions, warm-up, and measured-frame counts to match Task 0. If Task 1/2 required an instrumentation adjustment, apply the same instrumentation-only adjustment to a temporary clean worktree at the Task 0 commit and recapture both sides; never compare different probes.

- [ ] **Step 2: Capture post-view/240 numbers**

Repeat all Task 0 Release scenarios with VSync off at 1920×1080 and 2560×1440, plus the separate 144 Hz VSync-on visual sample. Record stage deltas, iteration counts, input latency, authority hashes, save bytes, and whether Task 1/2 alone improved or regressed each scenario.

- [ ] **Step 3: Commit the comparison evidence**

```powershell
git add scripts/RunPerformanceBenchmark.ps1 docs/validation/240fps-baseline.md
git commit -m "perf: compare bounded 240 hz presentation"
```

---

### Task 4: Add versioned caches and eliminate full-world frame scans

**Files:**
- Create: `src/dungeon/dungeon_render_versions.hpp`
- Create: `src/platform/raylib/frame_view_cache.hpp`
- Create: `src/platform/raylib/frame_view_cache.cpp`
- Create: `src/platform/raylib/visible_set_cache.hpp`
- Create: `src/platform/raylib/visible_set_cache.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/hud_view_model.cpp`
- Modify: `src/platform/raylib/ground_loot_view.cpp`
- Modify: `src/platform/raylib/material_loot_view.cpp`
- Modify: `src/platform/raylib/material_residency.cpp`
- Create: `tests/platform/frame_view_cache_tests.cpp`
- Create: `tests/platform/visible_set_cache_tests.cpp`
- Create: `tests/platform/frame_hot_path_source_guard_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces monotonic `DungeonRenderVersions { room, topology, combat, drop, hud, material, camera_cell, resolution }`.
- Produces fixed caches for HUD static layout, resident-monster base draw data, visible loot/environment sets, and material residency requests.
- Uses complete stable values plus versions as keys; no pointer address or uninitialized padding participates in equality/hash.

- [ ] **Step 1: Write cache hit/invalidation RED tests**

Four presentation frames between authority ticks must build stable monster/HUD/material data once. Room transition, death/continue, reload, resolution, ecology, equipped skill, loot change, and camera-cell change invalidate exactly their dependent caches. Interpolation alpha may update continuous transforms without invalidating topology/base material data.

- [ ] **Step 2: Add the hot-path source/operation guard**

Instrument iteration counts and fail if one presentation frame visits more than 128 monsters or scans all 1,152 field records. Fail if off-camera authoritative equipment/material/potion records are formatted, label-laid-out, or clamped to a screen edge. The source guard rejects production calls to legacy full snapshots and per-frame material-residency rebuild loops.

- [ ] **Step 3: Implement camera-cell and version-based caches**

Rebuild visible sets only when the camera cell span or corresponding authority version changes. Query the square-room `RoomDropSpatialIndex`, never all ground authority, and select overflowing visible loot by approved rarity priority, then squared distance, then stable ordinal without deleting authority. Cache static HUD geometry by resolution/font/language and dynamic values separately. Reuse an unchanged material request bitmask without calling load/unload logic.

- [ ] **Step 4: Measure the cache stage and enforce the benefit gate**

Repeat Task 3 scenarios. Keep each cache only if CPU-active P99 improves by at least 10% or its stage drops by at least 0.50 ms with no scenario regression above 3%. Record cache hit ratios and removed work, not just FPS.

- [ ] **Step 5: Run tests and commit**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon|platform)\.units$|performance\.frame_hot_path_source_guard' --output-on-failure
git add src/dungeon src/platform/raylib tests/platform docs/validation/240fps-baseline.md
git commit -m "perf: cache bounded visible presentation data"
```

---

### Task 5: Build a deterministic eight-lane frame-preparation pool

**Files:**
- Create: `src/core/execution_lane_budget.hpp`
- Create: `src/core/execution_lane_budget.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `src/persistence/save_commit_worker.hpp`
- Modify: `src/persistence/save_commit_worker.cpp`
- Create: `src/platform/raylib/frame_prep_types.hpp`
- Create: `src/platform/raylib/frame_prep_pool.hpp`
- Create: `src/platform/raylib/frame_prep_pool.cpp`
- Create: `src/platform/raylib/frame_prep_storage.hpp`
- Create: `src/platform/raylib/frame_prep_storage.cpp`
- Create: `src/platform/raylib/frame_command_builder.hpp`
- Create: `src/platform/raylib/frame_command_builder.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Create: `tests/platform/frame_prep_pool_tests.cpp`
- Create: `tests/platform/frame_prep_storage_tests.cpp`
- Create: `tests/platform/frame_command_builder_tests.cpp`
- Create: `tests/platform/frame_prep_raylib_api_guard_test.cmake`
- Create: `tests/platform/frame_prep_stack_guard_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`
- Create: `tests/core/execution_lane_budget_tests.cpp`
- Modify: `tests/core/CMakeLists.txt`
- Modify: `tests/core/test_main.cpp`

**Interfaces:**
- Produces heap-owned `FramePrepStorage` containing up to four aligned `FramePrepSlot`s with state `free/running/complete/cancel_requested`, immutable `FramePrepInput`, and exclusive fixed output. `raylib_host` owns it through `std::unique_ptr`; neither host, pool, worker, nor serial path embeds or declares a slot/command array automatically.
- Produces `kWorldDrawCommandCapacity=8192` and `kUiDrawCommandCapacity=2048`; compile-time formulas cover 128 monsters, 512 projectiles, 128 hazards, 128 environment objects, visible loot, doors/hole, player, active skills, feedback, debug, and HUD.
- Produces `FramePrepPool::start(worker_count)`, `submit(slot,generation)`, `cancel(generation)`, `try_collect(generation)`, and `stop_and_join()`.
- Keeps `FramePrepPool` pointer-only/small (`sizeof<=2048`) and hard-caps exact frame-prep heap storage at 56 MiB; together with Task 1 view storage, new CPU presentation storage is at most 64 MiB and is separate from texture/VBO budgets.
- Produces a seven-token `ExecutionLaneBudget` shared by the persistent frame workers and `SaveCommitWorker`; tokens are held only during actual work and released before sleeping.
- Requested frame lane count includes the main thread and accepts 1, 2, 4, or 8. With no save job, this yields 0, 1, 3, or 7 active frame workers; while the save worker owns one token, an 8-lane request yields main + at most six active frame workers, so total active execution never exceeds eight.

- [ ] **Step 1: Write queue/capacity/lifecycle RED tests**

Require bounded queue behavior, one in-flight generation, exact slot transitions, cancellation acknowledgement before reuse, partial allocation/thread-start cleanup, exception containment, idle condition-variable sleep, full join before renderer/window/audio teardown, and a whole-frame serial fallback on any failure. Initialization first attempts four heap slots; if that allocation fails cleanly it attempts one heap slot and runs serial, and if even one slot cannot be allocated renderer initialization fails explicitly without a large stack fallback. Serial preparation always reuses slot zero. Under a concurrent maximum V9 encode/write/readback job, assert `main + executing frame workers + executing save worker <= 8`, no starvation, no busy wait, and bounded transaction completion. Capacity tests enumerate every worst-case command multiplier and require the sum to fit 8192/2048; changing a renderer multiplier without updating the proof must fail compilation or the capacity test.

- [ ] **Step 2: Write deterministic lane-count RED tests**

For identical input and alpha, 1/2/4/8 requested lanes and the 8-requested/save-active seven-frame-lane case must emit byte-equal command counts, stable keys, compatible signatures, depth ordering, and merged command bytes. Authority hashes/events/save bytes must also match over a 60,000-tick trace. Task completion order may vary; output order may not.

- [ ] **Step 3: Implement immutable slicing and stable merge**

Partition monsters, projectiles/hazards, environment, loot/effects, and HUD into fixed index ranges. Each lane writes only its assigned output slice. The main thread may execute one slice, then merges by the renderer's complete depth key with `(entity_kind, stable_ordinal, generation, subcommand_kind, emission_index)` as tie breakers. Never globally reorder transparent objects for material convenience.

- [ ] **Step 4: Enforce the raylib/OpenGL boundary**

The API guard rejects raylib, rlgl, OpenGL, Win32 input, audio, RNG, save, `CombatWorld`, or `DungeonSession` mutation calls in worker translation units. The stack guard rejects automatic/local `FramePrepSlot`, command-buffer arrays, or aggregate `FramePrepStorage` in production. Workers cannot borrow pointers into mutable renderer/material/session objects.

- [ ] **Step 5: Add production worker selection and low-work serial cutoff**

Start at most `min(7, max(0, hardware_concurrency-1))` frame workers, but acquire shared lane-budget tokens per generation; `hardware_concurrency()==0`, token shortage, thread creation failure, minimized/background mode, and workloads below the measured cutoff use fewer lanes or the serial builder. Workers stay resident and sleep when idle. Record requested/granted lanes plus wake, work, merge, wait, and save-worker overlap times.

- [ ] **Step 6: Enforce the benefit gate and commit**

Measure 1/2/4/8 requested frame lanes in all four scenarios both without saving and during repeated maximum-payload background saves. Select the fastest stable request no greater than eight for the default; if 8 loses to 4 by more than 3%, keep adaptive 4 for that workload while retaining deterministic 8-lane support. Parallel preparation must meet the global 10%/0.50 ms benefit gate and total active lanes must never exceed eight.

```powershell
cmake --build --preset windows-msvc-debug --target arpg_core_tests arpg_persistence_tests arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(core|persistence|platform)\.units$|^performance\.frame_prep_(raylib_api|stack)_guard$' --output-on-failure
git add src/core src/persistence/save_commit_worker.* src/platform/raylib tests/core tests/platform docs/validation/240fps-baseline.md
git commit -m "perf: prepare bounded frames across eight cpu lanes"
```

---

### Task 6: Add the bounded pre-stall presentation guard

**Files:**
- Create: `src/platform/raylib/frame_deadline_guard.hpp`
- Create: `src/platform/raylib/frame_deadline_guard.cpp`
- Create: `src/platform/raylib/presentation_frame_reuse.hpp`
- Create: `src/platform/raylib/presentation_frame_reuse.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `src/platform/raylib/frame_prep_types.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Create: `tests/platform/frame_deadline_guard_tests.cpp`
- Create: `tests/platform/presentation_frame_reuse_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces a 120-frame fixed-history `FrameDeadlineGuard`.
- Enters emergency mode after two consecutive predicted CPU-active frames at or above 3.50 ms, or when current frame preparation reaches 3.20 ms without a complete publication.
- Exits after 30 consecutive complete frames predicted below 3.00 ms.
- Reuses only two complete compatible command generations for at most two presentation frames and at most 8.33 ms total.

- [ ] **Step 1: Write transition/threshold RED tests**

Cover entry/exit hysteresis, history wrap, prediction inputs, exact 3.50/3.20/3.00 boundaries, two-frame/8.33 ms ceiling, and hard-stall reporting. One-frame spikes below the thresholds cannot enter emergency mode.

- [ ] **Step 2: Write stable-identity compatibility RED tests**

Pair commands only by `(room_generation,entity_kind,stable_ordinal,entity_generation,subcommand_kind,emission_index)`. Compatibility also requires topology, material, source rectangle, animation frame, blend/depth mode, visibility, color/alpha, and discrete-effect versions to match. Only world/camera position and explicitly continuous rotation may interpolate or extrapolate; HUD, damage numbers, hit flashes, skill frames, visibility, and authority values freeze at the latest complete frame.

- [ ] **Step 3: Implement bounded reuse with full-frame fallback**

Room transition, death/continue, reload, teleport, material generation, skill/attack/hit topology, command-set change, missing history, cancellation, or capacity fault disables reuse. Late worker output publishes only when generation and signature still match. After the limit, display the latest complete frame and record a hard miss; never keep extrapolating.

- [ ] **Step 4: Prove what the mechanism cannot hide**

Inject stalls independently into frame preparation, authority tick, main-thread checkpoint capture, save enqueue/result application, worker encode/write/readback/verify, main-thread merge, draw submission, `EndDrawing`, and GPU completion. Only pure CPU preparation spikes may produce reused frames. Save/disk stalls remain separate profiler evidence, and transaction presentation may continue under the square-room committing contract, but neither case is counted as successful frame insertion. All other stalls are reported as unhideable.

- [ ] **Step 5: Measure overhead/false triggers and commit**

Across stable 10,000-frame runs, false emergency entry must stay below 1% and normal-path CPU-active overhead below 0.10 ms. Authority/event/save hashes must match guard-disabled runs.

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
git add src/platform/raylib tests/platform docs/validation/240fps-baseline.md
git commit -m "perf: mask bounded frame preparation spikes"
```

---

### Task 7: Move only measured draw submission work to OpenGL 3.3 batching

**Files:**
- Create: `src/platform/raylib/world_gpu_batch.hpp`
- Create: `src/platform/raylib/world_gpu_batch.cpp`
- Create: `src/platform/raylib/room_gpu_geometry.hpp`
- Create: `src/platform/raylib/room_gpu_geometry.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/material_pack.cpp`
- Create: `tests/platform/world_gpu_batch_tests.cpp`
- Create: `tests/platform/gpu_feature_level_source_guard_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces main-thread-only OpenGL 3.3 static room chunk geometry and adjacent compatible `MaterialDrawRun`s.
- Produces a complete existing `DrawTexturePro`/material fallback for shader/VBO/init/capacity/version failures.
- Adds no textures, render targets, compute programs, readbacks, or authority-visible state.

- [ ] **Step 1: Use current post-Task-6 metrics to select the migration**

Re-run the Task 0 scenarios on the exact Task 6 commit, then compare current CPU time for environment tile/prop command construction, draw submission, material state changes, and `EndDrawing`. Implement static environment chunk VBOs only if environment construction/submission costs at least 0.50 ms or the prototype improves CPU-active P99 by 10%. Consider dynamic adjacent sprite runs only after static environment batching passes and draw submission remains a measured hotspot. If neither gate passes, record that GPU migration is not worthwhile and do not merge dormant complexity.

- [ ] **Step 2: Write order/material/fallback RED tests**

Require identical projected vertices, atlas UVs, material channels, foot anchors, depth order, transparency result, door/hole state, and camera culling between scalar and batched plans. Only consecutive commands with identical shader, color/material textures, blend/depth state, and compatible parameters may share a run. Shader/VBO failure and capacity overflow must render the complete scalar frame.

- [ ] **Step 3: Build static room chunks with existing atlases**

Generate fixed world-space tile/prop vertices once per room/environment version, split into camera-cullable chunks, and upload/update them only on the main thread. Keep total new vertex/index storage at or below 3,145,728 bytes. Room changes retire buffers only after no published frame references their generation.

- [ ] **Step 4: Add adjacent state caching without global sorting**

Walk the already depth-sorted command stream, combine adjacent compatible runs, and skip redundant shader/texture/uniform state changes. Do not move an opaque or transparent command across another depth item to grow a batch. Preserve current material shader math and active-skill/monster atlas timelines.

- [ ] **Step 5: Enforce feature and memory guards**

Reject `#version 430`, compute shaders, SSBOs, indirect draw gameplay, OpenGL readback, and raylib/OpenGL calls outside the main-thread files. Re-run Stage 12 residency and require transition peak no greater than 261,010,720 texture bytes, hard cap 268,435,456 bytes, zero new texture bytes, and VBO/index total within 3,145,728 bytes.

- [ ] **Step 6: A/B benchmark, keep only profitable paths, and commit**

Run scalar and batched modes over the same 10,000-frame traces. Keep each path only when it meets the global benefit gate and does not alter pixel/evidence thresholds or regress another scenario above 3%.

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_stage12_material_formal -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$|^performance\.gpu_feature_level_source_guard$|^stage12\.material_' --output-on-failure
git add src/platform/raylib tests/platform docs/validation/240fps-baseline.md
git commit -m "perf: batch measured opengl 3.3 world draws"
```

If no GPU path qualifies, replace that commit with `perf: record gpu migration as not worthwhile` containing only reviewed measurements and no dormant production implementation.

---

### Task 8: Run final correctness, determinism, visual, resource, and performance gates

**Files:**
- Create: `docs/validation/visible-set-240fps-eight-core.md`
- Create: `docs/validation/evidence/visible-set-240fps-eight-core/`
- Modify only for deterministic scenario coverage: `scripts/RunPerformanceBenchmark.ps1`

**Interfaces:**
- Consumes Tasks 0-7.
- Produces a clean, independently reviewed, pushed branch with exact functional and performance evidence.

- [ ] **Step 1: Run Debug correctness suites serially**

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
. ./scripts/Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(core|combat|dungeon|persistence|platform)\.units$|performance\.|input_latency' --output-on-failure
```

- [ ] **Step 2: Run formal material/skill/launcher compatibility gates**

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(launcher\.|stage10\.|stage11c\.|stage11d\.|stage12\.material_|stage16\.|stage17\.)' --output-on-failure
```

Require unchanged material/skill frames, Chinese text clarity, loot labels, door/hole visuals, launcher behavior, and exact 25%/100% room semantics.

- [ ] **Step 3: Run lane/presentation determinism matrix**

Replay the same 60,000 authority ticks at 60/120/144/240 presentation Hz with 1/2/4/8 requested frame lanes, save worker idle/maximum-background-save/transaction-wait schedules, deadline guard on/off, and scalar/accepted GPU path. Require identical authority hashes, events, RNG consumption, drop/claim state, V9 save bytes, death state, and transition target. Assert no sample exceeds eight simultaneous execution lanes. Render commands are byte-equal for the same snapshot/alpha inputs regardless of requested or temporarily granted lane count.

- [ ] **Step 4: Run final Release benchmarks with clean-machine preflight**

At 1920×1080 and 2560×1440, VSync off, collect at least 2,000 warm-up plus 10,000 measured frames per fixed scenario. Targets on the validation machine are average FPS at least 230, 1% low at least 200 FPS, and CPU-active P99 at most 4.17 ms. For the total-population scaling gate, use a dedicated synthetic pair with identical camera route, exact per-frame resident monster ordinals/count, exact visible loot/environment command counts, effects, input, and materials; only hidden authoritative total differs (300 versus 1,125). The 1,125 member may cost no more than 10% per frame, and no frame path may visit all 1,125 monsters or all ground records. Keep the real density-affix scenarios as separate quality/load results rather than claiming they are equal-density. Record any hardware-limited miss as a miss, not a pass.

- [ ] **Step 5: Run real-window quality and resource acceptance**

At 144 Hz VSync on, verify smooth player/camera motion, all controls, no repeated/lost input, no animation/material regressions, and no visible correction beyond two frames after emergency reuse. Verify minimized CPU at or below 2%, worker idle sleep, zero post-warm-up hot-path allocations, no orphan worker/game/build process, view-storage heap at or below 8 MiB, frame-prep heap at or below 56 MiB, no production large automatic buffers, transition texture peak no greater than 261,010,720 bytes, and new VBO/index storage no greater than 3,145,728 bytes.

- [ ] **Step 6: Write evidence, remove failed optimizations, and commit**

Record commit, build, CPU/GPU/driver, resolution/refresh/VSync, power mode, scenario seed, total/resident counts, lanes/workers, average/1% low, stage P95/P99/max, allocations, draw commands/runs, cache ratios, guard entries/hard stalls, exact `sizeof`/resident bytes for view/frame/save storage, process working set, texture/VBO memory, and before/after deltas. Remove or default-disable every optimization that missed its benefit gate before recording final numbers.

```powershell
git add docs/validation/visible-set-240fps-eight-core.md docs/validation/evidence/visible-set-240fps-eight-core scripts/RunPerformanceBenchmark.ps1
git commit -m "test: validate visible-set 240 fps performance"
```

- [ ] **Step 7: Independent review and push**

Require one specification review, one determinism/concurrency review, and one code-quality review. Confirm the control checkout separately, then run in the integration worktree:

```powershell
git diff --check
git status --short --branch --untracked-files=all
```

Require the integration-worktree status output to contain only the branch header before pushing `codex/integration-large-square-rooms`. Do not merge to `main` until all three reviews are clean and the measured targets are reported truthfully.
