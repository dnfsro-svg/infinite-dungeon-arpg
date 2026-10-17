# Task 10 implementation report

Base: `d55585e`

Commit: `db1271d test: validate material runtime integration`

## Scope delivered

- Added a production-room real-raylib fixture covering cross-ecology residency, four doors, both active-skill timelines, nine ecology/resolution scenes, the death overlay, fallback behavior, and a 300 + 1800 frame stress run.
- Added monotonic texture load/unload diagnostics and exact residency/package byte gates.
- Fixed current-back-buffer capture ordering without changing gameplay or input.
- Fixed the supported 800x450 HUD scale and the warning modal's bright center sample/center ornament after original-resolution validation exposed real production defects.
- Replaced stale hard-coded lightning screenshot ROIs with projection/material-frame-derived ROIs and shape-aware connected-contour evidence.
- Added deterministic package references, SHA-256 checks, and the validation handoff.

## Verification

- Platform suite: 473 cases, 0 failures.
- Debug unit selection: 4/4 pass.
- Debug targeted asset/raylib/validator/input selection: 8/8 pass.
- Release raylib/validator selection: 4/4 pass.
- Package unit tests: 23/23 pass; real package generation exit 0 with 410 members.
- Release original-resolution visual matrix: 60/60 pass.
- Stress: resident 226,800,928 bytes; average 119.978 FPS; p99 9.250 ms; 1% low 108.105 FPS; stable load/unload totals after warmup.
- Final worktree clean, Python cache count 0, tracked build/game/test process count 0.

## Artifact

`C:/tmp/arpg-full-material-pack-rebuild/deliverables/arpg-material-pack-v3-native-backgrounds-runtime-integration-20260726.zip`

Size: 505,527,298 bytes

SHA-256: `058f22fe5dac735dc968e01016a4670808591eacabbb6b8e6fd402ec20260a61`

## Review package

`review-d55585e..db1271d.diff`
