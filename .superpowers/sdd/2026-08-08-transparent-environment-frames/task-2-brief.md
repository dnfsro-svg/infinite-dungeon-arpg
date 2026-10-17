### Task 2: 重建公共门与生态洞口输出

**Files:**
- Modify: `tools/build_environment_props.py:196-369`
- Modify: `tests/platform/environment_prop_asset_pipeline_tests.py:72-248`
- Modify: `assets/stage12/element_doors.png`
- Modify: `assets/stage12/element_doors_material.png`
- Modify: `assets/stage12/water_environment.png`
- Modify: `assets/stage12/water_environment_material.png`
- Modify: `assets/stage12/lightning_environment.png`
- Modify: `assets/stage12/lightning_environment_material.png`
- Modify: `assets/stage12/chaos_environment.png`
- Modify: `assets/stage12/chaos_environment_material.png`
- Modify: `assets/stage12/environment-props-build.json`

**Interfaces:**
- Consumes: Task 1 `_keyed_subject` / `_element_variant`，`_fit_to_cell`，`_material_map`，现有 `LAYOUT` / `ECOLOGIES`。
- Produces: 现有尺寸的 `element_doors*.png`、三张生态 atlas/material map，以及 schema 2 的 `environment-props-build.json`。

- [ ] **Step 1: Write failing public-output contracts**

  新增 `assert_isolated_silhouette(cell, margin)`：获取 alpha bbox，计算 `opaque(alpha>8) / bbox_area`，要求 `<0.88`；要求 bbox 四边安全边距不小于 margin；统计 bbox 内 alpha>8 覆盖超过 95% 宽/高的行列，各自不得超过 bbox 高/宽的 25%。将它应用到四个门 cell 与水/雷/混沌 `hole=(0,512)` cell。同时把 manifest 契约改为：

  ```python
  self.assertEqual(manifest["schema_version"], 2)
  self.assertEqual(manifest["sources"], {
      "door-concept-v1.png": sha256(ROOT / "art_source/stage12/door-concept-v1.png"),
      "abyss-hole-concept-v1.png": sha256(ROOT / "art_source/stage12/abyss-hole-concept-v1.png"),
  })
  ```

  将旧的“从生态 atlas bootstrap 门”用例改为“修改生态门 cell 不改变键色源生成结果”；将“无效旧 report hash 必须失败”改为“旧 report 不能冻结已发布错误帧，builder 必须从键色源重生成”。

- [ ] **Step 2: Run the asset suite to verify RED**

  Run: `python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v`

  Expected: public door/hole isolation and schema 2/source provenance assertions fail on the current published assets; atomic-publication tests remain green.

- [ ] **Step 3: Wire keyed variants into the existing atlas layout**

  - Remove `_canonical_element_doors`; `build_element_doors` always reads `door-concept-v1.png`, generates four variants in order `fire, water, lightning, chaos`, and fits each with `_fit_to_cell(subject, 12, 244)`.
  - In `build_ecology_environment`, build the keyed hole subject once from `abyss-hole-concept-v1.png`, use `_element_variant(..., ecology)` for `object_sources["hole"]`, and bypass `_retain_subject` only for `wall` and the already-isolated `hole`; fit the hole into the unchanged `(0,512,256,256)` cell.
  - Keep all non-hole ecology props and the opaque authored wall path unchanged.
  - Set report `schema_version` to `2` and add exact SHA-256 source provenance under `sources`; update `_validate_staged_outputs` to require schema 2 and exact source keys/hashes.
  - Do not add runtime selectors, shader branches, new texture files, or manifest entries.

- [ ] **Step 4: Regenerate assets atomically and run GREEN**

  Run:

  ```powershell
  python tools/build_environment_props.py --root E:\game
  python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v
  python tools/build_environment_props.py --root E:\game
  python -m unittest tests.platform.environment_prop_asset_pipeline_tests.EnvironmentPropAssetPipelineTests.test_builder_repeat_generation_is_byte_for_byte_stable -v
  ```

  Expected: full asset suite `OK`; repeat generation leaves every PNG/JSON SHA-256 unchanged; forced staged-validation test proves destination files remain unchanged on failure.

- [ ] **Step 5: Inspect generated frames before commit**

  Open `assets/stage12/element_doors.png` and each ecology atlas. Confirm no visible rectangular wall/floor plate in the four door cells or three hole cells, all subjects fit within their cells, and no alpha halo is clipped. Run `git diff --check` on the Python/JSON files.

- [ ] **Step 6: Commit only pipeline and generated outputs**

  ```powershell
  git add -- tools/build_environment_props.py tests/platform/environment_prop_asset_pipeline_tests.py assets/stage12/element_doors.png assets/stage12/element_doors_material.png assets/stage12/water_environment.png assets/stage12/water_environment_material.png assets/stage12/lightning_environment.png assets/stage12/lightning_environment_material.png assets/stage12/chaos_environment.png assets/stage12/chaos_environment_material.png assets/stage12/environment-props-build.json
  git commit -m "fix: rebuild transparent environment frames"
  ```

