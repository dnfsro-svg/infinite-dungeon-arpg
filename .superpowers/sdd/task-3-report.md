# Stage 11-D Task 3 Report — Dungeon-Owned Automatic Pickup Policy

## Status

DONE.

## Implementation

- Added dungeon-owned `AutoPickupPolicy`, defaulting to normal rarity so existing one-argument callers retain show-all behavior.
- Added `auto_pickup_eligible`: inactive ground is rejected, abyss chest rewards are always eligible, and ordinary monster drops compare rarity against the policy threshold.
- Threaded the policy through `DungeonSession::tick` and `request_nearby_pickups` only.
- Kept automatic pickup before same-tick exit attempts.
- Left explicit `request_pickup(std::uint16_t)` unchanged and policy-independent.
- Added no settings, platform, raylib, runtime, host, or snapshot-metadata dependency.

## TDD RED/GREEN Evidence

Tests were edited before production code. Existing dungeon test access already provided the required ground-item and player-position injection, so no test-support change or production backdoor was needed.

Effective RED build (through the installed VS 2022 x64 developer environment):

```powershell
cmd.exe /d /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out/build/windows-msvc-debug --target arpg_dungeon_tests -- -j1"
```

Before production edits, compilation failed as expected:

- C2039: `AutoPickupPolicy` was not a member of `arpg::dungeon`.
- C2660: the policy-aware tick test called a missing four-argument test helper path.

The base shell first hit shared Ninja/PDB permissions and then lacked MSVC standard-library include paths; those infrastructure failures were not counted as RED. The VS developer-environment failure above was the valid RED.

After the minimal production implementation, the target built. The first full execution exposed an over-strict new stress assertion that required zero allocations across a pickup save boundary; existing transaction architecture explicitly permits save-boundary allocation, so that out-of-scope assertion was removed while deterministic pending/commit equality remained. The next execution showed that the first low-danger fixture seed generated a magic reward; the test was corrected to deterministically select a legal low-danger abyss seed whose first reward is normal, as required by the brief.

Final GREEN:

```text
[real-input] launcher-1000 rooms=1000 ... unexpected=0 overflow=0/0/0/0
[stress] exits=1000 ... unexpected=0 dungeon_overflow=0 relay_overflow=0 combat_overflow=0 input_overflow=0
[stage10-abyss-stress] ticks=600 allocations=0 ...
254 cases, 0 failures
```

The final executable exited with code 0.

## Coverage

- `dungeon_loot_drop`: normal/magic/rare ordering, magic-or-better, rare-only, default show-all, direct eligibility, explicit normal pickup, and policy-aware same-tick pickup-before-exit.
- `dungeon_abyss_reward`: an ordinary normal item remains filtered while a normal abyss reward prepares an abyss claim under rare-only policy.
- `dungeon_stress`: identical sessions under rare-only policy prepare and commit the same rare ordinal, then continue through existing route/determinism and thousand-room stress checks.
- Existing persistence transaction, reload, room exit, abyss reward, and deterministic trace coverage all remained green.

The current dungeon runner does not interpret `ARPG_TEST_FILTER` and registers these as one `dungeon.units` executable, so it cannot isolate the three requested suites without changing an out-of-scope test main. The complete 254-case executable was run instead; it includes all `dungeon_loot_drop`, `dungeon_abyss_reward`, and `dungeon_stress` cases.

## Modified Files

- `src/dungeon/dungeon_types.hpp`
- `src/dungeon/dungeon_session.hpp`
- `src/dungeon/dungeon_session.cpp`
- `src/dungeon/dungeon_transition.cpp`
- `tests/dungeon/dungeon_loot_drop_tests.cpp`
- `tests/dungeon/dungeon_abyss_reward_tests.cpp`
- `tests/dungeon/dungeon_stress_tests.cpp`

## Self-review

- `request_pickup` has no policy parameter or policy check.
- Both automatic scan sites receive the tick policy; the awaiting-exit site still scans before `attempt_exit`.
- Abyss eligibility occurs only after the active check and ignores rarity.
- Ordinary eligibility requires monster-drop source and an inclusive rarity threshold.
- Default parameters preserve existing normal pickup behavior.
- Dependency scan found no `settings`, `platform`, or `raylib` reference in the modified dungeon production files.
- `git diff --check` passed; only existing LF-to-CRLF normalization warnings were emitted.
- No runtime/host/snapshot metadata was changed.

## Commit

`1cd161c feat: filter automatic loot pickup`

## Concerns

- No functional concern remains.
- Test-suite filtering is not implemented by the existing dungeon runner; verification used the full superset as documented above.

## Review Fix Follow-up

The initial Task 3 review was not approved. Both findings were fixed:

- Replaced the near-unbounded normal-reward seed search with a fixed inclusive upper bound of 100,000. Exhaustion leaves `found_normal_reward` false and immediately fails through `ARPG_REQUIRE(found_normal_reward)`.
- Extended both filtered ordinary-drop paths. After the magic or rare pending save commits, the same session scans again with the default show-all policy, selects the original normal ordinal, commits it, and verifies that ground entry is inactive.

### Defect-state effectiveness

A temporary focused runner executed only `dungeon_loot_drop` and `dungeon_abyss_reward` (48 cases) and was removed before final verification and commit.

- With the seed search limit temporarily set to zero, the focused run failed immediately at `found_normal_reward`: `48 cases, 1 failures`.
- With a temporary defect that retained a magic threshold only after the first filtered pickup had committed, the focused run failed exactly at the new widened-policy assertion: `magic_or_better.pending_save_view()->pickup_ordinal == kNormalOrdinal`; `48 cases, 1 failures`.
- After restoring the intended code, the focused run reported `48 cases, 0 failures`.

### Final verification

```powershell
cmd.exe /d /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out/build/windows-msvc-debug --target arpg_dungeon_tests -- -j1"
.\out\build\windows-msvc-debug\bin\arpg_dungeon_tests.exe
git diff --check
```

- Build: passed after restoring the formal full runner.
- Full executable: exit code 0, `254 cases, 0 failures`.
- Thousand-room stress: `unexpected=0`, all dungeon/relay/combat/input overflow counters were zero.
- Abyss stress: 600 ticks, zero allocations.
- Final review-fix content changes are limited to `tests/dungeon/dungeon_abyss_reward_tests.cpp` and `tests/dungeon/dungeon_loot_drop_tests.cpp`; no production file or test-main change remains.

### Review-fix commit

`be8f410 test: harden automatic pickup policy coverage`
