# Task 3 Report: Stage11B Settings Validation Extraction

## Scope

- BASE: `22e73e4` (`chore: record task 2 review completion`).
- Created `host_validation_stage11b.hpp/.cpp` in the named
  `arpg::platform::host_validation` namespace and registered the source in
  `arpg_raylib`.
- Moved the complete Stage11B state, deterministic physical-edge injection,
  completion predicate, snapshot hash, and summary writer out of
  `raylib_host.cpp` without changing their state fields or algorithm bodies.
- The host continues to initialize `load_status` from the same `loaded.status`,
  records pause tick/hash values at the original points, and retains the
  rebind-frame observations, input-chain position, fixed-tick path, and
  `EndDrawing()` capture surface.

## TDD Evidence

After retargeting the guards and before adding the Stage production files:

- `stage11b_architecture_guard_test.cmake` failed because
  `host_validation_stage11b.hpp` was missing.
- `stage11b_settings_evidence_guard_test.cmake` failed because
  `host_validation_stage11b.cpp` was missing.
- `host_validation_sequence_guard_test.cmake` failed because
  `host_validation_stage11b.hpp` was missing.

Those failures exercise the required new ownership boundary rather than the
former in-host implementation.  After implementation, the architecture guard,
evidence guard, sequence guard, and the Stage-source mutations all pass.

## Verification

- Required MSVC targets were built with `VsDevCmd`,
  `CMAKE_BUILD_PARALLEL_LEVEL=1`, and `-j1`. The serial Ninja log records
  links for `arpg_game.exe`, `arpg_platform_tests.exe`, and
  `arpg_stage11b_settings_formal.exe`.
- Required focused selector passed 6/6 in 14.70 seconds:
  `platform.units`, both host-validation sequence tests, the Stage11B
  architecture guard, the evidence guard, and its mutation self-test.
- `git diff --check` passed before the formal chain.
- The formal Step 4 selector did not pass: `stage11b.settings_formal` failed
  after 14.68 seconds and its dependent
  `stage11b.settings_evidence_validator` was not run. The generated report
  shows the Stage11B observations themselves are present (rebound new attack
  accepted, pause tick/hash frozen, recovery notice visible), but save-file
  fingerprints changed and the short-circuited formal harness never ran its
  swap scenario. This task did not modify persistence or the formal harness;
  that result is recorded as an unresolved failure, not claimed as a proven
  pre-existing baseline failure.

## Build Note

The first compile exposed two direct include requirements in the new isolated
source: `SettingsLoadStatus` is defined by `settings_store.hpp`, and the
by-value `PhysicalKeySnapshot` is defined by `host_input.hpp`. Both were added
as minimal direct dependencies before the successful target links.

## Review

- Guard ownership is split: Stage algorithms, summary fields, and injected
  keys are scanned in the Stage source; capture order, pause capture,
  fixed-tick, and physical input chain remain scanned in the host.
- The evidence self-test mutates both a Stage key route and a Stage summary
  field in copies of the actual Stage source; host mutations continue to copy
  the host source.
- No known P0/P1/P2 issue was found in the Task 3 production or guard diff.
