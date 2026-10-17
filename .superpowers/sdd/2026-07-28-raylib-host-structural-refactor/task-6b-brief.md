# Task 6B brief — extract Stage17 capture/report lifecycle

## Scope and baseline

- Work only in `E:/game/.worktrees/whole-game-refactor-2026-07` on branch `codex/whole-game-refactor-2026-07`.
- Task BASE is commit `33d86816d9f5e31e6fbb5d73f9091d2b07135d8a`.
- This is a pure structural extraction. Do not change gameplay, input, timing, RNG, event order, render order, screenshot timing/name/path priority, summary fields/text/order, completion predicates, persistence, colors, layout, or audio.
- Touch exactly these seven task files and no others:
  1. create `src/platform/raylib/host_validation_stage17_report.cpp`
  2. modify `src/platform/raylib/host_validation_stage17.hpp`
  3. modify `src/platform/raylib/raylib_host.cpp`
  4. modify `src/platform/raylib/CMakeLists.txt`
  5. modify `tests/platform/host_input_source_test.cmake`
  6. modify `tests/platform/input_latency_source_test.cmake`
  7. modify `tests/platform/host_validation_sequence_guard_test.cmake`
- Never stage or modify the existing untracked `tests/platform/__pycache__/active_skill_material_asset_pipeline_tests.cpython-313.pyc`.

## Required public interface

Declare in `host_validation_stage17.hpp` and define in the new report source:

```cpp
std::optional<std::string> stage17_capture_path(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&) noexcept;
void mark_stage17_capture_complete(Stage17SkillStonesValidationState&) noexcept;
bool stage17_validation_complete(const RaylibHostConfig&,
    const Stage17SkillStonesValidationState&) noexcept;
void write_stage17_validation_summary(const RaylibHostConfig&,
    const Stage17SkillStonesValidationState&) noexcept;
```

The header must include only the standard types it exposes (`<optional>`, `<string>` in addition to existing includes) and must not expose raylib types or include `<raylib.h>`.

## Exact production move

Move the complete BASE block formerly at `raylib_host.cpp:365-672` into `host_validation_stage17_report.cpp` under `arpg::platform::host_validation`:

- private helpers: `stage17_draw_runtime_valid`, `stage17_capture_name`, `stage17_skill_name`, `write_stage17_loadout`, `stage17_support_none_count`;
- public definitions: the four APIs above.

Keep the moved function bodies semantically and textually identical apart from required namespace qualification/indentation and includes. Preserve every screenshot basename, summary field and field order, the exact `result=pass/fail` predicate, and the `catch (...)` plus warning behavior. Register the new source exactly once in `arpg_raylib`, adjacent to the Stage17 runtime source.

## Host lifecycle that must remain in `raylib_host.cpp`

- `present_frame_and_maybe_capture()` remains the sole owner of `EndDrawing()`, `LoadImageFromScreen()`, and `ExportImage()`.
- Keep the Stage17 capture-path request before the existing competing screenshot paths and retain their current priority.
- Keep the normal presented-frame chain in this order: call the presentation/capture function, inspect its result, then call `mark_stage17_capture_complete()` only for a requested and successful Stage17 capture.
- After the loop, keep the `clean_shutdown_exact_ready` state assignment before the summary calls.
- Keep summary order `11B -> 11C -> 11D -> 17`.
- The member reference needed for the host assignment and the four API call sites are expected to remain in host. The basename literals, definitions/completion predicate, summary field literal `clean_shutdown_exact_ready=`, and Stage17 summary result token must be owned only by the report source.

## TDD guard contract

Before creating production implementation, modify the three existing guard tests so at least one named guard fails specifically because `host_validation_stage17_report.cpp` is missing or ownership has not moved. Run that one guard directly and record the expected RED.

After implementation, the guards must:

- read header, runtime, report, host, and CMake as appropriate;
- prove each of the four public definitions exists exactly once in the report source, is declared exactly once in the header, and no definition remains in host/runtime;
- prove all five private helpers are owned exactly once by the report source and not exposed by the header;
- prove the report source alone owns the basename literals, completion implementation, summary field literal, and Stage17 summary result/output tokens;
- separately bind the real `run_raylib_host` executable surface and retain the actual capture lifecycle and post-loop summary ordering;
- require the report source to be registered exactly once as a real top-level `arpg_raylib` source;
- reject comments, strings, uncalled lambdas, cross-function/cross-scope fragments, inactive preprocessor text, early-return/unreachable evidence, and CMake comment/quoted/conditional decoys using the existing sanitizers/helpers rather than a narrowed raw-text scan.

Do not remove or weaken any existing Stage17 runtime/input/latency/sequence assertion or mutation self-test.

## Verification and report contract

Before any build, verify no duplicate `cmake`, `ctest`, `ninja`, `cl`, or `link` process is active. Run serially:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_input_source|platform\.input_latency_source|platform\.host_validation_sequence|stage17\.)' --output-on-failure -j1
git diff --check
```

Also compare all nine moved BASE definitions and the retained Host capture/summary lifecycle against BASE `33d8681`; report exact counts and any normalization used. Do not commit. Write a complete implementation/test/self-review report to:

`.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task-6b-report.md`

Return status (`DONE`, `DONE_WITH_CONCERNS`, `NEEDS_CONTEXT`, or `BLOCKED`), one-line verification summary, and concerns only.
