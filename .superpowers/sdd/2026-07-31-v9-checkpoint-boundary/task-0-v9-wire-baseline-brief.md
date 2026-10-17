# Task 0 Brief: Freeze the pre-refactor V9 wire manifest

Worktree: `E:/game/.worktrees/whole-game-refactor-2026-07`

Base commit: `5ba484f`

Read completely before editing:

- `docs/superpowers/specs/2026-07-31-v9-checkpoint-boundary-design.md`
- Task 0 and Global Constraints in
  `docs/superpowers/plans/2026-07-31-v9-checkpoint-boundary.md`

## Allowed files

- Create `tests/persistence/checkpoint_v9_wire_baseline.cpp`
- Create `tests/persistence/checkpoint_v9_wire_baseline_test.cmake`
- Modify `tests/persistence/CMakeLists.txt`

Do not edit production code, existing fixtures/tests, plans, evidence, or
timeouts. Do not commit.

## Required behavior

Create executable target `arpg_checkpoint_v9_wire_baseline` and CTest
`persistence.v9_wire_baseline`.

The executable accepts exactly a scenario name and output path, constructs one
deterministic valid V9 save, encodes it using the current production
`encode_checkpoint_v9_into`, and writes only the exact encoded bytes to that
output path. It must support these five scenarios:

1. `canonical_none`
2. `active_normal`
3. `started_abyss`
4. `death_pending`
5. `cleared_abyss_rewards`

Across the five fixtures, use nondefault literal values that exercise active
room/combat data, attack/history/effects, a nondefault CombatDeathSnapshot,
abyss data, equipment ground, material ground, and potion ground. Every fixture
must pass the current production structural validator before encoding. Do not
use wall clock, process ID, filesystem state, random_device, or unstable
unordered iteration.

Heap-own every `SaveCheckpointSlot` and 8 MiB byte buffer using
`std::unique_ptr` plus `new (std::nothrow)` and check allocation before use. Do
not place a full save slot/buffer automatically on the stack. Return nonzero on
bad arguments, unknown scenario, invalid fixture, encode failure, partial
write, or filesystem failure.

The CMake script runs all five scenarios into its supplied build directory,
checks each exact byte count, computes `file(SHA256 ...)`, and compares against
literal pre-refactor expected values. It may have a deliberate one-time
capture mode only while deriving the literals, but the checked-in/default test
must never learn or update expected values automatically. It must print a
scenario-specific diagnostic on mismatch.

Register the executable/test without changing `persistence.units`. Link the
probe only to the current libraries needed to build its valid fixtures; this is
a pre-refactor characterization probe, not the later link-isolation target.

## Verification

Run serially:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_checkpoint_v9_wire_baseline --parallel 1
ctest --test-dir out/build/windows-msvc-debug `
  -R '^persistence\.v9_wire_baseline$' `
  --output-on-failure -j1 --no-tests=error
git diff --check -- tests/persistence/CMakeLists.txt `
  tests/persistence/checkpoint_v9_wire_baseline.cpp `
  tests/persistence/checkpoint_v9_wire_baseline_test.cmake
```

Require 1/1 pass. Report all five exact sizes/SHA-256 values, changed paths,
and commands/results. Do not stage or commit.
