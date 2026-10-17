# Task 5 Report — Kill drops, fixed ground pool, and snapshots

Status: `DONE_WITH_CONCERNS`

Commit: `2eabc1b feat: drop and snapshot ground health potions`

## Scope completed

- Added exactly four focused health-potion cases and registered totals `7` and `294`.
- Integrated coupon-first secondary drops while leaving the ordinary material branch unchanged.
- Added the fixed 192-slot ground-potion array, direct spawn-ordinal placement, no-overwrite saturation diagnostics, ascending snapshot packing, and pickup-receipt copying.
- Preserved ground potions through ordinary room clear and cleared them on room construction, transient reset, and committed transition/abyss-abandon paths.
- Did not implement pickup, consumption, healing, persistence, UI, or `MaterialId` mapping.

## TDD evidence

Valid RED after adding/registering all four tests and before production changes:

```text
7 cases, 4 failures
[FAIL] dungeon_health_potion.coupon priority and common potion coexistence: coexist_snapshot.ground_health_potion_count == 1U
[FAIL] dungeon_health_potion.potions preserve material and coupon rolls: snapshot.ground_health_potion_count != 0U
[FAIL] dungeon_health_potion.fixed potion pool boundaries and no overwrite: full.ground_health_potion_count == arpg::dungeon::kGroundHealthPotionCapacity
[FAIL] dungeon_health_potion.potion snapshot sorted and room exit clears: installed.ground_health_potion_count == 3U
```

Focused GREEN:

```text
7 cases, 0 failures
```

Focused command:

```powershell
$env:ARPG_HEALTH_POTION_ONLY='1'
& .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
Remove-Item Env:ARPG_HEALTH_POTION_ONLY
```

Full dungeon verification:

```text
294 cases, 1 failures
```

The only failure was the pre-registered Task 8 golden mismatch:

```text
dungeon_abyss_stress.1000 room abyss trace matches golden through reloads
expected: 0xe102b17e6423351b
actual:   0x0d55b41ebb0b1128
```

No golden value was modified. No other dungeon failure occurred.

## Changed and committed files

- `tests/dungeon/dungeon_health_potion_tests.cpp`
- `tests/dungeon/dungeon_test_support.hpp`
- `tests/dungeon/dungeon_test_main.cpp`
- `src/dungeon/dungeon_session.hpp`
- `src/dungeon/dungeon_session.cpp`
- `src/dungeon/dungeon_snapshot.cpp`
- `src/dungeon/dungeon_transition.cpp`

## Concerns

- The known `dungeon_abyss_stress` golden hash remains stale by instruction and is deferred to Task 8.
- This report is intentionally untracked and is not part of commit `2eabc1b`.
