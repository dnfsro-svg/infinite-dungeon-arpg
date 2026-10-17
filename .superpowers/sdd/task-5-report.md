# Task 5 report: Stage 12-D effects, loot icons, and formal validation

## Delivered

- Added `effects_ui.png` (1024x1024 RGBA, 4 MiB), its original source copy, reproducible chroma-key atlas builder, and provenance record.
- Added material IDs, manifest frames, element-effect and rarity-icon selectors. Fire/water/lightning/chaos projectiles, hit sparks, launcher trail, landing dust, affix aura, and white/blue/gold/abyss ground-loot icons draw from the effects atlas, with the prior procedural drawing preserved as fallback.
- Added `--screenshot-dir <directory>` parsing and `RaylibHostConfig::screenshot_directory`. F12 retains the legacy default filename/location, but formal or concurrent callers can now isolate that filename in a caller-owned directory.
- Added an ordinary-gameplay scheduled validation capture seam: it is active only when every prior stage scenario is `none`, uses `validation_capture_file`, and does not alter player input, combat, dungeon, persistence, door, hole, or loot rules. The formal run also invokes the existing Stage10 input/hole formal executable and copies its actual descent summary into the evidence directory.
- Added formal Raylib validation at 1280x720 and 1920x1080, with an actual corrupted `effects_ui.png` run proving the game stays running via material fallback. It additionally creates a caller-owned F12 capture directory and a real eight-monster renderer showcase: the showcase modifies only a transient presented snapshot, never the runtime snapshot/AI. The PowerShell validator rejects absent/wrong-size PNGs, missing isolated F12/showcase/input-hole evidence, over-budget atlases, or missing fallback records.
- Hardened formal evidence cleanup: the runner accepts only the known CTest build evidence parent and deletes only its fixed `stage12-run` child. The new root-safety negative test proves project/workspace/temp-parent candidates are rejected without deleting a sentinel.

## Test-first record

- Added `stage12_material_render_tests.cpp` before implementation for manifest budget, rarity-icon distinction, and effects-atlas category routing. The first red build was blocked before compilation by the inherited worktree build lock and missing VS environment (`Windows SDK detected ''`); after entering the project-required VS 2022/SDK 10.0.26100.0 shell, the complete test set was run green.

## Verification

- Debug `platform.units`: PASS, 304 cases, 0 failures.
- Debug `stage12`: initial formal/validator PASS 2/2; after the root-safety hardening, the current `platform.units|stage12` run PASS 4/4, including actual F12 output at `f12-monsters-1280x720.png/stage8-equipment-loot.png`, an eight-monster renderer screenshot, a corrupted-effects fallback screenshot, and Stage10 descent evidence (`depth=2`, `last_transition=1`, `resolution_valid=1`).
- Debug after root-safety hardening: `platform.units|stage12` PASS, 4/4 (includes `stage12.material_root_safety`).
- Release filtered `platform.units|stage12`: PASS, 3/3.
- Formal evidence report records `manifest=pass`, `atlas_bytes=25165824`, `fallback=pass`, `input_hole_regression=pass`, `monsters=8`, `screenshot_isolation=pass`, `result=pass`.
- `git diff --check`: no whitespace errors; only pre-existing CRLF conversion notices.

## Full-suite note

- Full Debug script could not relink because the user-visible Debug game was running: `LNK1168` on `out/build/windows-msvc-debug/bin/arpg_game.exe` (PID 256564, parent already exited). It was not terminated.
- Full Release script reached test 21/81, then failed an existing Stage11 death screenshot comparison: `Normal/restarted death panel hashes differ` in `tests/dungeon/stage11_death_formal_capture_test.ps1:95`. The Stage12 changes are not on that scenario path; targeted Release Stage12 and platform tests passed.
