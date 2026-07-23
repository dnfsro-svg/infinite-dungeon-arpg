# Native fire room background build report

## RED

- Command: `E:\codex1\python-dev\Python313\python.exe -m unittest tests.platform.native_room_background_asset_pipeline_tests -v`
- Result: expected FAIL (1 failure, 1 pass).
- Failure reason: `native room background builder is missing`; the builder, build
  report and three required outputs did not exist before the implementation.

## GREEN

- Command: `E:\codex1\python-dev\Python313\python.exe -m unittest tests.platform.native_room_background_asset_pipeline_tests -v`
- Result: PASS (2 tests, 0 failures, no warnings).
- The test invokes the actual builder, verifies the exact source manifest,
  source/output SHA-256 entries, all recorded placement scales, exact dimensions
  and modes, and byte-for-byte single-pass LANCZOS runtime derivation.

## Outputs and zero-upscale proof

| Output | Pixels | SHA-256 |
| --- | ---: | --- |
| `art_source/stage12/backgrounds/fire/fire-room-background-master.png` | 3840 x 2160 RGBA | `5bab11b0b5ad2745bebded1b810a085ac72a7641143ebb21cc1c54d5a048e57d` |
| `assets/stage12/fire_room_background.png` | 2560 x 1440 RGBA | `e24e7f866386aa00fea5f1df6105e968c9fd8c32ed654c383f6c61d0b29dd30a` |
| `assets/stage12/fire_room_background_material.png` | 2560 x 1440 RGBA | `69ab6e3bcb4ff9a345e8f1f9bf18d511a33d253a996acba81cba94b5343c78c0` |

- Inputs are only the two declared files under
  `art_source/stage12/backgrounds/fire/`; the manifest does not reference old
  `assets/stage12/fire_environment.png`.
- Build report: `assets/stage12/room-background-build.json`.
- Placements: 19 native crop placements; maximum `scale_x = 1.0`, maximum
  `scale_y = 1.0`. No source `resize()` is used.
- Runtime is the master resized exactly once with `Image.Resampling.LANCZOS`.
- ROI check: central luminance standard deviation `13.5502`, top-edge
  luminance standard deviation `16.0006`; central ROI is not a solid color.

## Modified files

- `tools/build_native_room_backgrounds.py`
- `tests/platform/native_room_background_asset_pipeline_tests.py`
- `art_source/stage12/backgrounds/background-sources.json`
- `art_source/stage12/backgrounds/fire/fire-room-background-master.png`
- `assets/stage12/fire_room_background.png`
- `assets/stage12/fire_room_background_material.png`
- `assets/stage12/room-background-build.json`
- `.superpowers/sdd/native-background-task-1-report.md`

## Commit

- Build delivery commit: `3f0752b2b4c3fee72eeff934b9bc041364862bc8`
  (`feat: author native fire room background`).

## Visual inspection and concerns

- Opened the 3840 x 2160 master and 2560 x 1440 runtime output. Both retain
  the dark-steel/ancient-gold/ember structural frame, have a clear unoccupied
  central combat area, and contain no character, monster, UI, text or bright
  central interaction object.
- No obvious crop seam was observed at either inspected size. The intentionally
  restrained ember treatment keeps the room dark; runtime renderer integration
  and four-direction gameplay layers remain outside this offline Task 1 scope.
