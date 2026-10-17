# Task 1 Report: Keyed Subject Extraction and Element Tinting

Status: DONE

## Changed files

- `tools/build_environment_props.py`
- `tests/platform/environment_prop_asset_pipeline_tests.py`

## RED

Command:

```powershell
python -m unittest tests.platform.environment_prop_asset_pipeline_tests.EnvironmentPropAssetPipelineTests.test_keyed_sources_produce_shared_isolated_element_variants -v
```

Result: expected `ERROR`; `AttributeError: module 'environment_prop_builder' has no attribute '_keyed_subject'`; ran 1 test in 0.003s.

## GREEN

Command:

```powershell
python -m unittest tests.platform.environment_prop_asset_pipeline_tests.EnvironmentPropAssetPipelineTests.test_keyed_sources_produce_shared_isolated_element_variants -v
```

Result: `OK`; ran 1 test in 3.623s.

Full asset suite command:

```powershell
python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v
```

Result: `OK`; ran 8 tests in 23.763s (the prior 7-test baseline plus this task's new test).

Self-review command:

```powershell
git diff --check
```

Result: exit 0; no whitespace errors. The implementation uses the exact key-distance thresholds, eight-connected threshold-8 largest component, 2px Gaussian blur, 8px bbox padding, specified palettes, and preserves alpha exactly across all four color variants. No atlas, JSON, C++, gameplay, or main-worktree files were changed.

Commit: `b02e50bb47b7ea25e6804d3f686a2a37344b7798` (`feat: add keyed environment subject pipeline`)

Concerns: None. Git emitted existing line-ending conversion warnings while staging; `git diff --check` passed and the committed files are valid.
