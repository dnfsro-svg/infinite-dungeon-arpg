# Stage 11-C Task 2 Report

## Status

DONE

## RED evidence

After registering all notice tests, the project MSVC/SDK configure completed and
the platform build failed as required before production API existed:

```text
hud_notice_state_tests.cpp(3): fatal error C1083: 无法打开包括文件:
“hud_notice_state.hpp”: No such file or directory
```

During review, an additional test-first RED proved unchanged context was
repeatedly incrementing the overflow diagnostic:

```text
[FAIL] hud_notice_state.unchanged context has no repeat overflow:
state.dropped_count() == 1U
```

## Changed files and behavior

- `src/platform/raylib/hud_notice_state.hpp/.cpp`: fixed four-slot, stable
  priority notice state; pause-safe expiration; persistent save/recovery state;
  room-context clearing; bounded ControlHints extraction without allocation.
- `tests/platform/hud_notice_state_tests.cpp`: nine behavior tests covering
  every priority, two-line view, timer freeze/saturation, signal de-duplication,
  room cleanup, overflow, persistence, and non-NUL rebound hints.
- Raylib/platform test CMake and test main register the module and update the
  platform total to 199 cases.

## GREEN evidence

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
```

Succeeded. Direct platform executable result: `199 cases, 0 failures`.

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.input_latency_source|stage11b.settings_formal)$" --output-on-failure
```

Succeeded: `3/3` tests passed in `15.50 sec`.

```powershell
git -c safe.directory=E:/game/.worktrees/stage11c-complete-hud diff --check
```

Succeeded with no whitespace errors.

## Commit

`40244a77d2267f0598af1f80f92645820cd8acd2` —
`feat: prioritize bounded hud notices`.

## Self-review

- Only Task 2 files changed; no HUD drawing, layout, host integration, session,
  store, RNG, input sampling, or fixed-tick code was touched.
- Queue capacity is `std::array<HudNotice, 4>` and text is `HudText96`; no
  dynamic containers or string allocation are used.
- All `ControlHints` scans are limited to the fixed 160-byte array and each
  output buffer is explicitly NUL-terminated.

## Concerns

None. The report is ignored by `.git/info/exclude` and is intentionally not
part of the Task 2 commit.

## Review Fixes

Review requested two Important fixes. Both were added test-first before the
production change:

- `room clear edge without commit` first failed because a real
  `combat -> cleared` publication in the same room/generation was rejected by
  the queue gate. The fix observes the phase edge and suppresses only a repeat
  clear for the same room with independent clear-observation state.
- `room zero context transition` first failed because room index zero was used
  as an uninitialized sentinel. The fix uses an explicit observation flag, so a
  `room 0 -> room 1` change clears old interaction context immediately.

RED evidence after adding the tests: `201 cases, 2 failures`, exactly the two
new named regressions. Final GREEN evidence: platform/game build succeeded,
direct platform executable reported `201 cases, 0 failures`, and the required
CTest expression passed `3/3` in `15.33 sec`; final `git diff --check` was
clean. Review-fix commit:
`c1e2791761ca3285bdfd809039d2b2938cd83484` —
`fix: correct hud notice observation edges`.
