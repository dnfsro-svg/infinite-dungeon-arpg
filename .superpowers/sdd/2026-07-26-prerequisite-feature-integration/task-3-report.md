# Task 3 implementation report

## Status and commits

- Status: DONE
- Start HEAD: `39bc3b125cd96984d431375b56e17797a2988863`
- Catalog/fixture commit: `88f8a3b415d4f310242380f58d88452734abc75d`
  (`balance: sharply reduce monster base stats`)
- Final HEAD: `17cbb0a01f4a4e039574e689986fb0a5256c42ed`
  (`balance: suppress monster affixes through depth three`)
- No push was performed.

## Commit 1 changed files

- `src/combat/monster_catalog.cpp`
- `tests/combat/break_stress_tests.cpp`
- `tests/combat/combat_test_main.cpp`
- `tests/combat/monster_affix_runtime_tests.cpp`
- `tests/combat/monster_affix_trigger_tests.cpp`
- `tests/combat/monster_catalog_tests.cpp`
- `tests/combat/monster_melee_tests.cpp`
- `tests/combat/player_build_tests.cpp`
- `tests/combat/player_death_snapshot_tests.cpp`

The catalog-order `(HP, damage)` readback is exactly:

1. `(100,36)`
2. `(190,27)`
3. `(315,21)`
4. `(135,0)`
5. `(110,12)`
6. `(125,18)`
7. `(120,14)`
8. `(145,11)`

The catalog diff changed only `max_hp` and the matching elemental contact
damage for those eight entries. Movement, timing, threat cost, tags, shields,
break, projectile, feedback, and support fields were unchanged. The two
source commits applied without conflicts; no old registry side was selected.

## Commit 2 changed files

- `src/combat/monster_affix_generation.cpp`
- `tests/combat/combat_test_main.cpp`
- `tests/combat/monster_affix_generation_tests.cpp`
- `tests/dungeon/dungeon_abyss_stress_tests.cpp`
- `tests/dungeon/stage16_loot_reinforcement_simulation.cpp`

The first depth band is exactly `100/0/0/0`. After catalog, canonical monster,
and input-affix validation, abyss supplementation returns an empty set when
`depth <= 3U`. The depth-4+ weight tables and execution path are unchanged.
The combined combat registry is 243 cases.

## Deterministic golden trace

The initial combined gate exposed only the two frozen fingerprints allowed by
the plan. A temporary diagnostic was applied identically to the current tree
and an ignored `88f8a3b` source snapshot, then fully removed from tracked
files before the final diff and commit.

- Encounter structure hash, excluding monster affixes:
  `0x7ab72c7b7705e29e` in both versions.
- Shallow encounter population: 3037 monsters in both versions.
- Shallow affix count: 636 before, 0 after.
- Shallow affix hash:
  `0xe62b1c9a7b4d88a7` before,
  `0x21722e3cadf1d868` after.
- Fixed normal+abyss generation control for depths
  `4/9/10/19/20/39/40`, all monster IDs, and seeds `0..255`:
  `0xc37d42c2c85c07b0` in both versions.
- Door trials/hits stayed exactly `4000` and `9/11/9/7`; reload-free and
  every-37-room-reload traces stayed identical.

This proves that the dungeon trace changed only through the approved shallow
affix suppression, so its golden changed from `0xe102b17e6423351b` to
`0x0d55b41ebb0b1128`.

The Stage 16 fingerprint iterates seeds 1..512 at initial depth 1 and folds
room/equipment/abyss data plus encounter affixes. Its own material-RNG
interleaving equality still passed, while only the old golden comparison
failed. Together with the equal encounter-structure and depth-4+ controls
above, this proves the same sole semantic change; the approved new fingerprint
is `10882612280735857555ULL` (old `15972885342817959559ULL`).

## Commands and results

1. First catalog build attempt:
   `cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1`
   failed during CMake regeneration because the invoking PowerShell had no
   `WindowsSDKVersion` (`detected ''`). No code was changed for this failure.
2. Environment-correct build:
   dot-sourced `./scripts/Configure.ps1 -Preset windows-msvc-core-debug` in
   the same PowerShell, then ran the same serial build. Build passed 60/60.
3. `ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure`
   passed 1/1; direct executable passed 242 cases before the affix case.
4. Final three-target serial build:
   `cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests arpg_dungeon_tests arpg_stage16_loot_reinforcement_simulation -- -j1`
   passed.
5. The first exact combined CTest found only the dungeon golden mismatch;
   after its traced update, combat passed and dungeon passed, then Stage 16
   exposed only its traced frozen fingerprint mismatch.
6. Focused Stage 16 after its traced update passed 1/1 in 44.21 seconds.
7. Final exact combined gate:
   `ctest --preset windows-msvc-core-debug -R '^(combat|dungeon)\.units$|^stage16\.loot_reinforcement\.simulation$' --output-on-failure`
   passed 3/3, 0 failures: combat 1.09 seconds, dungeon 133.08 seconds,
   Stage 16 45.12 seconds, total 179.30 seconds.
8. Direct `arpg_combat_tests.exe` passed 243 cases, 0 failures.
9. `git diff --check` and `git diff --cached --check` passed before each
   commit. Final tracked status is clean.

## Scope and self-review

- `742ce70`, `b57930e`, and `2422d13` are not ancestors of final HEAD and
  were not imported.
- No launcher, potion, material, room-size, camera, framerate, threading,
  GPU, or control-checkout path changed.
- No cherry-pick conflict occurred.
- All builds were serial; no second build ran concurrently.
- The temporary `Q:` path mapping used only to avoid a Windows diagnostic
  snapshot path-length limit was removed, and no build/test process remained.
- Concerns: none.
