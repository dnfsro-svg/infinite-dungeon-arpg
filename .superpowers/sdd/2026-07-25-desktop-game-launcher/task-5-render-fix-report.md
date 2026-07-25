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

The stage-aware diagnostic build recorded:

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
