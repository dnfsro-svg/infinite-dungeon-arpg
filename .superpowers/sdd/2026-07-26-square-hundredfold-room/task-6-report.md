# Task 6 implementation report

## Result

Task 6 is implemented on baseline
`30fe24b48391733b0ecffd2bd545890dd1248318`.

Normal and abyss rooms now calculate the permanent quarter-kill threshold as
`ceil(initial_monsters / 4)`. Reaching it prepares a separate exact
`room_unlock` save. The four exits and abyss hole remain closed until that
receipt commits; commit publishes exactly one `exits_opened` event while the
room stays in combat. Full clear remains a later, separate transaction and
publishes exactly one `room_cleared` event.

Fresh Task 6 focused verification passed 15/15 cases with zero failures:
dungeon 12/12, platform 2/2, and checkpoint V9 persistence 1/1.

## Implemented scope

- Added `required_kills(initial) == (initial + 3) / 4` and persisted room
  authority for `required_kills`, committed `exits_unlocked`, and `full_clear`.
- Added exact `room_unlock` transaction preparation, rollback/retry,
  indeterminate faulting, one-event publication, and same-tick ordering before
  full-clear preparation.
- Removed the production live-target clear shortcut. `prepare_room_clear()`
  now requires `defeated == initial`, zero remaining targets, and a committed
  unlock.
- Kept attacks, skills, AI and combat counters live after unlock. Full clear
  still reaches the existing cleared/awaiting-exit flow and pays rewards once.
  `cleared` remains the existing one-tick publication phase; exit input remains
  enabled in its established `awaiting_exit` phase, plus the new committed
  combat-unlock phase.
- Allowed committed-combat exits. Normal early exit atomically applies defeated
  monster XP plus already claimed loot, but does not pay the clear bonus or
  vacuum unclaimed items/materials. Transient room state is discarded only
  after commit; `not_committed` restores the same combat room.
- Added distinct `abyss_early_exit` for a started abyss room. Commit applies
  failed/resolved abyss authority, zero clear reward, and the selected next
  room atomically; rollback and indeterminate semantics remain explicit.
  Existing cleared-abyss `abyss_abandon` protection was not changed.
- Bound all four door states, the hole visual/prompt, and HUD notices to the
  committed unlock flag. Hole interaction radius remains exactly `3.25F` and
  the existing host input binding remains unchanged.
- Added V9 round-trip coverage proving a partial unlocked room reloads into
  combat with exits open, rather than being promoted to cleared.
- Updated the existing deterministic clear helper and stress transaction
  classifier so legacy suites can progress through the new unlock boundary.

## Enum compatibility

`PendingSaveKind` is not explicitly assigned numeric constants, so preserving
declaration order is the compatibility mechanism. Both Task 6 values were
appended after every pre-existing value. No old enumerator moved or changed its
numeric value: `room_unlock` and `abyss_early_exit` are the only new tail values.
Checkpoint V9 stores room authority fields, not the in-memory enum value.

## TDD evidence

### RED

1. The initial dungeon test registration was built before production symbols
   existed. After loading the Visual Studio SDK environment, the focused
   source failed to compile on the missing `required_kills`,
   `DungeonSnapshot::exits_unlocked`, and `PendingSaveKind::room_unlock`.
2. After the minimal unlock path, the expanded eight-case focused runner
   reported `8 cases, 3 failures`: partial-unlock reload was not yet represented
   correctly by its fixture, full clear duplicated `exits_opened`, and the
   live-target clear shortcut did not fault.
3. Early-exit tests were added before their transaction kind. Compilation
   failed on missing `PendingSaveKind::abyss_early_exit`. After the first
   implementation, the 12-case runner reported `12 cases, 1 failures`, exposing
   insufficient preallocated item capacity at checkpoint capture. The fixture
   was corrected to exercise the real no-allocation boundary.
4. The two platform cases initially reported `2 cases, 2 failures`: committed
   combat unlock still rendered a sealed hole, and HUD notices did not expose
   the hole/exit prompt.

The first raw `cmake --build` attempt stopped before compilation because the
calling PowerShell did not expose `WindowsSDKVersion`. The same command under
`VsDevCmd.bat` produced the meaningful RED compile result above; the SDK
environment failure is not counted as a test failure.

### GREEN progression

- Minimal dungeon unlock cases: 3/3.
- Expanded threshold/commit/rollback/fault/reload/order cases: 8/8.
- Final dungeon cases including normal and started-abyss early exits: 12/12.
- Platform committed-combat door/hole/HUD cases: 2/2.
- V9 partial-unlock round trip: 1/1.

## Final focused commands and results

Before each native compile batch, the task checked that no `cmake`, `ninja`, or
`cl` process was active and that free physical memory exceeded 1.5 GiB. The
final batch started with 5.43 GiB free and zero matching compiler processes.

Only Task 6 production objects were rebuilt:

```text
ninja -C out/build/windows-msvc-debug -j1 src/dungeon/CMakeFiles/arpg_dungeon.dir/dungeon_session.cpp.obj src/dungeon/CMakeFiles/arpg_dungeon.dir/dungeon_transition.cpp.obj src/dungeon/CMakeFiles/arpg_dungeon.dir/dungeon_snapshot.cpp.obj src/dungeon/CMakeFiles/arpg_dungeon.dir/room_progress_checkpoint.cpp.obj
```

Result: all four objects compiled successfully. The existing dungeon archive
was reconstructed with `lib.exe` from its 20 current object files; no unrelated
test translation unit was compiled.

The final focused test and platform-production object batch was:

```text
ninja -C out/build/windows-msvc-debug -j1 tests/dungeon/CMakeFiles/arpg_dungeon_tests.dir/dungeon_exit_unlock_tests.cpp.obj tests/persistence/CMakeFiles/arpg_persistence_tests.dir/checkpoint_v9_tests.cpp.obj tests/platform/CMakeFiles/arpg_platform_tests.dir/task6_exit_unlock_view_tests.cpp.obj src/platform/raylib/CMakeFiles/arpg_raylib.dir/dungeon_view_math.cpp.obj src/platform/raylib/CMakeFiles/arpg_raylib.dir/hud_notice_state.cpp.obj
```

Result: 5/5 objects compiled. The three focused executables were linked with
MSVC `link.exe` from their focused main/test objects and the current local
production libraries. For the platform runner, the two fresh platform objects
preceded `arpg_raylib.lib`, ensuring the runner exercised the modified
definitions rather than stale archive members.

Final execution command:

```text
out\task6-focused\task6_exit_unlock.exe
out\task6-platform-focused\task6_exit_unlock_view.exe
out\task6-persistence-focused\task6_v9_unlock.exe
```

Final output:

```text
12 cases, 0 failures
2 cases, 0 failures
1 cases, 0 failures
focused_exit_codes dungeon=0 platform=0 persistence=0
```

`git diff --check` also completed with no whitespace errors. The CRLF notices
are Git working-copy conversion warnings, not diff-check failures.

## Full-gate resource event and final controller closure

The first exact serial full-target build completed all 103/103 steps. While
MSVC compiled `dungeon_abyss_stress_tests.cpp`, its transient private allocation
peaked near 16.47 GiB (working set near 6.81 GiB); physical free memory briefly
reached zero and machine commit reached approximately 36.57/38.05 GiB. A stop
request raced with natural compiler completion, so no process was killed. The
machine recovered and every later build remained serial (`-j1`). The only
compiler warning was the pre-existing constant-condition C4127 in
`pause_menu_view_tests.cpp:377`.

The first complete CTest run exposed legacy fixture migration omissions rather
than production regressions: dungeon reported 343 cases with 12 failures,
persistence passed 106/106, and platform reported 497 cases with 8 failures.
The stale tests had omitted the separately committed `room_unlock` boundary,
the new durable `exits_unlocked` authority, or exact V9 abyss-death history.
They were migrated without changing production code or weakening fault and
atomicity assertions.

Additional closure evidence:

```text
transaction focused runner: 20/20 passed
equipment 1000-room stress: 2/2 passed (139.8 s)
affix 1000-room stress: 2/2 passed (12.3 s)
independent final review: P0=0, P1=0, P2=0; ready to merge
```

The final exact target build rebuilt the last changed platform test in 2/2
steps, and an immediate exact rerun reported `ninja: no work to do.` The final
formal command was:

```text
ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon|persistence|platform)\.units$' --output-on-failure
```

Final result:

```text
dungeon.units:     passed, 304.33 s
persistence.units: passed, 81.84 s
platform.units:    passed, 15.40 s
3/3 tests passed, 0 failed; total 402.05 s
```

Task 6 therefore has no remaining build, test, review, or resource blocker.

## Review fix round 1

Independent review reported P0=0, P1=3, P2=1. All four findings were
addressed without changing the Task 6 quarter-kill contract:

- Started-abyss early exit now records the destroyed room as a literal
  `LastAbyssResolution::lifecycle == failed`, while leaving the selected
  destination room and its top-level abyss authority unchanged. The history
  record is constrained to zero generated/claimed and all rewards abandoned.
- Public V5-V8 decoding still requires reserved byte 150 to be zero and maps
  it to `none`; public V8 encoding rejects a failed-only history that it cannot
  represent losslessly. The embedded ARPGSV8 image is canonical and also keeps
  byte 150 at zero. V9 stores the lifecycle in a one-byte outer payload tail,
  covered by the outer CRC. Old V9 images without the tail decode to `none`;
  the envelope/header, 8 MiB maximum, worker protocol, and inner size/CRC stay
  unchanged.
- Reloading an already-cleared abyss room now reconstructs committed
  `exits_unlocked=true` and `full_clear=true` flags without republishing either
  `exits_opened` or `room_cleared`.
- Legacy lifecycle/wave drivers now commit `room_unlock` before
  `room_clear`/`abyss_clear`; event assertions use the durable order
  `exits_opened` then `room_cleared`. Each transaction independently reserves
  one publication slot.
- The V9 unlock suite case table has namespace-static storage, removing the
  returned pointer to a function-local array.
- Exact state comparators and their field-mutation tests now include the new
  resolution lifecycle.

### Review RED evidence

- Cleared-abyss reconstruction: the 13-case Task 6 runner failed on
  `snapshot.exits_unlocked` before committed flags were restored.
- Real early-exit V9 tests initially failed both normal and abyss destination
  cases during encode; after the model was introduced, two negative cases
  exposed the missing rejection of illegal `lifecycle=cleared` history.
- The affected legacy lifecycle/wave runner initially reported 6/33 failures:
  four stale one-transaction helpers and two reversed event-order assertions.
  After migration, one remaining stale assertion correctly exposed that the
  unlock was already committed before clear preparation.

### Fresh GREEN verification

The Visual Studio 2022 developer environment was loaded explicitly and all
native work remained serial (`-j1`). Production incremental compilation
completed 5/5 steps for `arpg_dungeon.lib` and `arpg_persistence.lib`.

Focused Task 6 results:

```text
dungeon exit/unlock: 13 cases, 0 failures
checkpoint V9 unlock/early-exit: 4 cases, 0 failures
platform view/HUD: 2 cases, 0 failures
focused total: 19 cases, 0 failures
```

Affected old-suite results:

```text
lifecycle + wave: 33 cases, 0 failures
lifecycle + wave + death: 64 cases, 0 failures
public V8 codec + V9 + real save worker: 49 cases, 0 failures
final progression/save-store field audit: 26 cases, 0 failures
```

The 64- and 49-case runs subsume some focused cases and are reported as
separate regression surfaces rather than an inflated aggregate. The complete
build target was not run, per controller resource restriction. `git diff
--check` passed before amend, and no root-level compiler artifacts or
untracked source files remained.

## Review fix round 2

Follow-up review reported P0=0, P1=1, P2=1: the first fix had placed the new
field inside an ARPGSV8 reserved byte, and the failed-history path did not yet
exercise worker exact readback. Round 2 moved the field to the V9-owned payload
tail and added real worker coverage.

RED was `5 cases, 3 failures`: both early-exit destinations failed the new
`inner[150] == 0` assertion, and a synthetic old V9 image without the new tail
failed to decode. GREEN keeps the inner image publicly V8-decodable, accepts
only zero or one outer tail byte, accepts only `none/failed`, validates failed
counts, and rejects a genuinely truncated room payload, an extra byte, and an
illegal tail enum after recomputing the outer CRC.

Fresh serial verification:

```text
arpg_persistence.lib incremental build: 8/8 steps
V9 focused compatibility/early-exit: 5 cases, 0 failures
real worker exact/equal-revision: 3 cases, 0 failures
Task 6 dungeon: 13 cases, 0 failures
Task 6 platform: 2 cases, 0 failures
affected public V8 + V9 + worker: 50 cases, 0 failures
```

No complete target was run, preserving the controller's resource boundary.

## Review fix round 3

Follow-up review reported P0=0, P1=1, P2=0: an old V9 image without the
outer lifecycle byte decoded a legal pending abyss death with historical
resolution lifecycle `none`. `DungeonSession` correctly rejected that state
because the durable death checkpoint requires the matching resolution to be
`failed`.

The RED fixture encodes a structurally legal pending abyss death, removes only
the final V9 lifecycle byte, reduces the outer payload length, and recomputes
the outer CRC. The focused result was `6 cases, 1 failures`; the failure
confirmed both `lifecycle == none` and restore fault
`death_sequence_mismatch`.

Old V9 decode now normalizes the lifecycle only when there is no extension
byte and the durable death authority precisely identifies the same abyss
death: pending lifecycle, `death_was_abyss`, nonzero sequence, committed
death-retreat generation/transition, matching death anchor, top-level failed
abyss, matching room seed/rule, and exact zero-generated/all-abandoned reward
counts. This is an in-format normalization, so `migrated` remains false. A
later V9 write naturally adds the `failed` tail byte. Historical cleared
abandonment and ordinary pending death fixtures retain lifecycle `none`.

Fresh serial verification:

```text
arpg_persistence.lib incremental build: 2/2 steps
V9 lifecycle compatibility/restore: 6 cases, 0 failures
death lifecycle restore regression: 31 cases, 0 failures
real worker exact/equal-revision: 3 cases, 0 failures
```

`git diff --check` passed. No complete target was run and nothing was pushed,
preserving the controller's resource boundary.
