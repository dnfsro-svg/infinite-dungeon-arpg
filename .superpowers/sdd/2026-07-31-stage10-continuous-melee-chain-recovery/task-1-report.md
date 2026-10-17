# Stage 10 Continuous Melee Chain Recovery — Task 1 Report

## Status

BLOCKED after the maximum three direct rounds. The prescribed persistent melee
chain and the evidence-approved light-lane recovery both compiled and changed
the deterministic trace, but the unchanged 12,000-tick budget still expired at
184/600 defeated. CTest was not run and nothing was staged or committed because
native direct GREEN is mandatory.

## Protected baseline

- Worktree: `E:\game\.worktrees\task9-gate-recovery`
- Branch: `codex/task9-gate-recovery`
- Initial committed HEAD: `8b99ef7e341396b8d6ee97e5ca88dec9d4b28abb`
- Initial staging area: empty
- Initial tracked dirty scope: only
  `tests/dungeon/stage10_validation_fixture.cpp`
- Preserved inherited fixture diff hash:
  `8e80991684cab8297d072a5b23e0504f36d51f78`
- Established RED was consumed without rerunning it: native exit 8 at tick
  12,000, 144/600 defeated, Storm/Draw/light 7/35/51, 75 acquisitions,
  68 absent releases, 1,623 sweep ticks, HP 768/768, barrier 51.
- Before round 3 the owner committed the approved design amendment as
  `bc0e94a8bf84daa2a665737ddb1287794c15e495`; it changed only the design spec.
  Staging remained empty.

## Implementation

The fixture now:

1. Persists `melee_chain` from the existing completed close-for-light decision.
2. In melee mode selects active living residents by real light-lane tier,
   squared XY distance, then ordinal, while honoring the one-attempt stalled
   ordinal exclusion.
3. Acquires melee locks directly in close mode and uses the existing
   `movement_toward(player.position, target.position)` path without rebuilding
   a five-unit ranged stance.
4. Preserves the existing absent release, movement-stall exclusion, sweep,
   Storm/Draw/light priority, and pre-melee ranged behavior.
5. Records failure-only melee entry/handoff/tier/ranged-after-entry/cumulative
   distance counters.
6. After round-1 evidence showed repeated blocked-nearest reacquisition, adds
   the approved `recover_until_light_lane` state. A close stall enters recovery;
   successful XY sweep movement does not end it; only a public active living
   in-lane resident ends recovery and is acquired by the same deterministic
   distance/ordinal order. There is no grace period, tick timeout, or action
   shortcut. Ordinary no-resident sweep behavior is unchanged.

## Static audits

Commands run before execution and again after the final round:

```powershell
git diff --check
git diff -- tests/dungeon/stage10_validation_fixture.cpp
rg -n "relay_defeated\s*\(|events_\.try_push\s*\(" `
  tests/dungeon/stage10_validation_fixture.cpp
```

- `git diff --check`: exit 0; only the existing LF-to-CRLF notice.
- Forbidden defeat/event injection search: no matches.
- Forbidden state-write review found no HP, defeat, remaining, phase,
  cooldown, input-buffer, save-state, or production mutation.
- No J1/J2/J3/combo buffer was added.
- `drive_clear()` contains exactly one `session.tick(movement)`.
- Budget, thresholds, density, bounds, equipment, assertions, Stage B-G,
  production, and registered timeout were not changed.

## VS2022 x64 commands

Each round used Developer PowerShell with
`VSCMD_ARG_TGT_ARCH=x64`, `CMAKE_BUILD_PARALLEL_LEVEL=1`, and:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_stage10_validation_fixture -- -j1
$process = Start-Process `
  'out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe' `
  -Wait -PassThru -NoNewWindow
$process.ExitCode
```

- Visual Studio: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`

## Direct rounds

### Round 1 — persistent melee chain

- Configure: exit 0, 7,921 ms.
- Target-only build: exit 0, 2,989 ms.
- Native direct: exit 8, 3,192 ms.
- Terminal state: 99/600 defeated, 501 remaining, HP 745/768, barrier 0;
  Storm/Draw/light 3/18/35; acquisitions 1,326; absent releases 43;
  movement stalls 1,283; sweep ticks 2,578; melee entry 1; melee handoffs 30;
  in-lane/nearest acquisitions 0/1,312; ranged-after-entry 0; cumulative melee
  initial distance squared 26,214.6.
- Diagnosis: melee intent persisted, but a close stall released the lock and
  ordinary sweep cleared after one successful XY tick, repeatedly reacquiring
  another blocked nearest resident.

### Round 2 — facing-progress hypothesis

- Evidence checked: production `simulate_player` updates facing before
  `move_player_to(candidate)` can reject XY movement.
- Minimal experiment treated an XY-stationary facing change as close progress.
- Configure: exit 0, 7,527 ms.
- Target-only build: exit 0, 3,057 ms.
- Native direct: exit 8, 3,119 ms.
- All terminal counts and values were identical to round 1, proving the facing
  hypothesis did not occur in this deterministic trace.
- The entire facing experiment was reverted with `apply_patch` before round 3.

### Round 3 — approved light-lane recovery

- Design HEAD: `bc0e94a8bf84daa2a665737ddb1287794c15e495`.
- Configure: exit 0, 7,559 ms.
- Target-only build: exit 0, 2,968 ms.
- Native direct: exit 8, 3,874 ms.
- Terminal state: 184/600 defeated, 416 remaining, HP 713/768, barrier 0;
  Storm/Draw/light 7/33/79; acquisitions 96; absent releases 95; movement
  stalls 1; sweep ticks 1,983; melee entry 1; melee handoffs 82; in-lane/nearest
  acquisitions 2/80; ranged-after-entry 0; cumulative melee initial distance
  squared 3,325.87; recovery entries/ticks/reacquisitions 1/1,086/1.
- Recovery removed the repeated stall churn and successfully reached/reacquired
  a public in-lane resident, but total real combat throughput remained below the
  immutable clear budget.

## CTest and commit gates

- Registered CTest: not run because every native direct exit was nonzero.
- Staging area: empty after the final round.
- Commit: none.
- Required commit message `test: preserve stage10 melee target chain` was not
  used.

## Final scope audit

- Current committed HEAD:
  `bc0e94a8bf84daa2a665737ddb1287794c15e495`.
- Post-card fixture diff hash:
  `0f6fd993948be99d1be604e066ec2bb3cebcdcdd`.
- Tracked dirty scope remains only
  `tests/dungeon/stage10_validation_fixture.cpp`.
- The new continuous-melee plan and the three superseded Stage 10 plans remain
  untracked and unstaged. The superseded plans were not edited.
- No file was deleted, reset, cleaned, or staged.

## Concerns / next card

The authorized navigation repair is behaviorally evidenced: movement stalls
fell from 1,283 to 1, light requests rose from 35 to 79, and defeated monsters
rose from 99 to 184. It is still insufficient for 600 real defeats in 12,000
ticks. The remaining evidence points to action throughput after stable melee
navigation rather than another target-chain break. A new root-cause/design card
is required before any combo buffering, action-priority, production, equipment,
budget, or assertion change.
