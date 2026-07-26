# Material runtime integration repair validation

Validated on 2026-07-26 in the isolated worktree
`E:/game/.worktrees/material-runtime-integration` at base commit `d55585e`.
The resulting Task 10 commit uses the required message
`test: validate material runtime integration`.

## Commands and exit codes

All build and test commands ran serially. MSVC builds were launched from the
Visual Studio 2022 Build Tools developer environment.

| Command | Result |
| --- | --- |
| `cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_stage12_material_formal arpg_stage17_skill_stones_game_validation arpg_game --parallel 1` | exit 0 |
| `cmake --build --preset windows-msvc-debug --target arpg_skills_tests arpg_combat_tests arpg_dungeon_tests --parallel 1` | exit 0; these executables are required by the next CTest selection |
| Debug unit CTest selection from Task 10 | exit 0; 4/4 passed |
| Debug targeted Stage 12/17 asset, real-raylib, validator and input-source CTest selection from Task 10 | 8/8 passed, 0 failed, 0 not run |
| `./scripts/Configure.ps1 -Preset windows-msvc-release` | exit 0 |
| `cmake --build --preset windows-msvc-release --target arpg_game arpg_stage12_material_formal arpg_stage17_skill_stones_game_validation --parallel 1` | exit 0; 172/172 build steps |
| Release real-raylib CTest selection from Task 10 | exit 0; 4/4 passed |

Exact CTest selections:

```powershell
ctest --test-dir out/build/windows-msvc-debug -C Debug -R '^(platform\.units|combat\.units|skills\.units|dungeon\.units)$' --output-on-failure -j 1
ctest --test-dir out/build/windows-msvc-debug -C Debug -R '^(stage12\.(environment_prop_asset_pipeline|material_formal|material_evidence_validator)|stage17\.(active_skill_material_asset_pipeline|skill_stones\.(real_raylib|evidence_validator))|platform\.(input_latency_source|host_input_source))$' --output-on-failure -j 1
ctest --test-dir out/build/windows-msvc-release -C Release -R '^(stage12\.material_formal|stage12\.material_evidence_validator|stage17\.skill_stones\.real_raylib|stage17\.skill_stones\.evidence_validator)$' --output-on-failure -j 1
```

The final Debug targeted run contained these eight passing tests:

- `stage17.active_skill_material_asset_pipeline`
- `stage12.environment_prop_asset_pipeline`
- `stage17.skill_stones.real_raylib`
- `stage17.skill_stones.evidence_validator`
- `stage12.material_formal` (201.55 s)
- `stage12.material_evidence_validator` (11.16 s)
- `platform.input_latency_source` (65.96 s)
- `platform.host_input_source` (185.91 s)

The final Release real-raylib run passed Stage 17 in 10.43 s and its validator
in 1.57 s, then passed Stage 12 in 107.00 s and its validator in 10.29 s.

## Machine-readable evidence

Evidence roots:

- Debug: `out/build/windows-msvc-debug/tests/platform/stage12 material evidence/stage12-run`
- Release: `out/build/windows-msvc-release/tests/platform/stage12 material evidence/stage12-run`

Release metrics from `material-runtime-integration-evidence.txt`:

| Metric | Value |
| --- | ---: |
| Full pack bytes | 329,430,304 |
| Reviewed resident peak bytes | 226,800,928 |
| Observed resident peak bytes | 226,800,928 |
| Transition peak bytes | 261,010,720 |
| Warmup / measured frames | 300 / 1,800 |
| Stable load / unload calls after warmup | 138 / 92 |
| Average FPS | 119.978 |
| Sorted p99 frame time | 9.250 ms |
| 1% low (`1000 / p99_ms`) | 108.105 FPS |
| Captures / capture primes | 64 / 64 |

The resident peak is below the 256 MiB cap. No texture load or unload occurred
after the warmup request stabilized. Shutdown ended with 138 loads and 138
unloads. The fallback fixture recorded 28 loads and one unload.

Formal HUD evidence reports `hud_viewport_safe=pass` and resolution mask `7`
for 800x450, 1280x720 and 1920x1080. The production-projection/material-frame
ROIs are `lightning_shooter=413,414,104,104` and
`lightning_dasher=531,414,104,104`; the validator proves distinct in-bounds
connected contours without fixed legacy coordinates.

## Packaging and deployed artifact

`python -m unittest tests.platform.package_material_pack_v2_tests` passed
23/23 tests in 1.241 s, and `python -m py_compile` passed for both the packager
and its test module. The first real packaging attempt exited 1 because the
isolated worktree did not contain the two immutable legacy ZIP inputs. Their
copies in `C:/tmp/arpg-full-material-pack-rebuild/deliverables` were verified
against the packager's locked SHA-256 values before being copied into the
isolated worktree:

- v1: `b61c54be14e99fc083d4ea5d665ec24d899e565c835e077926ce46cd86d0f2a4`
- v2: `3c1c47de0e20fc3c4e97806cd2719ff44357563abf3d8c5d9b4bebfe301e6d1c`

The second `python tools/package_material_pack_v2.py` run exited 0, verified
410 archive members and produced SHA-256
`058f22fe5dac735dc968e01016a4670808591eacabbb6b8e6fd402ec20260a61`.
The verified deployed artifact is:

`C:/tmp/arpg-full-material-pack-rebuild/deliverables/arpg-material-pack-v3-native-backgrounds-runtime-integration-20260726.zip`

Its size is 505,527,298 bytes. After copy-and-hash verification, all three
large ZIP inputs/outputs were removed from this worktree and are not committed.

## Original-resolution screenshot review

All 60 Release PNGs in the following matrix were opened individually at their
original resolution; no row relies on a representative image or on automated
pixel checks as a substitute for visual review.

| Release capture set | 800x450 | 1280x720 | 1920x1080 |
| --- | --- | --- | --- |
| `integration-cross-ecology-*` | pass | pass | pass |
| `integration-doors-closed-*` | pass | pass | pass |
| `integration-doors-open-*` | pass | pass | pass |
| `integration-draw-slash-t{0,45,46,66,89}-*` | pass 5/5 | pass 5/5 | pass 5/5 |
| `integration-storm-swords-t{0,71,72,180,323,324,342,359}-*` | pass 8/8 | pass 8/8 | pass 8/8 |
| `integration-environment-water-*` | pass | pass | pass |
| `integration-environment-lightning-*` | pass | pass | pass |
| `integration-environment-chaos-*` | pass | pass | pass |
| `integration-death-*` | pass | pass | pass |

Cross ecology shows the fire room and chaos chaser. All four directional doors
are distinct, open/closed states differ, and no red placeholder or `?` appears.
Both skill timelines visibly change across their selected phases. Each ecology
shows five authored props without clipping into the HUD. The repaired death
modal has a dark blue-gray fill, no center ornament, and no title/body/prompt
overlap. The 800x450 HUD is wholly inside the screen.

Non-blocking observations: the 800x450 top and upper-right HUD are necessarily
dense, and the death recap intentionally has substantial empty space. No text
is clipped or overlapped. The validator independently rejects a bright-gray
death interior over 20 percent of the sampled panel.

## Remaining art gaps

These are known content-depth gaps, not runtime-integration failures:

- fire monsters still need full authored action boards for every pose;
- Storm Swords still needs 96 independent authored character frames.

Before packaging, the tracked-process snapshot contained zero `arpg_game`,
formal validator, CTest, CMake, compiler, linker or Ninja processes. After
packaging, artifact deployment and visual review, both the tracked-process
snapshot and the workspace-command-line helper snapshot again reported zero.
