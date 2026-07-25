# Task 5 desktop launcher render-fix report

## Scope

- Worktree: `E:\game\.worktrees\desktop-game-launcher`
- Branch: `codex/desktop-game-launcher`
- No deployment was performed and no game process was started.

## RED

The initial client-pixel assertion was corrected to use Per-Monitor-V2 coordinates before trusting its result. The unmodified Release launcher then failed:

```powershell
& .\tests\launcher\launcher_window_smoke_test.ps1 `
  -Launcher .\out\build\windows-msvc-release\bin\无限地下城启动器.exe
```

Observed result: physical client `1150x700`, only `366 / 50400` sampled pixels were dark (required at least 55%). The expected button/text region was also absent. This is a real white-client failure, not DPI capture virtualization.

## Stage and HRESULT diagnosis

Temporary session instrumentation in the original investigation recorded the following values. These are session observations, not replayable artifacts of the final commit:

```text
paint_count=3
stage=ID2D1RenderTarget::EndDraw
hresult=0x00000000
window_state=0x00000001 (D2D1_WINDOW_STATE_OCCLUDED)
pixel_size=1150x700
logical_size=920.0x560.0
dpi=120.0x120.0
```

The test also recorded `visible=True`, `iconic=False`, `cloaked=0`; all four client corners, center, button region, and path region resolved to the launcher PID. A controlled `ShowWindow(SW_RESTORE)`, `BringWindowToTop`, `SetForegroundWindow`, synchronous `RedrawWindow`, and `DwmFlush` still produced the white client. Changing only `D2D1::RenderTargetProperties` to `SOFTWARE` remained RED (`985 / 50400` dark samples), so that experiment was reverted.

Root cause: in this session, the HWND Direct2D render target can report `OCCLUDED` and present no pixels despite a successful `EndDraw`, a visible non-cloaked window, correct DPI, and repeated paints.

## Fix

- Added a real client-pixel smoke assertion after DWM flush, with Per-Monitor-V2 capture, multi-point ownership checks, substantial dark-background coverage, and start-button fill/text coverage.
- Propagated render failures with API stage plus HRESULT. Button text and resize failures no longer disappear silently.
- On a render/resource failure, invalid Direct2D resources are discarded and one repaint retry is requested.
- When the Direct2D target remains unavailable/occluded, the same client area uses a dark, readable GDI fallback with the approved launcher layout and Chinese copy instead of a white window.

## GREEN

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_launcher
ctest --test-dir out/build/windows-msvc-debug `
  -R '^launcher\.(units|manifest|window_smoke|icon_asset|deploy_self_test)$' --output-on-failure

. .\scripts\Configure.ps1 -Preset windows-msvc-release
cmake --build --preset windows-msvc-release --target arpg_launcher
ctest --test-dir out/build/windows-msvc-release `
  -R '^launcher\.(units|manifest|window_smoke|icon_asset|deploy_self_test)$' --output-on-failure
```

Both configurations built `arpg_launcher` successfully and passed all five focused launcher tests, including `launcher.window_smoke`.

## Resources and commit

- Render resources: Direct2D target and brushes are released after a failed render; fallback is immediate when `D2D1_WINDOW_STATE_OCCLUDED` persists.
- Resource monitoring found no concurrently active `cmake`, `ninja`, `cl`, `link`, or `msbuild` chain before each build.
- Commit: `fix: render desktop launcher client`

## Review round 1

- `D2DERR_RECREATE_TARGET` and `Resize` now return stage-aware `RendererResult` values without renderer-owned invalidation. The controller owns resource discard and a single retry latch, and displays the failure stage/HRESULT when a failure reaches the UI.
- A later successful paint, including an OCCLUDED-compatible fallback paint, restores the exact base title and clears the retry latch.
- The fallback now uses `LauncherView`: current ready/error status text with pure green/red status color, disabled/normal/pressed fills, hover/focus cyan borders, a path box, and the full-path tooltip.
- `launcher.window_smoke` constructs a missing-`assets/skills` fixture and verifies physical DPI dimensions, monitor placement, bounded ownership-checked capture, fallback colors/text distribution/status color, hover/pressed state, and tooltip. It is `interactive;windows-ui;launcher`, `RUN_SERIAL`, with a 20-second timeout.
- Review commit: `fix: harden launcher render fallback`

## Scoped test fix round 2

- The initial capture now retries up to three times until both pre/post multi-point ownership and the key rendered pixels are ready: dark coverage, disabled Start fill, Verify fill, and the missing-assets red status text.
- Only the final unsuccessful capture reports dark-sample count, Start and Verify RGB values, and pure-red pixel count.
- Placement now uses normal `HWND_TOP` foreground ordering rather than persistent `HWND_TOPMOST`; the `HWND_NOTOPMOST` cleanup is no longer needed. Physical-DPI sizing, monitor-workarea placement, `RUN_SERIAL`, and the 20-second timeout remain unchanged.
- The focused five-test launcher suite passed in both Debug and Release; `launcher.window_smoke` completed in 6.21 seconds and 6.14 seconds respectively.

## Scoped test fix round 3

- `launcher.window_smoke` creates a dedicated Windows Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` before launching the copied launcher and assigns that child process immediately. Assignment failures include their Win32 error; they are never skipped.
- On the normal path the test still sends `CloseMainWindow` and waits before closing the job handle. If CTest or the system terminates the test, closing the inherited job handle causes Windows to reclaim the launcher process.
- The fixture now uses the dedicated `arpg-launcher-window-smoke-` Temp prefix. Before each run it removes only stale direct children of the system Temp directory with that prefix; normal `finally` cleanup remains in place, and a force-killed run is reclaimed on the next test start.
- The focused five-test launcher suite passed in both Debug and Release; the Job Object was created and assignment succeeded in the current CTest environment.
