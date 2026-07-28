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
- The original formal Step 4 selector did not pass: `stage11b.settings_formal`
  failed after 14.68 seconds and its dependent
  `stage11b.settings_evidence_validator` was not run. The first failed
  predicate is the pause resume check (`resume_tick_before=1`,
  `resume_tick_after=5`, expected +1); because the harness chains assertions
  with `ok = ok &&`, it then short-circuits its later settings/swap children.
  The save-file fingerprint comparison occurs after that short circuit and was
  not the causal failure. This task did not modify persistence or the formal
  harness; BASE comparison is required before attributing the failure.

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

## Review Fix Round 1

- Reproduced Step 4 against detached BASE `22e73e4` with the same built
  executable configuration and 101 staged assets. Two BASE runs failed at the
  same pause-resume predicate, while the observed after tick varied from 4 to
  5 (`resume_tick_before=1`). HEAD observed 5. This is therefore recorded as
  the same failing predicate with variable observed tick count; the dependent
  validator remained not run. No formal harness, persistence, or gameplay
  code was changed.
- Stage algorithms are now extracted with the Task 2 splice-aware scanner and
  each required function is scanned independently. Mutation checks remove a
  real Stage token while leaving comment decoys; they reject with the
  Stage-specific missing-token diagnosis. The summary token includes its
  expression boundary, avoiding prefix matches in renamed identifiers.
- The host sequence guard extracts `run_raylib_host`, balances the actual
  `while (!exit_requested)` braces, and scans only sanitized code. Its direct
  scope check rejects a complete input declaration placed in a nested lambda,
  a complete declaration outside the loop, and a complete Stage11B summary
  call placed in a lambda. It also rejects a spliced comment decoy.
- The Stage11B private header now participates in the forbidden-dependency
  scan; its self-test injects a real `persistence/save_store.hpp` include.
  The unused `stable_key_raylib.hpp` include was removed from the host.
- Final verification: serial MSVC build of `arpg_game`,
  `arpg_platform_tests`, and `arpg_stage11b_settings_formal` passed in
  187.2 seconds. The focused six-test CTest selector listed exactly six tests
  and passed 6/6 in 581.18 seconds. Direct/self guard timings: evidence
  7.19/102.21 seconds, architecture including its mutation checks 34.43
  seconds, and sequence 74.43/339.27 seconds.
