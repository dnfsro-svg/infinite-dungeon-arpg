# Task 1 report: combat HUD skill avoidance

## Preflight

- Worktree: `E:\game\.worktrees\loot-suction-animation`
- Branch: `codex/full-chinese-ui`
- Required base HEAD: `1a67c463a287d7c6f7e0e27525263c57691611fa`
- Initial `git status --short --untracked-files=all`: clean.
- Read the repository and worktree `AGENTS.md`, then followed the task brief.

## RED

After extending the existing `hud_layout_is_bottom_centered_with_scaled_slot_geometry()` case, the prescribed focused command exited `2` during compilation. The first relevant error was:

```text
active_skill_view_tests.cpp(523): error C2039: "CombatHudLayout": 不是 "arpg::platform" 的成员
```

The same build also reported `make_combat_hud_layout` missing. This is the intended API-missing business RED, not an environment or include failure.

Command used (one PowerShell process):

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Remove-Item Env:ARPG_*_ONLY -ErrorAction SilentlyContinue
$env:ARPG_ACTIVE_SKILL_VIEW_ONLY = '1'
& .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
$code = $LASTEXITCODE
Remove-Item Env:ARPG_*_ONLY -ErrorAction SilentlyContinue
exit $code
```

## Implementation

- Added `hud_layout_avoiding_bottom_blocker()` with finite, positive-area and positive-scale guards. It translates both notices by the same delta and preserves their gap.
- Added `CombatHudLayout` and a shared active-skill layout helper that shifts the complete skill-strip visual candidate to the player panel's right with a `12 * scale` clearance, without changing skill scale.
- `make_combat_hud_layout()` owns the single frame composition: base HUD, skill geometry, then notice avoidance; it marks the composition valid only when active skills and both moved notices are valid/in the safe area.
- The public two-argument skill-layout entry point delegates through the same player-blocker geometry. The existing case compares its bounds and visual bounds with the composed result, retaining door-prompt geometry consistency.
- `CombatRenderer` caches the composed active-skill layout during HUD observation and draws that cache after the ordinary HUD, preserving draw order.

## GREEN

Re-ran the exact focused command above after the minimal implementation. Exit code: `0`.

Exact final test output:

```text
11 cases, 0 failures
```

No CTest, `scripts/Test.ps1`, Release build, windowed producer, or long source-guard self-test was run.

## Scope and self-review

- `git diff --check`: exit `0`, no whitespace errors.
- Before staging, `git status --short --untracked-files=all` listed exactly these seven modified task files:
  - `src/platform/raylib/hud_layout.hpp`
  - `src/platform/raylib/hud_layout.cpp`
  - `src/platform/raylib/active_skill_view.hpp`
  - `src/platform/raylib/active_skill_view.cpp`
  - `src/platform/raylib/combat_renderer.hpp`
  - `src/platform/raylib/combat_renderer.cpp`
  - `tests/platform/active_skill_view_tests.cpp`
- `git diff --cached --check`: exit `0`; staged exactly the seven files above.
- Self-review checked the four literal viewport positions, player/no-notice overlap, notice clearance/safe-area containment, scale preservation, viewport rejection, cached renderer ownership, and public entry-point equivalence.
- Independent read-only review reported no Critical findings. It raised a prospective direct-cache concern for `room_renderer.cpp`; it is not changed because the brief explicitly limits this task to seven files and requires the public entry point to be geometrically identical to the rendered composition, which the focused regression assertion enforces.

## Commit

- Commit: `9de899b` — `fix(ui): keep combat HUD clear of active skills`
- The report itself is intentionally untracked and excluded from the seven-file implementation commit.
