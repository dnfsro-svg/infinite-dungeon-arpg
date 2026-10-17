# Task 8 Report — Golden, clean builds, and boundary audit

Status: `BLOCKED`

## Baseline

- Worktree: `E:\game\.worktrees\balance-hp-potions`
- Branch: `codex/extreme-monster-nerf-hp-potions`
- Starting HEAD: `bfafd1e1daec64791e9d4bf5d14eacc1629ceaae`
- Merge base for boundary audit: `44f2e590413b9e17ddbf915a09b6acd9cac511fb`
- Live play, screenshots, final validation document, and commit are intentionally deferred to the main agent.

## Commands and evidence

### Core Debug clean-first baseline

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-core-debug -Fresh
cmake --build --preset windows-msvc-core-debug --clean-first -- -j1
ctest --preset windows-msvc-core-debug --output-on-failure
```

- Configure used MSVC 19.44.35228.0 from the approved VS 2022 Build Tools
  x64 path.
- Clean build completed all 196 build steps.
- CTest stopped on the expected `dungeon.units` failure: 4/5 passed,
  1/5 failed, 203.00 seconds.
- Dungeon executable: 306 cases, 1 failure.
- Sole baseline failure:
  `dungeon_abyss_stress.1000 room abyss trace matches golden through reloads`.
- Expected hash: `0xe102b17e6423351b`.
- Actual hash: `0x0d55b41ebb0b1128`.
- Door trace: 4000 trials, 36 hits, exact directions `9/11/9/7`.
- Reload trace: zero reloads is field-equal to 27 reloads; the test compares
  every room descriptor, encounter, reward, and resolution.
- The same run passed `combat.units`. Direct execution then freshly reported
  `245 cases, 0 failures`.
- The combat tests cover depths 1-3, all eight monster IDs, 256 seeds, normal
  and abyss affix count zero. They also freeze depth-4 count weights
  `55/38/7/0`, tier weights `80/20/0`, and abyss minimum affix count one.
- The Task 2 production diff changes only the depth-1-3 count weights to
  `100/0/0/0` and adds the abyss `depth <= 3` empty-set return. The abyss
  stress source itself has no diff from merge base.

### Allowlisted edits

- Updated only the verified abyss hash constant in
  `tests/dungeon/dungeon_abyss_stress_tests.cpp`.
- Added exact potion frozen comparisons in
  `tests/dungeon/dungeon_stress_tests.cpp`: ground count; every packed active
  potion's spawn ordinal, claim ordinal, and Vec3; receipt valid, commit
  generation, restored HP; and diagnostic ground saturation.
- Existing item, material, coupon, exit, room, reward, and resolution
  comparisons were preserved.
- `git diff --check` passed before the rerun.

### Core Debug rerun and unexpected blocker

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-core-debug
cmake --build --preset windows-msvc-core-debug -- -j1
ctest --preset windows-msvc-core-debug --output-on-failure
```

- Rebuilt the two changed dungeon test objects and linked successfully.
- `core.units`, `abyss.units`, `skills.units`, `combat.units`, and
  `dungeon.units` passed. `dungeon.units` took 199.99 seconds.
- CTest then reached an unexpected pre-existing Stage 16 golden and stopped:
  7/8 passed, 1/8 failed, 202.41 seconds. The remaining planned tests were not
  run because the preset stops on failure.
- Failing test: `stage16.loot_reinforcement.simulation`.
- Message: `FAIL: Task1 room/equipment/abyss golden fingerprint`.
- Expected fingerprint: `15972885342817959559`.
- Actual fingerprint: `10882612280735857555`.
- Its baseline/interleaved equality passed first, so the material RNG domain
  remains isolated. The fingerprint mixes encounter affix count/id/tier over
  512 depth-1 rooms, so the approved depth-1-3 zero-affix behavior explains
  the changed value.
- `tests/dungeon/stage16_loot_reinforcement_simulation.cpp` is outside the
  original Task 8 tracked-change allowlist. It was not modified before the
  stop, and Debug graphical/Release work was paused pending independent audit.

### Plan correction and second justified golden

- Independent historical minimal comparison established that commit
  `c738053` (Task 1 present) still produced `15972885342817959559`, while
  applying only the Task 2 production diff changed it exactly to
  `10882612280735857555`.
- The ignored Task 8 brief was corrected to permit only this one Stage 16
  fingerprint constant. No probability, baseline-vs-interleaved, stream, or
  other assertion was changed.
- The previously recorded Stage 16 failure is the TDD RED. The minimal GREEN
  change updates only `15972885342817959559ULL` to
  `10882612280735857555ULL`.
- Focused CTest passed 3/3 in 259.91 seconds: `combat.units` 1.03 seconds,
  `dungeon.units` 213.99 seconds, and
  `stage16.loot_reinforcement.simulation` 44.88 seconds.
- Fresh full core CTest then passed 69/69 with zero failures in 945.53
  seconds. The long runtime was real work, not a deadlock: read-only process
  and checkpoint sampling showed active CMake source/guard tests and progress
  through all 69 tests.

### Graphical Debug clean build and first full CTest attempt

Read-only resource sampling immediately before the graphical build showed
substantial host pressure without a disk-capacity problem:

- Available physical memory: 381-510 MB out of 16092.9 MB over five samples.
- Total physical-disk average queue length: 0.86-2.18.
- E: free space: 509.14 GB out of 953.56 GB.
- No user process was killed and no CTest timeout was changed.

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug --clean-first -- -j1
```

- Configure used MSVC 19.44.35228.0 from
  `C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe`.
- The clean graphical Debug build completed 366/366 with exit code 0.
- Started `2026-07-24T06:48:14.1954810-04:00`, ended
  `2026-07-24T06:54:34.1610690-04:00`: 379.97 seconds.
- The only notable compiler output was non-fatal MSVC C4127 constant-condition
  warnings at `tests/dungeon/dungeon_health_potion_tests.cpp:176-179`.
- Full log: `out/validation/task8-debug-build.log`.

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
```

- First full attempt stopped on test 17 because the preset stops on failure:
  16/17 passed, 1/17 failed, 379.37 seconds; CTest exit code 8.
- `combat.units` passed in 0.99 seconds, `dungeon.units` passed in 210.76
  seconds, and `stage16.loot_reinforcement.simulation` passed in 44.90 seconds.
- Failing test: `stage10.formal_game.capture_after_present`, 35.26 seconds;
  the validation executable returned code 13.
- Source audit maps code 13 to the fourth Stage 10 capture run
  (`chaos_expansion`) failing to reach and save its capture within the existing
  1600-presented-frame bound. Fresh captures 01-03 were present; capture 04 was
  absent. There was no C++ test assertion failure and no timeout was modified.
- Full log: `out/validation/task8-debug-ctest.log`.

### Stage 10 focused reproduction and root-cause boundary

After the first failure, host pressure had materially receded: five samples
showed CPU 1.95-7.66%, available memory 612-650 MB, and total physical-disk
queue length 0.01-0.03. The failed formal test was then rerun once, without
changing code or timeout:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
ctest --preset windows-msvc-debug `
  -R '^stage10\.formal_game\.capture_after_present$' --output-on-failure
```

- The focused reproduction failed again: 0/1 passed, 1/1 failed, 51.89
  seconds (CTest total 51.93 seconds), CTest exit code 8; validation executable
  code 13.
- This repeat under low CPU/disk load rules out treating the failure as the
  brief's permitted one-off resource-loading timeout.
- Full log: `out/validation/task8-debug-stage10-rerun.log`.

The source and save evidence identify the first runtime divergence without any
instrumentation or source edit:

- `stage10_formal_game_validation.cpp` maps code 13 to capture index 4,
  `chaos_expansion`, returning false from its prepare/run/capture check.
- Captures 01-03 are fresh and non-empty; `04-chaos-expansion.png` is absent.
- `capture-4/run_a.sav` is a valid generation-3 checkpoint for root 2068. Its
  final room is index 0, depth 1, chaos ecology, normal (`is_abyss=0`), with
  `last_transition=2` (`death_retreat`), abyss lifecycle `none`, abyss rule
  `none`, and no abyss resolution.
- Its frozen death record is sequence 1, normal-room death, monster ID 6
  (`chaos_chaser`), chaos damage, raw damage 14, final/health loss 11, and
  HP 0/1000.
- Therefore the validation driver dies in the initial normal chaos room before
  it enters the selected chaos abyss. There can be no
  `chaos_expansion` abyss-environment hazard or target screenshot, and the
  existing 1600-presented-frame bound expires without capture 04.
- Task 1 commit `cd31982` changed only the monster catalog and its combat tests;
  `git diff --quiet cd31982^ cd31982 --` for the Stage 10 formal source and
  raylib host returned 0 (no changes). In that commit, `chaos_chaser` changed
  from 260 HP / 45 chaos damage to 120 HP / 14 chaos damage. The observed death
  uses the new 14-damage value, while the unchanged legacy validation driver
  still fails to clear root 2068 safely.

This is a repeatable real formal validation failure, not a harmless warning or
resource-only timeout. Per the Task 8 brief, Release build/CTest and subsequent
artifact/boundary completion were not started after this point.

## Current status

`BLOCKED` — graphical Debug clean build passes, but the required full Debug
CTest cannot pass because `stage10.formal_game.capture_after_present`
reproducibly fails before the chaos abyss is entered. Release validation is
not run. No game code, timeout, or unrelated tracked file was changed during
this build-validation turn.

## Stage 10 deterministic dense-room build repair attempt

### Authorized RED and source comparison

- The accepted low-load RED remained the focused
  `stage10.formal_game.capture_after_present` code 13 recorded above: root
  2068 died in the ordinary chaos room to `chaos_chaser`, captures 01-03
  existed, and capture 04 was absent.
- Compared `tests/dungeon/stage10_formal_game_validation.cpp` against the
  complete `install_validation_build` change in Task 3 commit `d57c2e8`.
- Modified only `tests/dungeon/stage10_formal_game_validation.cpp`: added the
  catalog include and mirrored the six deterministic base IDs, six explicit
  affixes per slot, required level 95, reinforcement +15, and
  `validate_item` check. Seed search, auto-driver, 64 simulation steps per
  presented frame, capture predicates (including the chaos 16-frame gate),
  1600-frame bound, and screenshot validator were unchanged.
- `git diff --check -- tests/dungeon/stage10_formal_game_validation.cpp`
  passed.

### Build

```powershell
cmake --build --preset windows-msvc-debug `
  --target arpg_stage10_formal_game_validation -- -j1
```

- The first invocation correctly failed before compilation because the shell
  had not loaded the VS environment (`fatal error C1083: cstddef`); no source
  change followed from this environment failure.

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_stage10_formal_game_validation -- -j1
```

- Configured the fixed MSVC environment, compiled the changed object, and
  linked `arpg_stage10_formal_game_validation.exe`: 2/2 build steps, exit 0.

### Focused GREEN attempts and new deterministic blocker

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
ctest --preset windows-msvc-debug `
  -R '^stage10\.formal_game\.capture_after_present$' `
  --output-on-failure -V
```

- First post-change run failed 0/1 in 104.80 seconds (CTest total 104.82),
  executable code 12. Only captures 01 and 02 existed; capture 03 was absent.
- Decoded both final capture-3 V8 save slots directly from the documented
  checkpoint offsets. `run_a.sav` was generation 7 and `run_b.sav` generation
  8; both were root 37, room 1, depth 1, floor room 2, ecology chaos,
  `is_abyss=1`, abyss lifecycle `cleared`, danger medium, rule
  `hunting_flames`, resolution invalid, death sequence 0, and death lifecycle
  none. Both slots had valid V8 magic/format and were written at the same test
  time; `run_b.sav` was exactly one current item record larger.
- Therefore capture-3 entered and cleared the required abyss, saved
  successfully, and did not die. The failure is that the existing 64-step
  presented-frame sampling did not observe a hunting-flame telegraph frame;
  it is not save I/O failure and not the original code-13 dense-room death.
- An unchanged focused rerun reproduced code 12 in 35.59 seconds (CTest total
  35.61), again with only captures 01 and 02. This makes the new failure
  deterministic rather than a one-off resource timeout.
- The content validator was not run because its required seven-image input
  does not exist. Capture 04 was never attempted on either post-change run, so
  its abyss/rule evidence cannot be claimed.

### Files, staging, and commit

- Source working-tree file:
  `tests/dungeon/stage10_formal_game_validation.cpp`.
- Existing unrelated Task 8 dirty tests remained untouched and unstaged:
  `tests/dungeon/dungeon_abyss_stress_tests.cpp`,
  `tests/dungeon/dungeon_stress_tests.cpp`, and
  `tests/dungeon/stage16_loot_reinforcement_simulation.cpp`.
- No file was staged and no commit was created because the focused formal gate
  is still red. Requested commit message
  `test: stabilize stage10 dense-room validation build` remains unused.

## Updated current status

`BLOCKED` — the only authorized implementation exactly mirrors the established
dense-room validation equipment build and fixes the original equipment gap,
but it deterministically skips the earlier hunting-flames warning capture and
returns code 12. Producing seven images would require changing a prohibited
driver/capture/simulation contract or receiving a revised authorized fixture;
the focused formal gate and content validator cannot be reported green.

## Stage 10 defensive-only follow-up

The brief was narrowed after the two deterministic code-12 runs: preserve the
generated weapon, base IDs, affixes, required levels, and offensive timing;
apply +15 only to the five generated non-weapon slots and validate each item.

### Final allowed source diff

- Kept the explicit `items/item_catalog.hpp` declaration include needed for
  `validate_item`.
- Kept every generated item unchanged except
  `if (index > 0U) item->reinforcement = 15U;`, followed by
  `validate_item(*item)`.
- Removed the full-build base-ID table, explicit affix table, required-level
  override, and weapon reinforcement. No driver, seed, 64-step simulation,
  capture predicate, 16-frame requirement, timeout, or validator changed.
- `git diff --check -- tests/dungeon/stage10_formal_game_validation.cpp`
  passed.

### Build and focused result

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_stage10_formal_game_validation -- -j1
```

- Reconfigured, compiled, and linked successfully: 2/2 build steps, exit 0.

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
ctest --preset windows-msvc-debug `
  -R '^stage10\.formal_game\.capture_after_present$' `
  --output-on-failure -V
```

- Focused formal still failed 0/1 in 64.21 seconds (CTest total 64.23),
  executable code 13. Captures 01-03 were present and capture 04 was absent.
- Capture-4 `run_a.sav` was valid V8 generation 3: root 2068, room 0,
  depth 1, floor room 1, chaos ecology, ordinary room, `death_retreat`, abyss
  lifecycle none, rule none, resolution invalid, death sequence 1. The death
  was from monster 6 `chaos_chaser`, chaos damage, raw damage 14, final/health
  loss 1, HP 0/1000, barrier 0/35, armor 100540, evasion 19150, armor
  reduction 8001 bp, and evasion rate 6203 bp.
- Capture-4 `run_b.sav` was the valid generation-2 pre-death slot for the same
  room, with death sequence 0 and no death record.
- Thus the defensive-only build materially reduced each final hit but still
  died in the same ordinary dense room before entering the chaos abyss. The
  content validator was not run because only three of seven images exist.

### Final staging and commit status

- No file was staged and no commit was created because the revised focused
  formal gate is red. The three pre-existing Task 8 dirty tests remain
  untouched and unstaged.
- Final status remains `BLOCKED`: the newly authorized defensive-only change
  does not survive root 2068, while increasing weapon offense or changing the
  driver/capture contract is outside the current brief.

## Stage 10 public-skill driver follow-up

### Fixture cleanup and first authorized repair

- Removed the failed defensive-only fixture experiment from
  `tests/dungeon/stage10_formal_game_validation.cpp` with an exact reverse
  patch: removed `items/item_catalog.hpp`, restored the generated item to
  `const`, and removed non-weapon reinforcement plus `validate_item`.
- The normalized working-tree blob is byte-for-byte the HEAD blob
  `277737a4931b1512b82b757a42e6ae4ecd2f286e`; both unstaged and staged
  `git diff --quiet` checks return zero for the formal fixture.
- Modified only `stage10_validation_input` in
  `src/platform/raylib/raylib_host.cpp`: after confirming a combat snapshot
  and entering the combat branch, ordinary rooms request public active-skill
  slot 1. Abyss rooms do not request a skill. The existing light attack,
  movement, seed search, 64-step sampling, predicates, 16-frame gate, timeout,
  and screenshot validator remain unchanged.
- `git diff --check` passed. `arpg_raylib` and
  `arpg_stage10_formal_game_validation` rebuilt successfully.

The first focused formal run returned code 14 in 37.14 seconds. An unchanged
confirmation run returned code 14 again in 70.91 seconds. Both produced fresh
captures 01-04 but no capture 05, so the failure had moved past the original
root-2068 capture-4 death but had not completed the formal route.

The first capture-5 V8 evidence decoded as root 2345, room 0, depth 1, floor
room 1, chaos ecology, ordinary room, `death_retreat`, abyss lifecycle none,
rule none, resolution invalid, and death sequence 1. The final damage was a
monster attack from monster ID 2 (`water_bulwark`), water damage, raw damage
21, final/health loss 3, HP 0/1000, barrier 0/35, armor 35275, evasion 19150,
armor reduction 6561 bp, and evasion rate 6203 bp. It died before entering the
selected life-sacrifice abyss.

### Approved slot-0 fallback and remaining RED

After architecture review, the brief was updated to allow slot 0 only when
slot 1 was not accepted. The ordinary-room branch was changed to:

```cpp
const auto storm = session.request_active_skill_slot(1U);
if (storm != combat::SkillCastResult::accepted) {
    static_cast<void>(session.request_active_skill_slot(0U));
}
```

No skill is requested in abyss rooms. The two required targets rebuilt
successfully. The one requested complete formal rerun still failed 0/1 with
code 14 in 41.72 seconds and again produced only captures 01-04.

Fresh capture-5 `run_b.sav` was valid `ARPGSV8`, format 8, generation 2,
1172 bytes: root 2345, room seed 1166841726865174716, room 0, depth 1, floor
room 1, chaos ecology, ordinary room, `death_retreat`; abyss lifecycle none,
rule none, resolution invalid. Its death record was sequence 1, lifecycle
pending, monster attack, monster ID 2 (`water_bulwark`), water damage, raw 21,
barrier loss 0, health/final loss 3, HP 0/1000, barrier 0/35, armor 35275,
evasion 19150, armor reduction 6561 bp, and evasion rate 6203 bp. The retreat
target was ordinary chaos room 1, seed 6754013345117110423. Capture 05 never
entered an abyss.

Production-source audit explains why the fallback can be consumed before a
useful attack lane exists. `CombatWorld::request_active_skill` has no target,
distance, or attack-lane precondition. It accepts storm immediately in combat
and locks its center from the player's current position/facing; the driver
requests it before `nearest_living_monster`, movement, and
`validation_attack_lane` are evaluated. While storm is cooling down, slot 0
is likewise accepted at the first tick where no active skill/basic attack or
player-unavailable gate blocks it, again without a target/range check. The
save format does not record individual skill requests, so this is a direct
production-code conclusion rather than a runtime-log claim.

### Current gate and staging status

- The focused formal gate is still red; captures 05-07 and the formal
  death/reset/restart/hole summary were not reached.
- The Stage 10 evidence guards, transaction fixture, validation game, content
  validator, abyss stress, and skill-input platform tests were not run after
  this RED because the brief requires the complete formal gate first.
- No file was staged and no commit was created. The three Task 8 test changes
  remain untouched and unstaged.
- Current status: `BLOCKED`. Further driver changes require a revised approved
  condition; the current authorized slot-1/slot-0 policy does not survive the
  deterministic root-2345 ordinary room.
