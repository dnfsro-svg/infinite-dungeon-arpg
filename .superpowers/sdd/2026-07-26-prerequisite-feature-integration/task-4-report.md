# Task 4 implementation report

## Baseline

- Worktree: `E:/game/.worktrees/integration-large-square-rooms`
- Branch: `codex/integration-large-square-rooms`
- Start HEAD: `17cbb0a01f4a4e039574e689986fb0a5256c42ed`
- Start status: clean

## Test-first RED evidence

- Test-only changes were ported before production code from `90c4348`,
  `82379ae`, and `2eabc1b`.
- Current-suite totals were fused as combat `243 -> 245` and dungeon
  `287 -> 294`.
- The first raw build attempt stopped during CMake regeneration because the
  shell had no loaded MSVC/Windows SDK environment (`detected ''`). This was
  recorded as an environment precondition failure, not accepted as RED.
- After dot-sourcing `scripts/Configure.ps1` in the same PowerShell process,
  the serial combat build failed in `player_health_tests.cpp` with C2039:
  `CombatWorld` has no member `restore_player_health_percent`.
- A separate serial dungeon build failed in
  `dungeon_health_potion_tests.cpp` with C1083: missing
  `dungeon/health_potion_loot.hpp`.
- Both failures are the intended missing-production-API/type RED state.

## Replay and transaction review

- `90c4348` replayed first. Serial combat build and focused verification passed:
  CTest `1/1`, direct `245 cases, 0 failures`.
- `82379ae` production paths applied cleanly. Its historical test hunks
  conflicted only because the later `2eabc1b` test superset was already present
  for RED; the current test superset and combined registry were retained. The
  dungeon library built and an ignored domain probe exited `0` after checking
  constants, exact threshold behavior, deterministic drop/miss outcomes, fixed
  capacity, and claim ordinals.
- `2eabc1b` applied without conflict. Focused direct verification passed
  `7 cases, 0 failures`; the intermediate dungeon suite passed `1/1` and direct
  `294 cases, 0 failures`.
- `37a0848 a83531f 78da4bf 6ffa3cf` were applied together with
  `cherry-pick -n` as one transaction unit; there were no conflicts.
- Final registry totals are combat `245`, potion-only `19`, and dungeon `306`.
- The combined implementation keeps ground records and claims fixed-capacity,
  uses the secondary per-monster material claim ordinal, and does not add a
  renderer, host, material, evidence, Stage 10/11, room, or performance path.

## Contract readback

- Healing is `ceil(max_hp * 2500 / 10000)`, clamped to missing HP, returns the
  actual amount, bypasses the abyss recovery multiplier, and never revives a
  defeated player.
- Drop rolling uses a separate domain constant and exact `< 3000` comparison.
  For spawn ordinal `37`, seed `2725` produces raw `2999` (drop) and seed
  `1902` produces raw `3000` (miss). Seeds `0/1/2/3` produce frozen raw values
  `259/3576/1771/4685`, represented by outcomes
  `true/false/true/false` in the permanent test.
- Auto-use is integer exact at `HP * 10000 <= max_HP * 7500`; above 75% leaves
  the ground record unchanged.
- Before an exact save commit, HP, ground records, claim bits, and receipt do
  not mutate. `not_committed` reconstructs the same ordered claim;
  `indeterminate` faults before applying healing; exact receipt and generation
  validation prevent duplicate application.
- Normal and abyss room clear stage material vacuum and ordered potion claims
  in one save. After exact commit, the abyss rule is cleared first so maximum
  HP is authoritative, then healing/receipt are applied, then room-clear state
  is published. Transition removes previous-room potion records.
- Stress snapshot equality compares all potion records, receipt fields, and
  saturation diagnostics.

## Review-driven RED/GREEN

- A read-only transaction audit initially found two test gaps: stress save
  confirmation did not recognize `health_potion_pickup`, and the RNG test did
  not freeze the exact 2999/3000 boundary/domain.
- Tests were strengthened before the stress-driver fix. The direct dungeon run
  exited `1` with exactly `306 cases, 1 failures` at
  `exercise_health_potion_save_boundary(...)` in the existing launcher-input
  stress case.
- Minimal test-driver repair classified the potion save boundary, counted it,
  and confirmed committed potion saves by restoring their resume phase. No
  production code changed for this repair.
- The exact same full direct run then exited `0` with
  `306 cases, 0 failures`. Independent re-review returned `APPROVED` with no
  remaining Critical or Important findings.

## Verification

- Builds were serial (`CMAKE_BUILD_PARALLEL_LEVEL=1`, Ninja `-j1`); no second
  build or test was started while one was active.
- Final focused potion suite: `19 cases, 0 failures`.
- Final direct dungeon suite: `306 cases, 0 failures`.
- Final CTest gate: `2/2` passed; combat `0.95s`, dungeon `204.89s`.
- Final direct combat suite: `245 cases, 0 failures`.
- Dungeon long-trace golden remained `0x0d55b41ebb0b1128` with unchanged door
  hit counts `9/11/9/7`.
- Extra Stage 16 direct simulation passed and retained
  `frozen_existing_stream=10882612280735857555`, proving the potion RNG did not
  perturb the existing room/equipment/abyss stream.
- `git diff --check` passed. No build/test process remained.

## Scope and commit

- Exact range contains 17 approved files: two combat world/test files; dungeon
  domain, session, transition, snapshot, types, CMake, and tests. There is no
  diff in platform, renderer, font, host, material assets/runtime, validation
  evidence, abyss golden, Stage 16 golden, room-size, or performance paths.
- Final commit: `8a848b30fafe87135cf432be8d0cd236c45c2c49`
  (`feat: add atomic health potion drops`).
- Push: intentionally not performed.

## External-review deterministic-comparison follow-up

- Scope was restricted to `tests/dungeon/dungeon_stress_tests.cpp`; the
  registered stress case array and the expected dungeon total remained
  unchanged at 306.
- RED: negative assertions were added inside the existing
  `identical_seed_and_route_are_field_equal` case before the comparator was
  implemented. The serial `arpg_dungeon_tests` build compiled the fixture and
  then failed at link with LNK2019/LNK1120 for the intentionally undefined
  `same_pending_health_potion_claim(...)` symbol. This was the expected missing
  comparison behavior, not an environment failure.
- GREEN: `same_snapshot()` now compares
  `pending_health_potion_spawn_ordinal`; the narrow optional claim comparator
  compares presence, the complete fixed spawn-ordinal array, count, expected
  HP, and expected max HP; `commit_equal()` invokes it before resolving either
  save. Regression assertions cover both optional-presence directions, every
  fixed array slot, all scalar claim fields, and verify that a differing claim
  leaves both pending saves unresolved.
- Final serial build completed without a new warning. The focused potion run
  passed `19 cases, 0 failures`. The authoritative direct dungeon run passed
  `306 cases, 0 failures`; the abyss stress hash remained
  `0x0d55b41ebb0b1128`.
- `git diff --check` passed. Read-only scope audit found exactly one changed
  tracked path, no `kCases` registry diff, no `const_cast`, no production,
  registry, golden, or Task 5 changes. Self-review found no remaining issue in
  the requested comparison-completeness class.
- Commit: `5a0e117` (`test: harden potion pending-state determinism`). The
  tracked worktree was clean after commit. Push was intentionally not
  performed.
