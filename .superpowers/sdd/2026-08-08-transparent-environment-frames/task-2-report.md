# CPLAY-019 Task 2 report

## Status

DONE. Public shared-door and ecology-hole cells are rebuilt from the Task 1 keyed sources, with schema 2 provenance, atomically generated outputs, and stable repeat hashes.

## Changed and committed files

- `tools/build_environment_props.py`
- `tests/platform/environment_prop_asset_pipeline_tests.py`
- `assets/stage12/element_doors.png`
- `assets/stage12/element_doors_material.png`
- `assets/stage12/water_environment.png`
- `assets/stage12/water_environment_material.png`
- `assets/stage12/lightning_environment.png`
- `assets/stage12/lightning_environment_material.png`
- `assets/stage12/chaos_environment.png`
- `assets/stage12/chaos_environment_material.png`
- `assets/stage12/environment-props-build.json`

## TDD evidence

RED command:

```powershell
python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v
```

Observed expected failures: `test_ecology_door_cells_do_not_change_keyed_source_output` (atlas mutation changed doors), schema 1 instead of 2, `test_old_report_cannot_freeze_published_incorrect_frames` (old report blocked rebuild), and `test_public_element_doors_are_isolated_unique_material_cells` (opaque coverage `0.9738562091503268` was not below `0.88`). Atomic staged-validation test remained green.

GREEN generation command (the required root was redirected to the isolated worktree to preserve the main worktree):

```powershell
python tools/build_environment_props.py --root E:\game\.worktrees\cplay019-transparent-environment
```

GREEN suite command and result:

```powershell
python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v
```

`Ran 8 tests in 51.598s` / `OK`.

Required independent repeat test:

```powershell
python -m unittest tests.platform.environment_prop_asset_pipeline_tests.EnvironmentPropAssetPipelineTests.test_builder_repeat_generation_is_byte_for_byte_stable -v
```

`Ran 1 test in 21.132s` / `OK`.

## Generation hash evidence

The SHA-256 values were identical before the first manual regeneration, after it, and after a second regeneration (`FirstStable=True`, `RepeatStable=True`):

| File | SHA-256 |
| --- | --- |
| `element_doors.png` | `6e6fcbdfb30bf169edfb44ca907a9914563bf2ea11f01f79154d2cfce0371cd4` |
| `element_doors_material.png` | `807e8de8241f9297574766dd28554c93ebb97bbc5d1b3b51ad269407a5ca4217` |
| `water_environment.png` | `a08bf7a0aacde2e0d6ae9dcfde36fee070c5f775d845b2a9da0182e6998d9581` |
| `water_environment_material.png` | `71a1b09b0e8351ac70a369955a6218b102bf2ac3a53d7f49a07127f325b6a304` |
| `lightning_environment.png` | `75d83cddc542b13e97c04703104bc945cc0e8eaa4913aa7f9ac7e79a488a64ec` |
| `lightning_environment_material.png` | `579a4da9f8225e7d0c1e9aeeeb0fff80d5f22ef0bf09e5cf5c0ced626542cd12` |
| `chaos_environment.png` | `b6cad8f31fd967616b9956bcb757746473573638bf3d8a5d7804fb1e490f3f15` |
| `chaos_environment_material.png` | `310b8067d7562ae3173441de152c99e9923195ebcce3914b52f6d8e7f20a6ae6` |
| `environment-props-build.json` | `225bb8bbb8d2c77efa4e780747e0b1ab82563a89f5ee52c96e355e74d1f28ba3` |

## Manual image inspection

Opened and checked `assets/stage12/element_doors.png`, `water_environment.png`, `lightning_environment.png`, and `chaos_environment.png`. All four door cells and all three `(0,512)` hole cells have transparent surroundings, remain inside their safe margins, show no rectangular wall/floor plate, and show no clipped alpha halo.

## Self-review

- `git diff --check -- tools/build_environment_props.py tests/platform/environment_prop_asset_pipeline_tests.py assets/stage12/environment-props-build.json` passed.
- The complete suite includes the forced staged-validation failure contract, which stayed green and verifies destination files remain unchanged on validation failure.
- Commit contains exactly the 11 brief-listed pipeline/generated files; no C++, manifest, gameplay, or main-worktree files were changed.

## Commit

`b44cfd7a6e46063debfc526238b558d2270337ad` (`fix: rebuild transparent environment frames`)

## Concerns

None. The brief's `--root E:\game` generation command was intentionally run with the isolated worktree root, as directed, so the main worktree remained untouched.

## Review fix round 1/5

Strengthened `test_old_report_cannot_freeze_published_incorrect_frames` so its temporary published root now saves the correct keyed-source `element_doors.png` bytes/SHA-256, replaces both public door images with distinguishable opaque error frames, and updates the old report's RGBA hash to the erroneous color frame's real hash. After the real builder runs, it asserts that the color PNG bytes and SHA-256 return to the saved keyed-source values, error pixel `(1, 2, 3, 255)` is absent at `(0, 0)`, and color/material alpha match. The test continues to assert schema/source provenance.

Sensitivity RED used a temporary, uncommitted canonical-reuse mutation in `build_element_doors`, then ran:

```powershell
python -m unittest tests.platform.environment_prop_asset_pipeline_tests.EnvironmentPropAssetPipelineTests.test_old_report_cannot_freeze_published_incorrect_frames -v
```

Result: `FAIL` in 8.797s at `self.assertEqual(color_path.read_bytes(), expected_color_bytes)`, proving a builder that reuses the self-consistent-but-wrong public door frame is caught. The production mutation was immediately removed and is not in the commit.

GREEN single-test command:

```powershell
python -m unittest tests.platform.environment_prop_asset_pipeline_tests.EnvironmentPropAssetPipelineTests.test_old_report_cannot_freeze_published_incorrect_frames -v
```

Result: `Ran 1 test in 10.728s` / `OK`.

GREEN full permitted asset-suite command:

```powershell
python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v
```

Result: `Ran 8 tests in 50.906s` / `OK`.

`git diff --check -- tests/platform/environment_prop_asset_pipeline_tests.py` passed before commit. Only `tests/platform/environment_prop_asset_pipeline_tests.py` was committed in review fix `6b32ad41a0ff0bb6fd7d0e611f300feb7e8308ed` (`test: cover corrupt published door frames`); production code and generated PNG/JSON were unchanged.
