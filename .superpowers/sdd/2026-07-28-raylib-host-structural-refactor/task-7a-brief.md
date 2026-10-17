# Task 7A brief — encapsulate Host validation input and event observation

## Scope and baseline

- Work only in `E:/game/.worktrees/whole-game-refactor-2026-07` on branch
  `codex/whole-game-refactor-2026-07`.
- Task BASE is commit `35cda8816fd3615f312e902671dfd3678c981625`.
- This is a structural refactor only. Preserve gameplay, key bindings, injected
  edges, rearm behavior, death/continue behavior, fixed-step order, event
  order, snapshot cadence, inventory behavior, rendering, capture timing,
  summary text/order, persistence, RNG, and audio exactly.
- Touch only these seventeen task files. The final guard files are an
  explicit correction to the plan's file list: both are required by the
  prescribed selector and currently require the old direct Host injectors, so
  they must follow the same invariant into the facade/runtime rather than be
  weakened or left inevitably red.
  1. create `src/platform/raylib/host_validation.hpp`
  2. create `src/platform/raylib/host_validation_state.hpp`
  3. create `src/platform/raylib/host_validation_runtime.cpp`
  4. modify `src/platform/raylib/host_validation_stage10_11.hpp`
  5. modify `src/platform/raylib/host_validation_stage11b.hpp`
  6. modify `src/platform/raylib/host_validation_stage11c.hpp`
  7. modify `src/platform/raylib/host_validation_stage11d.hpp`
  8. modify `src/platform/raylib/host_validation_stage17.hpp`
  9. modify `src/platform/raylib/raylib_host.cpp`
  10. modify `src/platform/raylib/CMakeLists.txt`
  11. create `tests/platform/host_validation_source_test.cmake`
  12. modify `tests/platform/host_validation_sequence_guard_test.cmake`
  13. modify `tests/platform/CMakeLists.txt`
  14. modify `tests/platform/host_input_source_test.cmake`
  15. modify `tests/platform/input_latency_source_test.cmake`
  16. modify `tests/platform/stage11b_settings_evidence_guard_test.cmake`
  17. modify `tests/platform/stage11b_settings_evidence_guard_self_test.cmake`
- Do not stage or commit. Do not modify the tracked progress ledger. Write the
  implementation report only to the ignored task report path named below.

## Required opaque facade

Create the raylib-free public facade in `host_validation.hpp`:

```cpp
class HostValidationRuntime final {
public:
    static std::unique_ptr<HostValidationRuntime> create(
        const RaylibHostConfig&,
        settings::SettingsLoadStatus) noexcept;
    ~HostValidationRuntime();
    void set_render_readiness(bool cjk_font_ready,
        bool active_skill_atlases_ready) noexcept;
    PhysicalKeySnapshot inject_physical_edges(
        PhysicalKeySnapshot, const settings::SettingsData&,
        const dungeon::DungeonSnapshot&,
        bool gameplay_rearm_required) noexcept;
    void observe_combat_event(const combat::CombatEvent&) noexcept;
    void observe_snapshot(const dungeon::DungeonSnapshot&) noexcept;
    void observe_inventory(const InventoryRenderer&,
        const dungeon::DungeonSnapshot&) noexcept;
    void observe_submitted_actions(const SubmittedFrameActions&) noexcept;
private:
    struct Impl;
    friend struct HostValidationStateAccess;
    explicit HostValidationRuntime(std::unique_ptr<Impl>) noexcept;
    std::unique_ptr<Impl> impl_;
};
```

The public header may forward-declare required types, include `<memory>`, and
must not include `host_validation_state.hpp`, any Stage header, or raylib.

## State ownership, allocations, and temporary transition shim

- Keep the six BASE state definitions in their existing Stage headers. This
  card changes ownership of the state instances, not ownership of the Stage
  type declarations; moving the declarations would break unrelated Stage
  evidence guards scheduled for the final 7C boundary card. The internal
  `host_validation_state.hpp` includes the five existing Stage headers and
  defines only the six-state aggregate plus the temporary access shim. Do not
  duplicate a state definition and do not edit Stage `.cpp` files.
- Remove `HostValidationStates` from `raylib_host.cpp`. `Impl` owns exactly one
  instance of each of the six states, one non-owning pointer/reference to the
  existing `RaylibHostConfig`, and a cached Stage11D output snapshot used by
  the Host consumers that intentionally precede Stage17 injection.
- `create(config, load_status)` performs exactly two task-owned heap
  allocations: one `Impl` and one facade, both with `new (std::nothrow)` and
  `std::unique_ptr`. Both allocations and their failure check occur before
  `InitWindow()`. Any failure returns null and the Host maps it to the existing
  `HostExitCode::save_initialization_failed`.
- `create` initializes Stage11B `load_status`. After renderer resources are
  initialized, the Host calls allocation-free `set_render_readiness()` exactly
  once to assign Stage11C font readiness and Stage17 atlas readiness.
- Task 7B/7C still require direct access to the same six state objects. Define
  a narrow internal `HostValidationStateAccess` friend shim in
  `host_validation_state.hpp` with static reference accessors for those exact
  six objects and a const accessor for the cached death-input snapshot. It must
  not copy or own state, allocate, appear in `host_validation.hpp` beyond the
  friend declaration, or be used outside `raylib_host.cpp` and
  `host_validation_runtime.cpp`. Record it as temporary debt to be removed by
  7B/7C; never keep a second state copy.

## Exact input and observation behavior

- `inject_physical_edges()` must compose exactly:
  `11B -> 11C -> 11D -> set Stage17 suspend_injection -> 17`.
- Host order must remain:
  `sample_physical_keys -> read gameplay_rearm_required -> facade inject ->
  gameplay_controls_physically_released/ack -> map_host_frame_input`.
- Cache the exact Stage11D output before Stage17 injection. Preserve the BASE
  split: gameplay rearm/released acknowledgement and `map_host_frame_input`
  consume the final Stage17 output, while `host_death_input_gate`, pause-menu
  mouse hit testing, focus state, and `pause_input_from_snapshot` continue to
  consume the cached pre-Stage17 snapshot. This prevents Stage17 inventory
  rebinding/injection from changing death-continue or pause-menu behavior. Add
  source-guard assertions for both sides of this boundary.
- Replace only these Stage17 observation hooks with facade calls:
  combat event, snapshot, inventory, submitted actions. Do not move
  `observe_stage17_draw_runtime`; it belongs to Task 7C.
- `observe_submitted_actions()` first performs the existing Stage17 observer,
  then preserves the exact Stage11B rebound logic: at injected frame 28 set
  `old_attack_checked` then add `combat[0]` to old count; at frame 29 add it to
  new count. Host must no longer read `submitted_actions.combat[0]` or update
  those three Stage11B fields directly.
- Change `drain_events()` only so its final validation argument is one
  `HostValidationRuntime*`. Its first combat-event consumer is the facade,
  followed by renderer, feedback, and audio in the existing order. Preserve
  all six call sites and all dungeon-event handling/clearing.
- Preserve exactly three Stage17 snapshot observation points: initial snapshot
  before initial drain; frame-top after snapshot/drain; fixed-tick snapshot
  before Stage11D claim observation and drain. Do not add observations beside
  recovery, inventory, reset, or death snapshots.
- Preserve the inventory observer after inventory input/transaction handling
  and before pause-menu updating. Call it every frame as before.

## Explicit 7B/7C exclusions

Do not encapsulate or change Stage10/11 fixed-step input/reached/continue,
Stage11B fixed-tick/resume/pause fields, Stage11D abyss claim or presented
semantics, Stage11C HUD/debug/capture fields, Stage17 draw-runtime observation,
any validation completion/capture path, clean-shutdown field, or the four
summary calls. They remain on the internal transition shim for the next cards.

## TDD source-guard contract

Before production files exist, create/register
`platform.host_validation_source` and run it to obtain the precise RED:

```text
Host validation runtime target is missing: .../host_validation.hpp
```

The new guard and the updated sequence guard must, using the shared lexical,
function-boundary, brace-depth, inactive-preprocessor, and CMake-registration
helpers:

- require all three new production files and exactly one real top-level direct
  registration of `host_validation_runtime.cpp` in `arpg_raylib`;
- prove `host_validation.hpp` is opaque and owns only the facade/PImpl API;
- reject any `host_validation_*`, `host_validation::`, or
  `HostValidationRuntime` dependency in `core`, `combat`, `dungeon`, or
  `persistence` sources;
- bind the real `run_raylib_host()`/`drain_events()` executable surfaces and
  reject direct Host calls to the four old injectors and four migrated Stage17
  observers, plus direct Host 11B submitted-action counting;
- require the real facade calls, input/rearm/map order, cached Stage11D death
  gate input, the six `drain_events` runtime-pointer call sites, the three
  snapshot observations, and the inventory/submitted-action locations;
- keep all old Stage17 draw/capture/report and 7B/7C checks intact;
- reject comment/string/raw-string, `#if 0`, lambda/cross-function/cross-scope,
  early-return/unreachable, wrong-brace-depth, duplicate, and CMake decoys.

Do not weaken or delete existing sequence, input-latency, Host-input, Stage11B,
Stage17, or mutation assertions. Keep legacy `HOST_OVERRIDE` fixtures reaching
their original targeted diagnostics.

Update `host_input_source_test.cmake` and `input_latency_source_test.cmake`
only where their existing assertions hard-code the four injectors or the
pre-Stage17 death input as direct `raylib_host.cpp` text. Move those exact
ownership/order checks onto `host_validation_runtime.cpp`, require the Host
facade call and cached Stage11D death-input accessor, and retain all existing
comment/string/inactive/lambda/cross-scope and ordering protections. Do not
remove a mutation, reduce a call count, or relax a diagnostic merely to pass.

Update the Stage11B settings evidence guard and its self-test because the
prescribed Task 7A selector runs `stage11b.*` and the existing evidence guard
binds the rebound submitted-action counters directly in `raylib_host.cpp`.
Move that exact evidence ownership to `HostValidationRuntime::observe_submitted_actions`,
preserve Stage17-observer-before-11B-count order and frame-28/frame-29 field
semantics, and retarget every related negative mutation to the actual owning
runtime source. All other Stage11B injection/completion/hash/summary evidence
remains in its existing Stage source and must stay unchanged.

## Verification and report

Before every build/test, confirm there is no duplicate `cmake`, `ctest`,
`ninja`, `cl`, or `link` chain. Use the repository toolchain and run serially:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_(source|sequence)|platform\.host_input_source|platform\.input_latency_source|stage11b\.|stage17\.)' --output-on-failure -j1
git diff --check
```

Compare moved state definitions, input composition, all hook call counts/order,
and initialization/destruction order against BASE `35cda88`. Write evidence and
concerns to:

`.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task-7a-report.md`

Return `DONE`, `DONE_WITH_CONCERNS`, `NEEDS_CONTEXT`, or `BLOCKED`; do not
stage or commit.
