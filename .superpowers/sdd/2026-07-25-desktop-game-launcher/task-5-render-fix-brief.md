# Task 5 render-fix brief

Work only in `E:\game\.worktrees\desktop-game-launcher` on branch `codex/desktop-game-launcher`.

## Proven failure

- The deployed native launcher opens with the exact 920x560 client area, but the entire client area remains pure white after waiting and forced redraw.
- `launcher.window_smoke` passes because it checks only GUI subsystem, process/window handle, and title.
- `LauncherWindow::paint()` ignores the `HRESULT` returned by `LauncherRenderer::render()`.
- Initialization succeeded far enough to create the window; do not guess which Direct2D call fails.

## Required TDD/debug sequence

1. Add a focused real-client-pixel smoke assertion that fails on the current white window. Capture only the client area after DWM has flushed; assert substantial dark-background coverage and pixels from at least one button/text region. Keep it robust to DPI scaling and do not rely on OCR.
2. Add the minimum stage-aware HRESULT diagnostics needed to run the current launcher once and identify the exact failing Direct2D API/path. Record the observed stage and numeric HRESULT in the report.
3. Fix only the proven failure.
4. Permanently stop swallowing render failures. On a render/resource failure, discard invalid Direct2D resources and request a repaint/retry where appropriate. If rendering still cannot recover, paint a dark, legible native fallback instead of leaving a white client area.
5. Run only launcher-focused Debug and Release builds/tests plus the new pixel smoke test. Do not run the full 115-test suite, do not deploy the final launcher, and do not start the game.
6. Run `git diff --check`, inspect the diff, commit the reviewed scope with message `fix: render desktop launcher client`, and write `.superpowers/sdd/2026-07-25-desktop-game-launcher/task-5-render-fix-report.md`.

## Constraints

- Preserve the approved layout, Chinese copy, pure colors, no-overlap requirements, install paths, gameplay behavior, and all unrelated dirty/untracked files.
- Do not modify the main checkout `E:\game`, reset Git, or delete files.
- Do not loosen a test to make it pass.
- Keep temporary diagnostics out of the production UI unless they are useful permanent error reporting.
- Monitor resource use; never start a second build/test chain while one is alive. If `taskkill.exe`/`conhost.exe` repeatedly respawn, capture parent PID and command line before taking action.
