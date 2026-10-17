# Task 5 render-fix review round 1

Base implementation: `fed1d3b`.

## Required fixes

1. Remove renderer-owned invalidation for `D2DERR_RECREATE_TARGET`; the controller alone may request at most one retry so a persistent device failure cannot create an infinite `WM_PAINT` loop.
2. Add a `resize` renderer stage and return/handle the Resize HRESULT. A transient failure may be retried, but its stage/result must not be silently discarded.
3. Restore the exact base window title after a later successful/occluded-compatible paint.
4. In the OCCLUDED fallback, show `view.status_text` using pure `RGB(0,255,0)` or `RGB(255,0,0)` according to `view.status_ready`; reserve the render failure message for actual failed HRESULTs.
5. Preserve fallback interaction fidelity used on this machine: disabled/normal/pressed button fill, hovered/focused border, path box and full-path tooltip when `show_full_path` is true. Keep Chinese text readable and layouts scaled from DIPs.
6. Strengthen the smoke test:
   - read `GetDpiForWindow` and assert physical client size equals 920x560 scaled by that DPI;
   - move the window fully inside one monitor work area, use a short bounded capture retry, and verify ownership immediately around capture;
   - assert the copied launcher's disabled Start fill is approximately `#555B63`, another enabled button fill is approximately `#24455E`, high-contrast Start text is spatially distributed, and the missing-assets status ROI contains pure-red text pixels;
   - reset TOPMOST in normal cleanup; mark the CTest `interactive;windows-ui;launcher`, `RUN_SERIAL`, with a bounded timeout.
7. Simplify `RenderTargetDiagnostics` to only the window-state information production actually uses unless another field has a permanent test/use.
8. Amend the report to label the exact paint-count/HRESULT/sample numbers as session observations from temporary instrumentation rather than replayable commit artifacts.

Run only launcher-focused Debug/Release builds/tests and `git diff --check`; do not deploy or run the game. Commit the review fix separately and update the report.
