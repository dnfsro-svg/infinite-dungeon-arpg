# Stage 11-C Task 4 Report — HUD Font, Palette and Drawing Resources

## Scope

GREEN. Only Task 4 is implemented: the shared fixed-capacity Chinese font plan,
HUD palette, and the standalone `HudRenderer` font lifecycle. No Task 5 panel
drawing, Task 6 context panels, Task 7 host integration, gameplay state,
input, RNG, ticks, settings, macOS, or item filtering changed.

## RED

The new `hud_font_tests.cpp` and test registration were added before the HUD
font/palette production APIs existed. After loading the repository's VS x64
environment, the expected build RED was:

```text
hud_font_tests.cpp(4): fatal error C1083: cannot open include file:
'hud_font.hpp': No such file or directory
```

The initial sandboxed build could not write the existing `out/build` tree and
the non-developer shell lacked the pinned Windows SDK environment. The same
build under `VsDevCmd.bat -arch=x64` reached the missing-API RED above.

## Changes

- Added `HudFontPlan`, which copies the existing `DeathOverlayFontPlan` and
  checks all required Task 4 text without a second UTF-8 parser or allocation.
- Extended the existing death-overlay fixed codepoint source with `剩余`,
  `出口已开放`, `未分配点`, `减速`, and `无敌`; its existing text already
  supplies the remaining labels and four elemental names.
- Added a single fixed `HudPalette`; all nine key colors are pairwise distinct
  and fully opaque.
- Added `HudRenderer` ownership: it tries the shared candidate paths with
  `LoadFontEx`, verifies every requested glyph has the exact codepoint,
  reports `font_ready()` only for a verified Chinese font, and falls back to
  `GetFontDefault()` with `font_ready()==false`. `shutdown()` and destruction
  unload only an owned font and are idempotent. `draw()` deliberately has no
  panel output until Task 5/6.

## Tests and evidence

```powershell
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game'
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 && ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|stage11.death_formal.five_paths|stage11b.settings_formal)$" --output-on-failure'
git -C E:\game\.worktrees\stage11c-complete-hud diff --check
```

- Build: `arpg_platform_tests` and `arpg_game` linked successfully.
- Direct platform test: `210 cases, 0 failures`.
- CTest: `platform.units`, `stage11.death_formal.five_paths`, and
  `stage11b.settings_formal` all passed (3/3). `stage11.death_formal.five_paths`
  is the actual registered corrupt/death-notice formal item; no separate test
  named "corrupt-notice" exists in this build.
- Resource lifecycle test proves repeated pre-initialization shutdown is safe
  and leaves `font_ready()` false. The production lifecycle separately verifies
  exact glyph values before granting `font_ready()`.
- `git diff --check` has no whitespace errors (only repository CRLF conversion
  notices).

## Self-review and concerns

- The existing `hud_renderer.cpp` still owns legacy `CombatRenderer` output;
  this task only appended `HudRenderer` resources. Its consumption and panel
  replacement remain Task 5–7 work.
- Actual Chinese glyph rendering is intentionally not claimed here; the
  renderer will report failure instead of treating the default font as Chinese
  ready. Task 9 formal screenshots own visual proof.
- No unresolved Task 4 implementation issue found.

## Commit

`feat: load chinese hud drawing resources`
